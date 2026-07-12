#pragma once

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <utility>

enum class PeMachine {
    X86,
    X64,
    Other
};

enum class PeInspectionErrorCode {
    FileNotFound,
    FileUnreadable,
    MalformedImage
};

struct PeInspectionError {
    PeInspectionErrorCode code;
    std::string message;
    std::filesystem::path path;
};

struct PeImageInfo {
    PeMachine machine = PeMachine::Other;
    std::set<std::string> exports;
};

template <typename T>
class PeInspectionResult {
public:
    static PeInspectionResult Success(T value) {
        return PeInspectionResult(std::move(value));
    }

    static PeInspectionResult Failure(PeInspectionError error) {
        return PeInspectionResult(std::move(error));
    }

    bool has_value() const noexcept { return value_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }
    const T& value() const { return value_.value(); }
    const PeInspectionError& error() const { return error_.value(); }

private:
    explicit PeInspectionResult(T value) : value_(std::move(value)) {}
    explicit PeInspectionResult(PeInspectionError error)
        : error_(std::move(error)) {}

    std::optional<T> value_;
    std::optional<PeInspectionError> error_;
};

class PeExportInspector {
public:
    static PeInspectionResult<PeImageInfo> Inspect(
        const std::filesystem::path& path);
};
