#pragma once

#include "dbp/package/PackageFormat.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dbp::package {

inline constexpr std::size_t kRuntimeDescriptorSize = 96U;

enum class RuntimeMode : std::uint32_t {
    Application = 0,
    Installer = 1,
};

struct RuntimeDescriptor {
    RuntimeMode mode = RuntimeMode::Application;
    PackageId packageId{};
    KeyId keyId{};
    std::string packageFileName;
};

std::string ExpectedPackageFileName(const PackageId& packageId);

PackageResult<std::vector<std::uint8_t>> SerializeRuntimeDescriptor(
    const RuntimeDescriptor& descriptor);
PackageResult<RuntimeDescriptor> ParseRuntimeDescriptor(
    const std::vector<std::uint8_t>& bytes);

PackageResult<bool> WriteRuntimeDescriptorAtomically(
    const std::filesystem::path& descriptorPath,
    const RuntimeDescriptor& descriptor);
PackageResult<RuntimeDescriptor> ReadRuntimeDescriptor(
    const std::filesystem::path& descriptorPath);

} // namespace dbp::package
