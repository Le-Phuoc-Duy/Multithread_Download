#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include "cli/ArgumentParser.h"
#include "core/DownloadController.h"
using namespace std;

namespace {
volatile std::sig_atomic_t gStopRequested = 0;

void handleSignal(int) {
    gStopRequested = 1;
}
}

int main(int argc, char* argv[]) {
    DownloadConfig config;
    ArgumentParser parser;

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    if (!parser.parse(argc, argv, config))
        return 1;

    DownloadController controller(config, &gStopRequested);
    controller.start();

    return 0;
}
