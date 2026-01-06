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

    // Decide numbers of thread
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
    if (!fileWriter->open())
        return false;

    segmentQueue = std::make_unique<SegmentQueue>(metadata.segments);
    threadPool = std::make_unique<ThreadPool>(stopFlag);

    spawnWorkers();

    const auto startTime = std::chrono::steady_clock::now();
    auto lastProgressLog = startTime;

    // Log progress
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

    stop();
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

    logger.stop();
}

bool DownloadController::initMetadata() {
    HttpClient client(cfg.url);
    HttpHeadResult head{};

    if (!client.head(head))
        return false;
    supportsRange = head.acceptRanges;

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

    return true;
}

void DownloadController::spawnWorkers() {
    auto workerFn = [this]() {

        DownloadWorker worker(
            *segmentQueue,
            *fileWriter,
            *connectionPool,
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

