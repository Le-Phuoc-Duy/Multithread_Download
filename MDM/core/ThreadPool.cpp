#include "ThreadPool.h"

ThreadPool::ThreadPool(std::atomic<bool>& stopFlag)
    : shouldStop(stopFlag) {
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::start(std::size_t n, WorkerFn worker) {
    std::lock_guard<std::mutex> lock(mtx);

    for (std::size_t i = 0; i < n; ++i) {
        threads.emplace_back(worker);
    }
}

void ThreadPool::scaleUp(std::size_t n, WorkerFn worker) {
    start(n, worker);
}

void ThreadPool::scaleDown(std::size_t n) {
    // scaleDown mềm: báo stop, join dần
    std::lock_guard<std::mutex> lock(mtx);

    std::size_t removeCount = std::min(n, threads.size());
    shouldStop.store(true);

    for (std::size_t i = 0; i < removeCount; ++i) {
        if (threads.back().joinable())
            threads.back().join();
        threads.pop_back();
    }

    shouldStop.store(false);
}

void ThreadPool::shutdown() {
    std::lock_guard<std::mutex> lock(mtx);

    shouldStop.store(true);

    for (auto& t : threads) {
        if (t.joinable())
            t.join();
    }

    threads.clear();
}

std::size_t ThreadPool::size() const {
    std::lock_guard<std::mutex> lock(mtx);
    return threads.size();
}
