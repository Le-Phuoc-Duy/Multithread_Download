#pragma once
#include <string>
#include <functional>
#include <cstdint>

struct HttpHeadResult {
    std::uint64_t contentLength = 0;
    std::string etag;
    bool acceptRanges = false;
};

class HttpClient {
public:
    explicit HttpClient(const std::string& url);
    ~HttpClient();

    bool head(HttpHeadResult& out);
    bool getRange(std::uint64_t offset,
        std::uint64_t size,
        const std::function<bool(const char*, std::size_t)>& onData);

private:
    void* curl;
    std::string url;
};
