#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>

class ThreadPool {
public:
    using WorkerFn = std::function<void()>;

    explicit ThreadPool(std::atomic<bool>& stopFlag);
    ~ThreadPool();

    void start(std::size_t n, WorkerFn worker);
    void scaleUp(std::size_t n, WorkerFn worker);
    void scaleDown(std::size_t n);
    void shutdown();

    std::size_t size() const;

private:
    std::vector<std::thread> threads;
    std::atomic<bool>& shouldStop;
    mutable std::mutex mtx;
};
