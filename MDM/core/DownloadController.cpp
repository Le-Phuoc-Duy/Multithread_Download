#include "DownloadController.h"

#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

DownloadController::DownloadController(const DownloadConfig& config, volatile std::sig_atomic_t* externalStop)
    : cfg(config),
    externalStopSignal(externalStop),
    progress(0)
{
}

bool DownloadController::start() {
    logger.start();
    if (!initMetadata())
        return false;

    progress.reset(metadata.fileSize);

    // Chọn số thread: ưu tiên giá trị người dùng, nếu 0 => auto theo CPU và server
    workerCount = cfg.maxThreads;
    if (workerCount == 0) {
        std::size_t hw = std::thread::hardware_concurrency();
        if (hw == 0) hw = 4; // fallback
        workerCount = std::clamp<std::size_t>(hw * 2, 2, 32);
    }

    if (!supportsRange) {
        workerCount = 1;
    }

    if (!metadata.segments.empty())
        workerCount = std::min<std::size_t>(workerCount, metadata.segments.size());
    else
        workerCount = 1;

    connectionPool = std::make_unique<ConnectionPool>(cfg.url, workerCount);

    fileWriter = std::make_unique<FileWriter>(cfg.outputPath, metadata.fileSize);
    if (!fileWriter->open(cfg.resume))
        return false;

    segmentQueue = std::make_unique<SegmentQueue>(metadata.segments);
    threadPool = std::make_unique<ThreadPool>(stopFlag);

    spawnWorkers();

    const auto startTime = std::chrono::steady_clock::now();
    auto lastProgressLog = startTime;
    lastMetadataSave = startTime;

    // Chờ tải xong mọi segment, log tiến trình định kỳ
    while (!stopFlag.load(std::memory_order_relaxed)) {
        if (externalStopSignal && *externalStopSignal != 0)
            stopFlag.store(true, std::memory_order_relaxed);

        if (allSegmentsDone())
            break;

        auto now = std::chrono::steady_clock::now();
        if (now - lastProgressLog >= std::chrono::seconds(1)) {
            const auto downloaded = progress.downloaded();
            const double pct = progress.progress() * 100.0;

            std::ostringstream os;
            os << "Progress: "
                << downloaded << "/" << metadata.fileSize << " bytes ("
                << std::fixed << std::setprecision(1) << pct << "%)";

            logger.log(os.str());
            lastProgressLog = now;
        }

        if (now - lastMetadataSave >= std::chrono::seconds(2)) {
            persistMetadataSnapshot();
            lastMetadataSave = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    const auto endTime = std::chrono::steady_clock::now();
    const std::chrono::duration<double> duration = endTime - startTime;
    const double avgSpeed = duration.count() > 0
        ? static_cast<double>(progress.downloaded()) / duration.count()
        : 0.0;

    const bool success = allSegmentsDone();
    {
        std::size_t doneSegments = 0;
        for (const auto& seg : metadata.segments) {
            if (seg.state == SegmentState::Done)
                ++doneSegments;
        }

        std::ostringstream conclusion;
        conclusion << "Download "
            << (success ? "completed" : "stopped")
            << " in " << std::fixed << std::setprecision(2)
            << duration.count() << "s, avg speed "
            << std::setprecision(2) << (avgSpeed * 8.0 / 1'000'000.0)
            << " Mbps, threads " << workerCount
            << ", segments " << doneSegments << "/" << metadata.segments.size();

        if (encounteredError.load(std::memory_order_relaxed)) {
            std::string errCopy;
            {
                std::lock_guard<std::mutex> lock(errorMutex);
                errCopy = lastError;
            }
            conclusion << " (had errors";
            if (!errCopy.empty())
                conclusion << ": " << errCopy;
            conclusion << ")";
        }
        else if (!success) {
            conclusion << " (unknown error)";
        }

        logger.log(conclusion.str());
    }

    stop(); // join thread, lưu meta, đóng file
    return allSegmentsDone();
}

bool DownloadController::allSegmentsDone() const {
    for (const auto& seg : metadata.segments) {
        if (seg.state != SegmentState::Done)
            return false;
    }
    return true;
}


void DownloadController::stop() {
    stopFlag.store(true);
    if (threadPool)
        threadPool->shutdown();

    if (fileWriter)
        fileWriter->close();

    if (metadataStore)
        metadataStore->save(buildMetadataSnapshot());
    
    logger.stop();
}

bool DownloadController::initMetadata() {
    metadataStore = std::make_unique<MetadataStore>(cfg.outputPath + ".meta");

    HttpClient client(cfg.url);
    HttpHeadResult head{};

    if (!client.head(head))
        return false;
    supportsRange = head.acceptRanges;

    if (cfg.resume && metadataStore->exists()) {
        if (!metadataStore->load(metadata))
            return false;

        if (!metadataStore->validate(metadata, head.etag, head.contentLength)) {
            if (metadata.fileSize != head.contentLength) {
                logger.log("Resume validation failed: file size changed (local " +
                    std::to_string(metadata.fileSize) + ", remote " +
                    std::to_string(head.contentLength) + "). Delete .meta to restart.");
            }
            else if (!metadata.etag.empty() && metadata.etag != head.etag) {
                logger.log("Resume validation failed: remote ETag changed (local " +
                    metadata.etag + ", remote " + head.etag + "). Delete .meta to restart.");
            }
            else {
                logger.log("Resume validation failed: metadata mismatch. Delete .meta to restart.");
            }
            return false;
        }

        // Nếu có segment đang InProgress (từ lần ngắt trước), gán lại Pending để có thể tải tiếp
        for (auto& seg : metadata.segments) {
            if (seg.state == SegmentState::InProgress)
                seg.state = SegmentState::Pending;
        }

        return true;
    }

    // Fresh download
    metadata.url = cfg.url;
    metadata.etag = head.etag;
    metadata.fileSize = head.contentLength;
    metadata.completedBytes = 0;

    metadata.segments.clear();

    std::uint64_t offset = 0;
    std::uint64_t index = 0;

    while (offset < metadata.fileSize) {
        std::uint64_t size = std::min<std::uint64_t>(
            cfg.segmentSize,
            metadata.fileSize - offset
        );

        metadata.segments.push_back({
            index++,
            offset,
            size,
            SegmentState::Pending
            });

        offset += size;
    }

    return metadataStore->save(metadata);
}

void DownloadController::spawnWorkers() {
    auto workerFn = [this]() {

        DownloadWorker worker(
            *segmentQueue,
            *fileWriter,
            *connectionPool,   // ✅ đúng
            [this](const WorkerReport& rep) {
                onWorkerReport(rep);
            },
            stopFlag
        );

        worker.run();
        };

    threadPool->start(workerCount, workerFn);
}


void DownloadController::onWorkerReport(const WorkerReport& report) {
    if (report.success) {
        {
            std::lock_guard<std::mutex> lock(metadataMutex);
            metadata.completedBytes += report.bytesDownloaded;
        }
        progress.add(report.bytesDownloaded);
        //logger.log("Segment " + std::to_string(report.segmentIndex) + " done");
    }
    else {
        encounteredError.store(true, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(errorMutex);
            lastError = report.error;
        }
        //logger.log("Segment " + std::to_string(report.segmentIndex) + " failed: " + report.error);
    }
}

void DownloadController::persistMetadataSnapshot() {
    if (!metadataStore || !segmentQueue)
        return;

    auto snapshot = buildMetadataSnapshot();
    metadataStore->save(snapshot);
}

DownloadMetadata DownloadController::buildMetadataSnapshot() {
    DownloadMetadata snapshot;
    {
        std::lock_guard<std::mutex> lock(metadataMutex);
        snapshot = metadata;
    }

    if (segmentQueue) {
        snapshot.segments = segmentQueue->snapshot();
        // Nếu có segment đang InProgress, đặt lại Pending để resume sẽ tải lại
        for (auto& seg : snapshot.segments) {
            if (seg.state == SegmentState::InProgress)
                seg.state = SegmentState::Pending;
        }
    }

    return snapshot;
}
