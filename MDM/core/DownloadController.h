#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <cstddef>
#include <csignal>
#include <chrono>

#include "utils.h"
#include "SegmentQueue.h"
#include "ThreadPool.h"
#include "DownloadWorker.h"
#include "../io/FileWriter.h"
#include "../net/HttpClient.h"
#include "../monitor/ProgressTracker.h"
#include "../monitor/Logger.h"

class DownloadController {
public:
    explicit DownloadController(const DownloadConfig& config, volatile std::sig_atomic_t* externalStop = nullptr);

    bool start();
    void stop();

private:
    void onWorkerReport(const WorkerReport& report);
    bool initMetadata();
    void spawnWorkers();
    bool allSegmentsDone() const;
private:
    const DownloadConfig& cfg;
    volatile std::sig_atomic_t* externalStopSignal{ nullptr };

    std::atomic<bool> stopFlag{ false };

    std::unique_ptr<FileWriter> fileWriter;
    std::unique_ptr<SegmentQueue> segmentQueue;
    std::unique_ptr<ThreadPool> threadPool;
    std::unique_ptr<ConnectionPool> connectionPool;

    ProgressTracker progress;
    Logger logger;

    DownloadMetadata metadata;
    std::mutex metadataMutex;
    std::atomic<bool> encounteredError{ false };
    std::mutex errorMutex;
    std::string lastError;
    bool supportsRange{ false };
    std::size_t workerCount{ 0 };
};
