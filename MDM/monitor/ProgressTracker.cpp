#include "ProgressTracker.h"

ProgressTracker::ProgressTracker(std::uint64_t totalBytes)
    : total(totalBytes),
    start(std::chrono::steady_clock::now()) {
}

void ProgressTracker::add(std::uint64_t bytes) {
    current.fetch_add(bytes, std::memory_order_relaxed);
}

std::uint64_t ProgressTracker::downloaded() const {
    return current.load(std::memory_order_relaxed);
}

double ProgressTracker::progress() const {
    return total == 0 ? 0.0 : (double)downloaded() / (double)total;
}

double ProgressTracker::speedBytesPerSec() const {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - start;
    return elapsed.count() > 0 ? downloaded() / elapsed.count() : 0.0;
}

void ProgressTracker::reset(std::uint64_t totalBytes) {
    total = totalBytes;
    current.store(0, std::memory_order_relaxed);
    start = std::chrono::steady_clock::now();
}

