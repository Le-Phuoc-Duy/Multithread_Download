#include "DownloadController.h"

#include <thread>

DownloadController::DownloadController(const DownloadConfig& config)
    : cfg(config),
    progress(0)
{
}

bool DownloadController::start() {
    logger.start();
    connectionPool = std::make_unique<ConnectionPool>(cfg.url, cfg.maxThreads);

    if (!initMetadata())
        return false;

    progress.reset(metadata.fileSize);

    fileWriter = std::make_unique<FileWriter>(cfg.outputPath, metadata.fileSize);
    if (!fileWriter->open(cfg.resume))
        return false;

    segmentQueue = std::make_unique<SegmentQueue>(metadata.segments);
    threadPool = std::make_unique<ThreadPool>(stopFlag);

    spawnWorkers();

    // Chờ tải xong mọi segment
    while (!stopFlag.load(std::memory_order_relaxed)) {
        if (allSegmentsDone())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
        metadataStore->save(metadata);
    
    logger.stop();
}

bool DownloadController::initMetadata() {
    metadataStore = std::make_unique<MetadataStore>(cfg.outputPath + ".meta");

    HttpClient client(cfg.url);
    HttpHeadResult head{};

    if (!client.head(head))
        return false;

    if (cfg.resume && metadataStore->exists()) {
        if (!metadataStore->load(metadata))
            return false;

        if (!metadataStore->validate(metadata, head.etag, head.contentLength))
            return false;

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

    threadPool->start(cfg.maxThreads, workerFn);
}


void DownloadController::onWorkerReport(const WorkerReport& report) {
    if (report.success) {
        metadata.completedBytes += report.bytesDownloaded;
        progress.add(report.bytesDownloaded);
        logger.log("Segment " + std::to_string(report.segmentIndex) + " done");
    }

    // MVP: persist periodically or at shutdown
}
