#pragma once
#include <atomic>
#include <functional>

#include "utils.h"
#include "SegmentQueue.h"
#include "ConnectionPool.h"
#include "../io/FileWriter.h"
#include "../net/HttpClient.h"

class DownloadWorker {
public:
    using ReportCallback = std::function<void(const WorkerReport&)>;

    DownloadWorker(SegmentQueue& queue,
        FileWriter& writer,
        ConnectionPool& pool,
        ReportCallback cb,
        std::atomic<bool>& stopFlag);

    void run();

private:
    SegmentQueue& segmentQueue;
    FileWriter& fileWriter;
    ConnectionPool& connectionPool;
    ReportCallback report;
    std::atomic<bool>& shouldStop;
};

