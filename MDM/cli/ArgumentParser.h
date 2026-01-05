#pragma once
#include <string>
#include "../core/utils.h"

class ArgumentParser {
public:
    bool parse(int argc, char* argv[], DownloadConfig& out);

private:
    void printUsage() const;
};
