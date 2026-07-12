#pragma once

#include "SourceAssembler.h"

#include <cstddef>
#include <filesystem>
#include <vector>

class CompilationInput {
public:
    static SourceAssemblyResult<CompilationInput> FromSourceFile(
        const std::filesystem::path& path);
    static SourceAssemblyResult<CompilationInput> FromProject(
        const ProjectManifest& manifest,
        SourceAssemblyOptions options);

    const std::vector<std::byte>& bytes() const noexcept { return bytes_; }
    const std::filesystem::path& baseDirectory() const noexcept {
        return baseDirectory_;
    }
    const std::vector<SourceMapEntry>& sourceMap() const noexcept {
        return sourceMap_;
    }

private:
    CompilationInput(
        std::vector<std::byte> bytes,
        std::filesystem::path baseDirectory,
        std::vector<SourceMapEntry> sourceMap);

    std::vector<std::byte> bytes_;
    std::filesystem::path baseDirectory_;
    std::vector<SourceMapEntry> sourceMap_;
};
