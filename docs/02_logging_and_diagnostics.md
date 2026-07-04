# Phase 2: Logging and Diagnostics

## 🎯 Goal
Integrate a centralized, fast, and structured logging system for all operations in the compiler and runner. This replaces obsolete print statements and dialog boxes, enabling developers to diagnose compilation failures and execution crashes quickly and precisely.

---

## 🛠️ Recommended Library: spdlog
We recommend **spdlog** because it is:
* Extremely fast and won't bottleneck the compiler.
* Supports automatic file logging with rotation to prevent log size bloat.
* Provides colorized console logging for clean CLI diagnostics.
* Header-only, making it extremely easy to integrate into the CMake build system.

---

## 💻 Proposed Logger Architecture

We can define a simple logger wrapper in a shared header like `DBPLogger.h`:

```cpp
#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

class DBPLogger {
public:
    static void Initialize(const std::string& logFilePath) {
        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath, true);

            std::vector<spdlog::sink_ptr> sinks { console_sink, file_sink };
            auto logger = std::make_shared<spdlog::logger>("dbp_compiler", sinks.begin(), sinks.end());
            
            spdlog::set_default_logger(logger);
            spdlog::set_level(spdlog::level::trace); // Trace all steps
            spdlog::flush_on(spdlog::level::err);     // Flush on errors
        }
        catch (const spdlog::spdlog_ex& ex) {
            // Handle logger initialization failure
        }
    }
};

// Macros for easy integration with old codebases
#define DBP_TRACE(...)    spdlog::trace(__VA_ARGS__)
#define DBP_INFO(...)     spdlog::info(__VA_ARGS__)
#define DBP_WARN(...)     spdlog::warn(__VA_ARGS__)
#define DBP_ERROR(...)    spdlog::error(__VA_ARGS__)
```

---

## 🔄 Proposed Integration Points

1. **Program Entry (`Main.cpp`)**:
   * Call `DBPLogger::Initialize("dbp_compiler.log");` as the first operation in `main()`.
2. **Variable Table (`VarTable.cpp`)**:
   * Trace variable registration details:
     ```cpp
     DBP_TRACE("Registered variable: name={0}, type={1}, offset={2}", pVarName, dwType, dwOffset);
     ```
3. **Machine Code Generator (`ASMWriter.cpp`)**:
   * Trace instruction generation and relocation patching:
     ```cpp
     DBP_TRACE("Generated instruction: {0}, operand={1}", pDebugStr, lpOpData);
     ```
4. **Error Reporting (`Error.cpp`)**:
   * Write all syntax and runtime error diagnostics to the log file automatically.

---

## 🚀 Benefits
* **Remote Debugging**: Easily resolve compiler crashes on user machines by requesting the generated `dbp_compiler.log` file.
* **Architecture Visibility**: Trace logs give new developers a clear view of how compiler states change during the build process.
