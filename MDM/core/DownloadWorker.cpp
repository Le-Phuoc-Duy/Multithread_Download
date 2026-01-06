#include "DownloadWorker.h"

DownloadWorker::DownloadWorker(SegmentQueue& queue,
    FileWriter& writer,
    ConnectionPool& pool,
    ReportCallback cb,
    std::atomic<bool>& stopFlag)
    : segmentQueue(queue),
    fileWriter(writer),
    connectionPool(pool),
    report(std::move(cb)),
    shouldStop(stopFlag) {
}


void DownloadWorker::run() {
    while (!shouldStop.load(std::memory_order_relaxed)) {

        auto segOpt = segmentQueue.getNext();
        if (!segOpt.has_value())
            return;

        const Segment seg = *segOpt;

        WorkerReport rep{};
        rep.segmentIndex = seg.index;
        rep.bytesDownloaded = 0;
        rep.success = false;

        // Get connection
        auto client = connectionPool.acquire();

        bool ok = client->getRange(
            seg.offset,
            seg.size,
            [&](const char* data, std::size_t size) {
                if (!fileWriter.write(seg.offset + rep.bytesDownloaded, data, size))
                    return false;

                rep.bytesDownloaded += size;
                return true;
            });

        connectionPool.release(std::move(client));

        if (ok) {
            segmentQueue.markDone(seg.index);
            rep.success = true;
        }
        else {
            rep.error = "download failed";
        }

        report(rep);
    }
}
