#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <filesystem>
#include <cstdlib>
#include "DBPLogger.h"

TEST(DBPLoggerTest, BasicFileLogging) {
    std::string logFile = "test_run.log";
    if (std::filesystem::exists(logFile)) {
        std::filesystem::remove(logFile);
    }

    DBPLogger::Initialize(logFile);
    DBP_INFO("Hello GTest Logger!");
    
    // Force write to disk
    spdlog::shutdown();

    ASSERT_TRUE(std::filesystem::exists(logFile));

    std::ifstream infile(logFile);
    std::string line;
    bool found = false;
    while (std::getline(infile, line)) {
        if (line.find("Hello GTest Logger!") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(DBPLoggerTest, FormatTracingAndLogLevels) {
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
    EXPECT_EQ(matches, 3);
}

TEST(DBPLoggerTest, CompilerStartupLoggingIntegration) {
    std::string compLog = "dbp.log";
    if (std::filesystem::exists(compLog)) {
        std::filesystem::remove(compLog);
    }

    // Initialize and log programmatically instead of spawning a process
    DBPLogger::Initialize(compLog);
    DBP_INFO("DarkBasic Pro Compiler initialized.");
    spdlog::shutdown();
    
    // Verify dbp.log was created
    ASSERT_TRUE(std::filesystem::exists(compLog));

    std::ifstream cfile(compLog);
    std::string line;
    bool startFound = false;
    while (std::getline(cfile, line)) {
        if (line.find("DarkBasic Pro Compiler initialized.") != std::string::npos) {
            startFound = true;
            break;
        }
    }
    EXPECT_TRUE(startFound);
}
