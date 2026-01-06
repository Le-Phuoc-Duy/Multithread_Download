#include "ConnectionPool.h"

ConnectionPool::ConnectionPool(const std::string& u, std::size_t maxSize)
    : url(u), maxPoolSize(maxSize) {
}

std::unique_ptr<HttpClient> ConnectionPool::acquire() {
    std::lock_guard<std::mutex> lock(mtx);

    if (!pool.empty()) {
        auto client = std::move(pool.front());
        pool.pop();
        return client;
    }

    return std::make_unique<HttpClient>(url);
}

void ConnectionPool::release(std::unique_ptr<HttpClient> client) {
    if (!client)
        return;

    std::lock_guard<std::mutex> lock(mtx);

    if (pool.size() < maxPoolSize) {
        pool.push(std::move(client));
    }
}
