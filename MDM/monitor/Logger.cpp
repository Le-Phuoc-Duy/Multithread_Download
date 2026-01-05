#include "Logger.h"
#include <iostream>

Logger::Logger() {}

Logger::~Logger() {
    stop();
}

void Logger::start() {
    running.store(true);
    worker = std::thread(&Logger::run, this);
}

void Logger::stop() {
    running.store(false);
    cv.notify_all();

    if (worker.joinable())
        worker.join();
}

void Logger::log(const std::string& msg) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        messages.push(msg);
    }
    cv.notify_one();
}

void Logger::run() {
    while (running.load() || !messages.empty()) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]() {
            return !messages.empty() || !running.load();
            });

        while (!messages.empty()) {
            std::cout << messages.front() << std::endl;
            messages.pop();
        }
    }
}
