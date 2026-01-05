#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>

class Logger {
public:
    Logger();
    ~Logger();

    void start();
    void stop();
    void log(const std::string& msg);

private:
    void run();

private:
    std::queue<std::string> messages;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> running{ false };
    std::thread worker;
};
