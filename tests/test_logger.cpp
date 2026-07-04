#include <cassert>
#include <fstream>
#include <string>
#include <filesystem>
#include "DBPLogger.h" // This header does not exist yet!

int main() {
    std::string logFile = "test_run.log";
    if (std::filesystem::exists(logFile)) {
        std::filesystem::remove(logFile);
    }

    DBPLogger::Initialize(logFile);
    DBP_INFO("Hello TDD Logger!");
    
    // Force flush spdlog to disk
    spdlog::shutdown();

    // Assert log file was created
    assert(std::filesystem::exists(logFile));

    // Assert file contains logged contents
    std::ifstream infile(logFile);
    std::string line;
    bool found = false;
    while (std::getline(infile, line)) {
        if (line.find("Hello TDD Logger!") != std::string::npos) {
            found = true;
            break;
        }
    }
    assert(found);
    
    return 0; // Success
}
