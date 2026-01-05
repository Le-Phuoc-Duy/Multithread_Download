#pragma once
#include <atomic>
#include <memory>

#include "utils.h"
#include "SegmentQueue.h"
#include "ThreadPool.h"
#include "DownloadWorker.h"
#include "../io/FileWriter.h"
#include "../io/MetadataStore.h"
#include "../net/HttpClient.h"
#include "../monitor/ProgressTracker.h"
#include "../monitor/Logger.h"

class DownloadController {
public:
    explicit DownloadController(const DownloadConfig& config);

    bool start();
    void stop();

private:
    void onWorkerReport(const WorkerReport& report);
    bool initMetadata();
    void spawnWorkers();
    bool allSegmentsDone() const;
private:
    const DownloadConfig& cfg;

    std::atomic<bool> stopFlag{ false };

    std::unique_ptr<MetadataStore> metadataStore;
    std::unique_ptr<FileWriter> fileWriter;
    std::unique_ptr<SegmentQueue> segmentQueue;
    std::unique_ptr<ThreadPool> threadPool;
    std::unique_ptr<ConnectionPool> connectionPool;

    ProgressTracker progress;
    Logger logger;

    DownloadMetadata metadata;
};
