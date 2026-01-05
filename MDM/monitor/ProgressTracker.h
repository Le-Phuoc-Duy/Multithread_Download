#pragma once
#include <atomic>
#include <cstdint>
#include <chrono>

class ProgressTracker {
public:
    explicit ProgressTracker(std::uint64_t totalBytes);

    void add(std::uint64_t bytes);
    std::uint64_t downloaded() const;
    double progress() const;
    double speedBytesPerSec() const;
    void reset(std::uint64_t totalBytes);

private:
    std::uint64_t total;
    std::atomic<std::uint64_t> current{ 0 };
    std::chrono::steady_clock::time_point start;
};
