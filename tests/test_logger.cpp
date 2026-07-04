#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <cstdlib>
#include "DBPLogger.h"

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed: " << #expr << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

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
    ASSERT(std::filesystem::exists(logFile));

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
    ASSERT(found);
    
    // -------------------------------------------------------------
    // Test 2: DBPCompiler startup log integration test (TDD)
    // -------------------------------------------------------------
    std::string compLog = "dbp.log";
    if (std::filesystem::exists(compLog)) {
        std::filesystem::remove(compLog);
    }

    // Execute compiler (in the same directory)
    int ret = std::system("DBPCompiler.exe");
    
    // Verify dbp.log was created
    ASSERT(std::filesystem::exists(compLog));

    std::ifstream cfile(compLog);
    bool startFound = false;
    while (std::getline(cfile, line)) {
        if (line.find("DarkBasic Pro Compiler initialized.") != std::string::npos) {
            startFound = true;
            break;
        }
    }
    ASSERT(startFound);

    // -------------------------------------------------------------
    // Test 3: Format tracing and log levels (TDD)
    // -------------------------------------------------------------
    std::string testLog3 = "test_format.log";
    if (std::filesystem::exists(testLog3)) {
        std::filesystem::remove(testLog3);
    }
    DBPLogger::Initialize(testLog3);
    DBP_TRACE("Trace variable: {} = {}", "myVar", 100);
    DBP_WARN("Warning test: code={}", 404);
    DBP_ERROR("Error test: msg={}", "critical failure");
    spdlog::shutdown();

    std::ifstream infile3(testLog3);
    std::string line3;
    int matches = 0;
    while (std::getline(infile3, line3)) {
        if (line3.find("Trace variable: myVar = 100") != std::string::npos) matches++;
        if (line3.find("Warning test: code=404") != std::string::npos) matches++;
        if (line3.find("Error test: msg=critical failure") != std::string::npos) matches++;
    }
    ASSERT(matches == 3);

    return 0; // Success
}
