#include "SourceAssembler.h"

#include <fstream>
#include <limits>

namespace {

SourceAssemblyError MakeError(
    SourceAssemblyErrorCode code,
    const ProjectSourceEntry& source,
    std::string message) {
    return {code, std::move(message), source.resolvedPath, source.manifestKey};
}

SourceAssemblyResult<std::vector<std::byte>> ReadSource(
    const ProjectSourceEntry& source,
    std::uint64_t maxBytes) {
    std::error_code error;
    if (!std::filesystem::exists(source.resolvedPath, error) || error) {
        return SourceAssemblyResult<std::vector<std::byte>>::Failure(MakeError(
            SourceAssemblyErrorCode::SourceNotFound,
            source,
            "Source file was not found."));
    }

    const auto size = std::filesystem::file_size(source.resolvedPath, error);
    if (error) {
        return SourceAssemblyResult<std::vector<std::byte>>::Failure(MakeError(
            SourceAssemblyErrorCode::SourceUnreadable,
            source,
            "Source file size could not be read."));
    }
    if (size > maxBytes || size > std::numeric_limits<std::size_t>::max()) {
        return SourceAssemblyResult<std::vector<std::byte>>::Failure(MakeError(
            SourceAssemblyErrorCode::SourceTooLarge,
            source,
            "Source file exceeds the configured combined-source limit."));
    }

    std::ifstream stream(source.resolvedPath, std::ios::binary);
    if (!stream) {
        return SourceAssemblyResult<std::vector<std::byte>>::Failure(MakeError(
            SourceAssemblyErrorCode::SourceUnreadable,
            source,
            "Source file could not be opened."));
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty()) {
        stream.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if (!stream || static_cast<std::size_t>(stream.gcount()) != bytes.size()) {
            return SourceAssemblyResult<std::vector<std::byte>>::Failure(MakeError(
                SourceAssemblyErrorCode::SourceUnreadable,
                source,
                "Source file could not be read completely."));
        }
    }
    return SourceAssemblyResult<std::vector<std::byte>>::Success(std::move(bytes));
}

std::uint64_t CountNewlines(const std::vector<std::byte>& bytes) {
    std::uint64_t count = 0;
    for (const auto value : bytes) {
        if (value == std::byte{'\n'}) {
            ++count;
        }
    }
    return count;
}

} // namespace

SourceAssemblyResult<AssembledSource> SourceAssembler::Assemble(
    const ProjectManifest& manifest,
    SourceAssemblyOptions options) {
    AssembledSource assembled;
    std::uint64_t currentLine = 1;

    for (const auto& source : manifest.sources) {
        const auto remaining = options.maxBytes >= assembled.bytes.size()
            ? options.maxBytes - assembled.bytes.size()
            : 0;
        auto sourceResult = ReadSource(source, remaining);
        if (!sourceResult) {
            return SourceAssemblyResult<AssembledSource>::Failure(sourceResult.error());
        }

        auto sourceBytes = std::move(sourceResult.value());
        const bool needsBoundary = !assembled.bytes.empty() &&
            assembled.bytes.back() != std::byte{'\n'};
        const std::uint64_t required = sourceBytes.size() + (needsBoundary ? 2ull : 0ull);
        if (required > options.maxBytes - assembled.bytes.size()) {
            return SourceAssemblyResult<AssembledSource>::Failure(MakeError(
                SourceAssemblyErrorCode::SourceTooLarge,
                source,
                "Combined source exceeds the configured size limit."));
        }

        if (needsBoundary) {
            assembled.bytes.push_back(std::byte{'\r'});
            assembled.bytes.push_back(std::byte{'\n'});
            ++currentLine;
        }

        assembled.sourceMap.push_back({
            source.resolvedPath,
            source.manifestKey,
            assembled.bytes.size(),
            currentLine});
        currentLine += CountNewlines(sourceBytes);
        assembled.bytes.insert(
            assembled.bytes.end(), sourceBytes.begin(), sourceBytes.end());
    }

    return SourceAssemblyResult<AssembledSource>::Success(std::move(assembled));
}
