#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct CompilerArguments {
    bool json = false;
    bool trace = false;
    bool debug = false;
    bool help = false;
    bool emitFinalSource = false;
    bool legacyFinalSource = false;
    std::optional<std::filesystem::path> runtimeRoot;
    std::optional<std::filesystem::path> outputPath;
    std::optional<std::filesystem::path> packageKeyFile;
    std::filesystem::path inputPath;
};

class CompilerArgumentsResult {
public:
    static CompilerArgumentsResult Success(CompilerArguments value) {
        return CompilerArgumentsResult(std::move(value));
    }
    static CompilerArgumentsResult Failure(std::string error) {
        return CompilerArgumentsResult(std::move(error));
    }
    bool has_value() const noexcept { return value_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }
    const CompilerArguments& value() const { return value_.value(); }
    const std::string& error() const { return error_.value(); }

private:
    explicit CompilerArgumentsResult(CompilerArguments value)
        : value_(std::move(value)) {}
    explicit CompilerArgumentsResult(std::string error)
        : error_(std::move(error)) {}
    std::optional<CompilerArguments> value_;
    std::optional<std::string> error_;
};

CompilerArgumentsResult ParseCompilerArguments(
    const std::vector<std::string>& arguments);
CompilerArgumentsResult ParseWideCompilerArguments(
    const std::vector<std::wstring>& arguments);
