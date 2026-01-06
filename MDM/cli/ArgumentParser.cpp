#include "ArgumentParser.h"
#include <iostream>
#include <cstdlib>

namespace {
std::string deriveOutputFromUrl(const std::string& url) {
    // Get file name from url
    auto slash = url.find_last_of('/');
    std::string name = (slash == std::string::npos) ? url : url.substr(slash + 1);

    auto special = name.find_first_of("?#");
    if (special != std::string::npos)
        name = name.substr(0, special);

    if (name.empty())
        name = "download";

    return name;
}
}

bool ArgumentParser::parse(int argc, char* argv[], DownloadConfig& out) {
    if (argc < 2) {
        printUsage();
        return false;
    }

    out.url.clear();
    out.outputPath.clear();
    out.maxThreads = 0; // 0 = auto select threads
    out.segmentSize = 1 * 1024 * 1024;

    out.url = argv[1];

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-o" && i + 1 < argc) {
            out.outputPath = argv[++i];
        }
        else if (arg == "-t" && i + 1 < argc) {
            out.maxThreads = std::stoul(argv[++i]);
        }
        else if (arg == "-s" && i + 1 < argc) {
            out.segmentSize = std::stoull(argv[++i]);
        }
        else {
            printUsage();
            return false;
        }
    }

    if (out.outputPath.empty())
        out.outputPath = deriveOutputFromUrl(out.url);

    if (out.segmentSize == 0) {
        return false;
    }

    return true;
}

void ArgumentParser::printUsage() const {
    std::cout <<
        "Usage:\n"
        "  mdm <url> [-o <output>] [options]\n\n"
        "Options:\n"
        "  -o <file>        Output file path (default: name from url)\n"
        "  -t <threads>     Max threads (default: auto)\n"
        "  -s <bytes>       Segment size (default: 1MB)\n";
}
