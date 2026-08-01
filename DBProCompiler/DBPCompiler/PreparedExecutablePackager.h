#pragma once

#include "FileBuilder.h"
#include "RuntimeBundleResolver.h"

#include <filesystem>

struct StandalonePackagingRequest {
    std::filesystem::path outputPath;
};

class IStandalonePackagingServices {
public:
    virtual ~IStandalonePackagingServices() = default;

    [[nodiscard]] virtual bool ResolveRuntimeBundle() noexcept = 0;
    [[nodiscard]] virtual bool StageExecutable(
        const std::filesystem::path& outputPath) noexcept = 0;
    [[nodiscard]] virtual bool CustomizeResources() noexcept = 0;
    [[nodiscard]] virtual bool Publish() noexcept = 0;
};

class PreparedExecutablePackager {
public:
    [[nodiscard]] bool Package(
        const StandalonePackagingRequest& request,
        IStandalonePackagingServices& services) const noexcept;
};

class ASMWriterStandalonePackagingServices final
    : public IStandalonePackagingServices {
public:
    ASMWriterStandalonePackagingServices() = default;

    [[nodiscard]] bool ResolveRuntimeBundle() noexcept override;
    [[nodiscard]] bool StageExecutable(
        const std::filesystem::path& outputPath) noexcept override;
    [[nodiscard]] bool CustomizeResources() noexcept override;
    [[nodiscard]] bool Publish() noexcept override;

private:
    [[nodiscard]] bool AddRuntimeLibraries() noexcept;
    [[nodiscard]] bool AddProjectMedia() noexcept;
    [[nodiscard]] bool AddApplicationAssets() noexcept;
    [[nodiscard]] bool AddEffects() noexcept;
    [[nodiscard]] bool AddResourceMetadata() noexcept;

    CFileBuilder builder_;
    const ResolvedRuntimeBundle* runtimeBundle_{nullptr};
    std::filesystem::path outputPath_;
    std::filesystem::path mediaRoot_;
    std::filesystem::path replacementIcon_;
    bool effectsRequired_{false};
};
