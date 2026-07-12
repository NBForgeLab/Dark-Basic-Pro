#include "ProjectManifest.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>

namespace {

std::string Trim(std::string value) {
    const auto isNotSpace = [](unsigned char character) {
        return std::isspace(character) == 0;
    };
    const auto first = std::find_if(value.begin(), value.end(), isNotSpace);
    if (first == value.end()) {
        return {};
    }
    const auto last = std::find_if(value.rbegin(), value.rend(), isNotSpace).base();
    return std::string(first, last);
}

std::string FoldKey(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::filesystem::path ResolvePath(
    const std::filesystem::path& directory,
    const std::filesystem::path& declared) {
    if (declared.is_absolute()) {
        return declared.lexically_normal();
    }
    return (directory / declared).lexically_normal();
}

ProjectError Error(
    ProjectErrorCode code,
    const std::filesystem::path& path,
    std::string message,
    std::string key = {}) {
    return {code, std::move(message), path, std::move(key)};
}

} // namespace

ProjectResult<ProjectManifest> ProjectManifestReader::Read(
    const std::filesystem::path& requestedPath) {
    std::error_code filesystemError;
    const auto projectPath = std::filesystem::absolute(requestedPath, filesystemError)
        .lexically_normal();
    if (filesystemError || !std::filesystem::exists(projectPath)) {
        return ProjectResult<ProjectManifest>::Failure(Error(
            ProjectErrorCode::FileNotFound,
            requestedPath,
            "Project file was not found."));
    }

    std::ifstream stream(projectPath, std::ios::binary);
    if (!stream) {
        return ProjectResult<ProjectManifest>::Failure(Error(
            ProjectErrorCode::FileUnreadable,
            projectPath,
            "Project file could not be opened."));
    }

    std::optional<std::filesystem::path> mainSource;
    std::optional<std::filesystem::path> finalSource;
    std::optional<std::filesystem::path> executable;
    std::map<unsigned long, std::filesystem::path> includes;
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto trimmedLine = Trim(line);
        if (trimmedLine.empty() || trimmedLine.front() == ';') {
            continue;
        }
        const auto equals = trimmedLine.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const auto originalKey = Trim(trimmedLine.substr(0, equals));
        const auto key = FoldKey(originalKey);
        const auto value = Trim(trimmedLine.substr(equals + 1));
        if (key == "main") {
            if (!value.empty()) {
                mainSource = std::filesystem::path(value);
            }
            continue;
        }
        if (key == "final source") {
            if (!value.empty()) {
                finalSource = std::filesystem::path(value);
            }
            continue;
        }
        if (key == "executable") {
            if (!value.empty()) {
                executable = std::filesystem::path(value);
            }
            continue;
        }
        if (key.rfind("include", 0) != 0) {
            continue;
        }

        const auto suffix = key.substr(7);
        if (suffix.empty() || !std::all_of(suffix.begin(), suffix.end(), [](unsigned char c) {
                return std::isdigit(c) != 0;
            })) {
            return ProjectResult<ProjectManifest>::Failure(Error(
                ProjectErrorCode::MalformedIncludeKey,
                projectPath,
                "Include key must end in a positive numeric index.",
                originalKey));
        }

        unsigned long index = 0;
        try {
            index = std::stoul(suffix);
        } catch (const std::exception&) {
            return ProjectResult<ProjectManifest>::Failure(Error(
                ProjectErrorCode::MalformedIncludeKey,
                projectPath,
                "Include index is outside the supported range.",
                originalKey));
        }
        if (index == 0 || value.empty()) {
            return ProjectResult<ProjectManifest>::Failure(Error(
                ProjectErrorCode::MalformedIncludeKey,
                projectPath,
                "Include key requires a positive index and a source path.",
                originalKey));
        }
        if (!includes.emplace(index, std::filesystem::path(value)).second) {
            return ProjectResult<ProjectManifest>::Failure(Error(
                ProjectErrorCode::DuplicateIncludeIndex,
                projectPath,
                "Include index is declared more than once.",
                originalKey));
        }
    }

    if (!mainSource.has_value()) {
        return ProjectResult<ProjectManifest>::Failure(Error(
            ProjectErrorCode::MissingMain,
            projectPath,
            "Project requires a non-empty main source."));
    }

    unsigned long expectedIndex = 1;
    for (const auto& include : includes) {
        if (include.first != expectedIndex) {
            return ProjectResult<ProjectManifest>::Failure(Error(
                ProjectErrorCode::NonContiguousIncludes,
                projectPath,
                "Include indices must be contiguous starting at include1.",
                "include" + std::to_string(expectedIndex)));
        }
        ++expectedIndex;
    }

    ProjectManifest manifest;
    manifest.projectPath = projectPath;
    manifest.projectDirectory = projectPath.parent_path();
    manifest.sources.push_back({
        "main", *mainSource, ResolvePath(manifest.projectDirectory, *mainSource)});
    for (const auto& include : includes) {
        const auto key = "include" + std::to_string(include.first);
        manifest.sources.push_back({
            key, include.second, ResolvePath(manifest.projectDirectory, include.second)});
    }
    if (finalSource.has_value()) {
        manifest.finalSourcePath = ResolvePath(manifest.projectDirectory, *finalSource);
    }
    if (executable.has_value()) {
        manifest.executablePath = ResolvePath(manifest.projectDirectory, *executable);
    }
    return ProjectResult<ProjectManifest>::Success(std::move(manifest));
}
