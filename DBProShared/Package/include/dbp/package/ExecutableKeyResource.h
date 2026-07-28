#pragma once

#include "dbp/package/KeyProvider.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace dbp::package {

inline constexpr std::size_t kExecutableKeyResourceSize = 64U;
inline constexpr wchar_t kExecutableKeyResourceName[] =
    L"DBP_PACKAGE_KEY_V2";

struct ExecutablePackageKey {
    KeyId keyId{};
    SecureBuffer masterKey;
};

PackageResult<std::vector<std::uint8_t>>
SerializeExecutableKeyResource(
    const KeyId& keyId,
    const SecureBuffer& masterKey);
PackageResult<ExecutablePackageKey> ParseExecutableKeyResource(
    const std::vector<std::uint8_t>& bytes,
    const KeyId& expectedKeyId);

PackageResult<bool> InjectExecutablePackageKey(
    const std::filesystem::path& executablePath,
    const KeyId& keyId,
    const SecureBuffer& masterKey);
PackageResult<ExecutablePackageKey> ReadExecutablePackageKey(
    const std::filesystem::path& executablePath,
    const KeyId& expectedKeyId);
PackageResult<ExecutablePackageKey> ReadExecutablePackageKeyFromModule(
    void* moduleHandle,
    const KeyId& expectedKeyId);

} // namespace dbp::package
