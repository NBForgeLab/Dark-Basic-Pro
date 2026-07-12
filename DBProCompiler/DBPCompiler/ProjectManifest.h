#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

enum class ProjectErrorCode {
    FileNotFound,
    FileUnreadable,
    MissingMain,
    DuplicateMain,
    MalformedIncludeKey,
    DuplicateIncludeIndex,
    NonContiguousIncludes
};

struct ProjectError {
    ProjectErrorCode code;
    std::string message;
    std::filesystem::path projectPath;
    std::string manifestKey;
};

template <typename T>
class ProjectResult {
public:
    static ProjectResult Success(T value) {
        return ProjectResult(std::move(value));
    }

    static ProjectResult Failure(ProjectError error) {
        return ProjectResult(std::move(error));
    }

    bool has_value() const noexcept { return value_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }

    const T& value() const { return value_.value(); }
    T& value() { return value_.value(); }
    const ProjectError& error() const { return error_.value(); }

private:
    explicit ProjectResult(T value) : value_(std::move(value)) {}
    explicit ProjectResult(ProjectError error) : error_(std::move(error)) {}

    std::optional<T> value_;
    std::optional<ProjectError> error_;
};

struct ProjectSourceEntry {
    std::string manifestKey;
    std::filesystem::path declaredPath;
    std::filesystem::path resolvedPath;
};

struct ProjectManifest {
    std::filesystem::path projectPath;
    std::filesystem::path projectDirectory;
    std::vector<ProjectSourceEntry> sources;
    std::optional<std::filesystem::path> finalSourcePath;
    std::optional<std::filesystem::path> executablePath;
};

class ProjectManifestReader {
public:
    static ProjectResult<ProjectManifest> Read(const std::filesystem::path& path);
};
