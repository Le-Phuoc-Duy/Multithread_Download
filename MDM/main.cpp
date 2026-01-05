#include <iostream>
#include <chrono>
#include <thread>
#include "cli/ArgumentParser.h"
#include "core/DownloadController.h"
using namespace std;

int main(int argc, char* argv[]) {
    DownloadConfig config;
    ArgumentParser parser;

    if (!parser.parse(argc, argv, config))
        return 1;

    DownloadController controller(config);
    controller.start();

    return 0;
}
