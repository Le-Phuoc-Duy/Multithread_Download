#pragma once
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>

#include "../net/HttpClient.h"

class ConnectionPool {
public:
    ConnectionPool(const std::string& url, std::size_t maxSize);

    std::unique_ptr<HttpClient> acquire();
    void release(std::unique_ptr<HttpClient> client);

private:
    std::size_t maxPoolSize;
    std::string url;
    std::queue<std::unique_ptr<HttpClient>> pool;
    std::mutex mtx;
};
