#pragma once

#include "ProjectManifest.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

enum class SourceAssemblyErrorCode {
    ProjectFileNotFound,
    ProjectFileUnreadable,
    ProjectMissingMain,
    ProjectMalformedInclude,
    ProjectDuplicateMain,
    ProjectDuplicateInclude,
    ProjectNonContiguousIncludes,
    SourceNotFound,
    SourceUnreadable,
    SourceTooLarge,
    ArtifactWriteFailed
};

const char* SourceAssemblyDiagnosticCode(SourceAssemblyErrorCode code) noexcept;

struct SourceAssemblyError {
    SourceAssemblyErrorCode code;
    std::string message;
    std::filesystem::path sourcePath;
    std::string manifestKey;
};

template <typename T>
class SourceAssemblyResult {
public:
    static SourceAssemblyResult Success(T value) {
        return SourceAssemblyResult(std::move(value));
    }
    static SourceAssemblyResult Failure(SourceAssemblyError error) {
        return SourceAssemblyResult(std::move(error));
    }

    bool has_value() const noexcept { return value_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }
    const T& value() const { return value_.value(); }
    T& value() { return value_.value(); }
    const SourceAssemblyError& error() const { return error_.value(); }

private:
    explicit SourceAssemblyResult(T value) : value_(std::move(value)) {}
    explicit SourceAssemblyResult(SourceAssemblyError error)
        : error_(std::move(error)) {}

    std::optional<T> value_;
    std::optional<SourceAssemblyError> error_;
};

struct SourceAssemblyOptions {
    std::uint64_t maxBytes = 256ull * 1024ull * 1024ull;
};

struct SourceMapEntry {
    std::filesystem::path path;
    std::string manifestKey;
    std::uint64_t combinedByteStart = 0;
    std::uint64_t combinedLineStart = 1;
};

struct AssembledSource {
    std::vector<std::byte> bytes;
    std::vector<SourceMapEntry> sourceMap;
};

class SourceAssembler {
public:
    static SourceAssemblyResult<AssembledSource> Assemble(
        const ProjectManifest& manifest,
        SourceAssemblyOptions options);
};

class FinalSourceArtifactWriter {
public:
    static SourceAssemblyResult<bool> WriteAtomically(
        const std::filesystem::path& destination,
        const std::vector<std::byte>& bytes);
};
