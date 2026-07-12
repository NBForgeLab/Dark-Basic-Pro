#pragma once

#include "RuntimeContract.h"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

enum class RuntimeBundleClassification {
    Versioned,
    LegacyUnversioned
};

enum class RuntimeErrorCode {
    MissingRoot,
    MissingComponent,
    InvalidComponent,
    IncompatibleArchitecture,
    MissingCapability,
    PathEscapesRoot
};

struct RuntimeError {
    RuntimeErrorCode code;
    std::string message;
    std::filesystem::path runtimeRoot;
    std::filesystem::path componentPath;
    std::optional<RuntimeCapability> capability;
};

template <typename T>
class RuntimeResult {
public:
    static RuntimeResult Success(T value) {
        return RuntimeResult(std::move(value));
    }

    static RuntimeResult Failure(RuntimeError error) {
        return RuntimeResult(std::move(error));
    }

    bool has_value() const noexcept { return value_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }
    const T& value() const { return value_.value(); }
    const RuntimeError& error() const { return error_.value(); }

private:
    explicit RuntimeResult(T value) : value_(std::move(value)) {}
    explicit RuntimeResult(RuntimeError error) : error_(std::move(error)) {}

    std::optional<T> value_;
    std::optional<RuntimeError> error_;
};

struct RuntimeSelection {
    std::optional<std::filesystem::path> explicitRoot;
    std::filesystem::path compilerDirectory;
};

struct ResolvedRuntimeBundle {
    std::filesystem::path root;
    std::filesystem::path pluginsDirectory;
    std::filesystem::path userPluginsDirectory;
    std::filesystem::path licensedPluginsDirectory;
    std::filesystem::path effectsDirectory;
    std::filesystem::path corePath;
    RuntimeCapabilities capabilities;
    RuntimeBundleClassification classification =
        RuntimeBundleClassification::LegacyUnversioned;
};

class RuntimeBundleResolver {
public:
    static RuntimeResult<ResolvedRuntimeBundle> Resolve(
        const RuntimeSelection& selection,
        const ProgramRuntimeRequirements& requirements);
};
