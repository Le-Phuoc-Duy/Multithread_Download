#include "HttpClient.h"

#include <curl/curl.h>
#include <sstream>

static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* cb = static_cast<std::function<bool(const char*, std::size_t)>*>(userdata);
    std::size_t total = size * nmemb;
    if (!(*cb)(ptr, total))
        return 0;
    return total;
}

static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    std::size_t total = size * nitems;
    auto* result = static_cast<HttpHeadResult*>(userdata);

    std::string header(buffer, total);

    if (header.rfind("Content-Length:", 0) == 0) {
        result->contentLength = std::stoull(header.substr(15));
    }
    else if (header.rfind("ETag:", 0) == 0) {
        result->etag = header.substr(5);
    }
    else if (header.rfind("Accept-Ranges:", 0) == 0) {
        if (header.find("bytes") != std::string::npos)
            result->acceptRanges = true;
    }

    return total;
}

HttpClient::HttpClient(const std::string& u)
    : url(u) {
    curl = curl_easy_init();
}


HttpClient::~HttpClient() {
    if (curl)
        curl_easy_cleanup(static_cast<CURL*>(curl));
}

bool HttpClient::head(HttpHeadResult& out) {
    CURL* c = static_cast<CURL*>(curl);
    if (!c)
        return false;

    //curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_reset(c);

    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(c, CURLOPT_HEADERDATA, &out);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);

    return curl_easy_perform(c) == CURLE_OK;
}

bool HttpClient::getRange(std::uint64_t offset,
    std::uint64_t size,
    const std::function<bool(const char*, std::size_t)>& onData) {
    CURL* c = static_cast<CURL*>(curl);
    if (!c)
        return false;

    curl_easy_reset(c);

    std::ostringstream range;
    range << offset << "-" << (offset + size - 1);

    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_RANGE, range.str().c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, (void*)&onData);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(c);
    if (res != CURLE_OK)
        return false;

    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);

    // 🔑 DÒNG QUYẾT ĐỊNH
    return status == 206;
}

