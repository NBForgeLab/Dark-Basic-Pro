#include "CompilationInput.h"

#include <utility>

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
        return SourceAssemblyResult<CompilationInput>::Failure({
            SourceAssemblyErrorCode::SourceUnreadable,
            manifestResult.error().message,
            manifestResult.error().projectPath,
            manifestResult.error().manifestKey});
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
