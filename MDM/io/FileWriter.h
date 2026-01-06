#pragma once
#include <string>
#include <cstdint>
#include <mutex>

class FileWriter
{
public:
    FileWriter(const std::string& path, std::uint64_t fileSize);

    bool open();
    bool write(std::uint64_t offset, const char* data, std::size_t size);
    void flush();
    void close();

private:
    std::string filePath;
    std::uint64_t totalSize;

    std::mutex writeMutex;
    int fileHandle = -1;
};

