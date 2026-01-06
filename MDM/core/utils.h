#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

struct DownloadConfig {
    std::string url;
    std::string outputPath;

    std::size_t segmentSize;
    std::size_t maxThreads;

    bool resume;
};

enum class SegmentState {
    Pending,
    InProgress,
    Done
};

struct Segment {
    std::uint64_t index;
    std::uint64_t offset;
    std::uint64_t size;
    SegmentState state;
};

struct DownloadMetadata {
    std::string url;
    std::string etag;

    std::uint64_t fileSize;
    std::uint64_t completedBytes;

    std::vector<Segment> segments;
};

struct WorkerReport {
    std::uint64_t segmentIndex;
    std::uint64_t bytesDownloaded;
    bool success;
    std::string error;
};
