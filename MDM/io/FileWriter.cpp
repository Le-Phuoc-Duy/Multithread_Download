#include "FileWriter.h"

#include <filesystem>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

namespace fs = std::filesystem;

FileWriter::FileWriter(const std::string& path, std::uint64_t fileSize)
    : filePath(path), totalSize(fileSize) {
}

bool FileWriter::open(bool resume) {
#ifdef _WIN32
    int flags = _O_BINARY | _O_RDWR | _O_CREAT;
    int mode = _S_IREAD | _S_IWRITE;
#else
    int flags = O_RDWR | O_CREAT;
    int mode = 0644;
#endif

    if (!resume && fs::exists(filePath)) {
        fs::remove(filePath);
    }

#ifdef _WIN32
    fileHandle = _open(filePath.c_str(), flags, mode);
#else
    fileHandle = open(filePath.c_str(), flags, mode);
#endif

    if (fileHandle < 0)
        return false;

    // Pre-allocate file size
#ifdef _WIN32
    _lseeki64(fileHandle, totalSize - 1, SEEK_SET);
    _write(fileHandle, "", 1);
#else
    lseek(fileHandle, totalSize - 1, SEEK_SET);
    write(fileHandle, "", 1);
#endif

    return true;
}

bool FileWriter::write(std::uint64_t offset, const char* data, std::size_t size) {
    if (fileHandle < 0)
        return false;

    std::lock_guard<std::mutex> lock(writeMutex);

#ifdef _WIN32
    _lseeki64(fileHandle, offset, SEEK_SET);
    return _write(fileHandle, data, static_cast<unsigned int>(size)) == static_cast<int>(size);
#else
    lseek(fileHandle, offset, SEEK_SET);
    return ::write(fileHandle, data, size) == static_cast<ssize_t>(size);
#endif
}

void FileWriter::flush() {
#ifdef _WIN32
    _commit(fileHandle);
#else
    fsync(fileHandle);
#endif
}

void FileWriter::close() {
    if (fileHandle >= 0) {
#ifdef _WIN32
        _close(fileHandle);
#else
        close(fileHandle);
#endif
        fileHandle = -1;
    }
}
