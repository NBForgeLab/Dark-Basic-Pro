#include "CompilationInput.h"

#include <utility>

namespace {

SourceAssemblyErrorCode MapProjectError(ProjectErrorCode code) {
    switch (code) {
        case ProjectErrorCode::FileNotFound:
            return SourceAssemblyErrorCode::ProjectFileNotFound;
        case ProjectErrorCode::FileUnreadable:
            return SourceAssemblyErrorCode::ProjectFileUnreadable;
        case ProjectErrorCode::MissingMain:
            return SourceAssemblyErrorCode::ProjectMissingMain;
        case ProjectErrorCode::MalformedIncludeKey:
            return SourceAssemblyErrorCode::ProjectMalformedInclude;
        case ProjectErrorCode::DuplicateMain:
            return SourceAssemblyErrorCode::ProjectDuplicateMain;
        case ProjectErrorCode::DuplicateIncludeIndex:
            return SourceAssemblyErrorCode::ProjectDuplicateInclude;
        case ProjectErrorCode::NonContiguousIncludes:
            return SourceAssemblyErrorCode::ProjectNonContiguousIncludes;
    }
    return SourceAssemblyErrorCode::ProjectFileUnreadable;
}

const char* ProjectDiagnosticCode(ProjectErrorCode code) {
    switch (code) {
        case ProjectErrorCode::MissingMain: return "DBP1001";
        case ProjectErrorCode::NonContiguousIncludes: return "DBP1002";
        default: return "DBP1000";
    }
}

} // namespace

CompilationInput::CompilationInput(
    std::vector<std::byte> bytes,
    std::filesystem::path baseDirectory,
    std::vector<SourceMapEntry> sourceMap)
    : bytes_(std::move(bytes)),
      baseDirectory_(std::move(baseDirectory)),
      sourceMap_(std::move(sourceMap)) {}

SourceAssemblyResult<CompilationInput> CompilationInput::FromProjectFile(
    const std::filesystem::path& path,
    SourceAssemblyOptions options) {
    const auto manifestResult = ProjectManifestReader::Read(path);
    if (!manifestResult) {
        const auto& projectError = manifestResult.error();
        return SourceAssemblyResult<CompilationInput>::Failure({
            MapProjectError(projectError.code),
            std::string(ProjectDiagnosticCode(projectError.code)) + ": " +
                projectError.message,
            projectError.projectPath,
            projectError.manifestKey});
    }
    return FromProject(manifestResult.value(), options);
}

SourceAssemblyResult<CompilationInput> CompilationInput::FromSourceFile(
    const std::filesystem::path& requestedPath) {
    std::error_code error;
    const auto path = std::filesystem::absolute(requestedPath, error).lexically_normal();
    ProjectManifest manifest;
    manifest.projectPath = path;
    manifest.projectDirectory = path.parent_path();
    manifest.sources.push_back({"source", requestedPath, path});
    return FromProject(manifest, {});
}

SourceAssemblyResult<CompilationInput> CompilationInput::FromProject(
    const ProjectManifest& manifest,
    SourceAssemblyOptions options) {
    auto assembledResult = SourceAssembler::Assemble(manifest, options);
    if (!assembledResult) {
        return SourceAssemblyResult<CompilationInput>::Failure(assembledResult.error());
    }
    auto assembled = std::move(assembledResult.value());
    return SourceAssemblyResult<CompilationInput>::Success(CompilationInput(
        std::move(assembled.bytes),
        manifest.projectDirectory,
        std::move(assembled.sourceMap)));
}
