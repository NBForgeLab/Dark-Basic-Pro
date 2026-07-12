#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <vector>
#include <string>

class DBPLogger {
public:
    static void Initialize(const std::string& logFilePath, bool bJsonMode = false) {
        try {
            std::vector<spdlog::sink_ptr> sinks;
            if (bJsonMode) {
                // In JSON mode, route all logger outputs to stderr to keep stdout clean for JSON messages
                auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
                sinks.push_back(console_sink);
            } else {
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                sinks.push_back(console_sink);
            }
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath, true);
            sinks.push_back(file_sink);

            auto logger = std::make_shared<spdlog::logger>("dbp_compiler", sinks.begin(), sinks.end());
            
            spdlog::set_default_logger(logger);
            spdlog::set_level(spdlog::level::trace);
            spdlog::flush_on(spdlog::level::err);
        }
        catch (const spdlog::spdlog_ex& ex) {
            // Logger configuration failure
        }
    }
};

#define DBP_TRACE(...)    spdlog::trace(__VA_ARGS__)
#define DBP_INFO(...)     spdlog::info(__VA_ARGS__)
#define DBP_WARN(...)     spdlog::warn(__VA_ARGS__)
#define DBP_ERROR(...)    spdlog::error(__VA_ARGS__)
