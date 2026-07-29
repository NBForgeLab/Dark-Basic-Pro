#include "dbp/package/ExecutableKeyResource.h"

#include "dbp/package/ByteCodec.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace dbp::package {

namespace {

constexpr std::array<std::uint8_t, 8> resourceMagic{
    'D', 'B', 'P', 'K', 'E', 'Y', '2', 0,
};
constexpr std::uint16_t resourceMajorVersion = 2;
constexpr std::uint16_t resourceMinorVersion = 0;

PackageError ResourceError(
    const PackageErrorCode code,
    std::string message) {
    return {
        code,
        std::move(message),
        std::nullopt,
    };
}

template <typename T>
PackageResult<T> Failure(
    const PackageErrorCode code,
    std::string message) {
    return PackageResult<T>::Failure(
        ResourceError(code, std::move(message)));
}

class LoadedModule {
public:
    explicit LoadedModule(const HMODULE value) noexcept
        : value_(value) {}

    ~LoadedModule() {
        if (value_ != nullptr) {
            FreeLibrary(value_);
        }
    }

    LoadedModule(const LoadedModule&) = delete;
    LoadedModule& operator=(const LoadedModule&) = delete;

    HMODULE get() const noexcept {
        return value_;
    }

private:
    HMODULE value_;
};

struct ResourceLanguages {
    WORD first = 0;
    std::size_t count = 0;
};

class SensitiveBytes {
public:
    explicit SensitiveBytes(std::vector<std::uint8_t>& bytes) noexcept
        : bytes_(bytes) {}

    ~SensitiveBytes() {
        if (!bytes_.empty()) {
            SecureZeroMemory(bytes_.data(), bytes_.size());
        }
    }

    SensitiveBytes(const SensitiveBytes&) = delete;
    SensitiveBytes& operator=(const SensitiveBytes&) = delete;

private:
    std::vector<std::uint8_t>& bytes_;
};

BOOL CALLBACK CollectResourceLanguage(
    HMODULE,
    LPCWSTR,
    LPCWSTR,
    const WORD language,
    const LONG_PTR parameter) {
    auto* languages =
        reinterpret_cast<ResourceLanguages*>(parameter);
    if (languages->count == 0) {
        languages->first = language;
    }
    ++languages->count;
    return languages->count < 2U ? TRUE : FALSE;
}

PackageResult<std::vector<std::uint8_t>> ReadResourceBytes(
    const HMODULE module,
    const wchar_t* const resourceName) {
    ResourceLanguages languages;
    SetLastError(ERROR_SUCCESS);
    const auto enumerated = EnumResourceLanguagesW(
        module,
        MAKEINTRESOURCEW(10),
        resourceName,
        CollectResourceLanguage,
        reinterpret_cast<LONG_PTR>(&languages));
    if (!enumerated && languages.count == 0) {
        const auto error = GetLastError();
        if (error == ERROR_RESOURCE_TYPE_NOT_FOUND ||
            error == ERROR_RESOURCE_NAME_NOT_FOUND ||
            error == ERROR_RESOURCE_DATA_NOT_FOUND) {
            return Failure<std::vector<std::uint8_t>>(
                PackageErrorCode::MissingKey,
                "The executable package-key resource is missing.");
        }
        return Failure<std::vector<std::uint8_t>>(
            PackageErrorCode::IoFailed,
            "Enumerating the executable package-key resource failed.");
    }
    if (languages.count != 1U ||
        languages.first !=
            MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)) {
        return Failure<std::vector<std::uint8_t>>(
            PackageErrorCode::InvalidFormat,
            "The executable contains duplicate or non-neutral "
            "package-key resources.");
    }

    const auto resource = FindResourceExW(
        module,
        MAKEINTRESOURCEW(10),
        resourceName,
        languages.first);
    if (resource == nullptr) {
        return Failure<std::vector<std::uint8_t>>(
            PackageErrorCode::InvalidFormat,
            "The executable package-key resource cannot be resolved.");
    }
    const auto resourceSize = SizeofResource(module, resource);
    if (resourceSize != kExecutableKeyResourceSize) {
        return Failure<std::vector<std::uint8_t>>(
            PackageErrorCode::InvalidFormat,
            "The executable package-key resource size is invalid.");
    }
    const auto loaded = LoadResource(module, resource);
    const auto* bytes = static_cast<const std::uint8_t*>(
        LockResource(loaded));
    if (loaded == nullptr || bytes == nullptr) {
        return Failure<std::vector<std::uint8_t>>(
            PackageErrorCode::IoFailed,
            "Loading the executable package-key resource failed.");
    }
    return PackageResult<std::vector<std::uint8_t>>::Success(
        std::vector<std::uint8_t>(bytes, bytes + resourceSize));
}

PackageResult<bool> UpdateKeyResources(
    const std::filesystem::path& executablePath,
    std::vector<std::uint8_t>& primary,
    std::vector<std::uint8_t>* const fallback) {
    const auto update =
        BeginUpdateResourceW(executablePath.c_str(), FALSE);
    if (update == nullptr) {
        return Failure<bool>(
            PackageErrorCode::PublicationFailed,
            "Opening the executable resource table for update failed.");
    }
    const WORD language =
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    const auto updateOne = [update, language](
                               const wchar_t* const name,
                               std::vector<std::uint8_t>* bytes) {
        return UpdateResourceW(
            update,
            MAKEINTRESOURCEW(10),
            const_cast<wchar_t*>(name),
            language,
            bytes == nullptr
                ? nullptr
                : static_cast<void*>(bytes->data()),
            bytes == nullptr
                ? 0U
                : static_cast<DWORD>(bytes->size())) != FALSE;
    };
    if (!updateOne(kExecutableKeyResourceName, &primary) ||
        (fallback != nullptr &&
         !updateOne(
             kExecutableFallbackKeyResourceName,
             fallback))) {
        EndUpdateResourceW(update, TRUE);
        return Failure<bool>(
            PackageErrorCode::PublicationFailed,
            "Updating the executable package-key resources failed.");
    }
    if (!EndUpdateResourceW(update, FALSE)) {
        return Failure<bool>(
            PackageErrorCode::PublicationFailed,
            "Committing the executable package-key resources failed.");
    }
    return PackageResult<bool>::Success(true);
}

} // namespace

PackageResult<std::vector<std::uint8_t>>
SerializeExecutableKeyResource(
    const KeyId& keyId,
    const SecureBuffer& masterKey) {
    if (masterKey.size() != kPackageMasterKeySize) {
        return Failure<std::vector<std::uint8_t>>(
            PackageErrorCode::InvalidFormat,
            "The executable package master key must be exactly 32 bytes.");
    }
    ByteWriter writer;
    writer.WriteBytes(resourceMagic.data(), resourceMagic.size());
    writer.WriteUInt16(resourceMajorVersion);
    writer.WriteUInt16(resourceMinorVersion);
    writer.WriteUInt32(kExecutableKeyResourceSize);
    writer.WriteBytes(keyId.data(), keyId.size());
    writer.WriteBytes(masterKey.data(), masterKey.size());
    if (writer.Bytes().size() != kExecutableKeyResourceSize) {
        return Failure<std::vector<std::uint8_t>>(
            PackageErrorCode::InvalidFormat,
            "The executable package-key resource size is not canonical.");
    }
    return PackageResult<std::vector<std::uint8_t>>::Success(
        writer.Bytes());
}

PackageResult<ExecutablePackageKey> ParseExecutableKeyResource(
    const std::vector<std::uint8_t>& bytes,
    const KeyId& expectedKeyId) {
    if (bytes.size() != kExecutableKeyResourceSize) {
        return Failure<ExecutablePackageKey>(
            PackageErrorCode::InvalidFormat,
            "The executable package-key resource size is invalid.");
    }
    ByteReader reader(bytes);
    const auto magic = reader.ReadBytes(resourceMagic.size());
    const auto major = reader.ReadUInt16();
    const auto minor = reader.ReadUInt16();
    const auto declaredSize = reader.ReadUInt32();
    const auto keyIdBytes = reader.ReadBytes(KeyId{}.size());
    if (!magic || !major || !minor || !declaredSize ||
        !keyIdBytes) {
        return Failure<ExecutablePackageKey>(
            PackageErrorCode::InvalidFormat,
            "The executable package-key resource is truncated.");
    }
    if (!std::equal(
            magic.value().begin(),
            magic.value().end(),
            resourceMagic.begin())) {
        return Failure<ExecutablePackageKey>(
            PackageErrorCode::InvalidFormat,
            "The executable package-key resource magic is invalid.");
    }
    if (major.value() != resourceMajorVersion ||
        minor.value() != resourceMinorVersion) {
        return Failure<ExecutablePackageKey>(
            PackageErrorCode::UnsupportedVersion,
            "The executable package-key resource version is unsupported.");
    }
    if (declaredSize.value() != kExecutableKeyResourceSize) {
        return Failure<ExecutablePackageKey>(
            PackageErrorCode::InvalidFormat,
            "The executable package-key resource declared size is invalid.");
    }

    ExecutablePackageKey result;
    std::copy(
        keyIdBytes.value().begin(),
        keyIdBytes.value().end(),
        result.keyId.begin());
    if (result.keyId != expectedKeyId) {
        return Failure<ExecutablePackageKey>(
            PackageErrorCode::MissingKey,
            "The executable package-key identifier does not match.");
    }
    auto keyBytes = reader.ReadBytes(kPackageMasterKeySize);
    if (!keyBytes) {
        return Failure<ExecutablePackageKey>(
            PackageErrorCode::InvalidFormat,
            "The executable package-key bytes are truncated.");
    }
    result.masterKey =
        SecureBuffer::FromBytes(std::move(keyBytes.value()));
    return PackageResult<ExecutablePackageKey>::Success(
        std::move(result));
}

PackageResult<bool> InjectExecutablePackageKey(
    const std::filesystem::path& executablePath,
    const KeyId& keyId,
    const SecureBuffer& masterKey) {
    return InjectExecutablePackageKeys(
        executablePath,
        keyId,
        masterKey,
        nullptr);
}

PackageResult<bool> InjectExecutablePackageKeys(
    const std::filesystem::path& executablePath,
    const KeyId& keyId,
    const SecureBuffer& masterKey,
    const ExecutablePackageKey* const fallbackKey) {
    auto bytes =
        SerializeExecutableKeyResource(keyId, masterKey);
    if (!bytes) {
        return PackageResult<bool>::Failure(bytes.error());
    }
    SensitiveBytes eraseSerializedKey(bytes.value());
    PackageResult<std::vector<std::uint8_t>> fallbackBytes =
        PackageResult<std::vector<std::uint8_t>>::Success({});
    if (fallbackKey != nullptr &&
        fallbackKey->keyId != keyId) {
        fallbackBytes = SerializeExecutableKeyResource(
            fallbackKey->keyId,
            fallbackKey->masterKey);
        if (!fallbackBytes) {
            return PackageResult<bool>::Failure(
                fallbackBytes.error());
        }
    }
    SensitiveBytes eraseFallback(fallbackBytes.value());
    std::error_code statusError;
    const auto status =
        std::filesystem::symlink_status(executablePath, statusError);
    if (statusError || !std::filesystem::is_regular_file(status)) {
        return Failure<bool>(
            PackageErrorCode::IoFailed,
            "The package-key resource destination is not a regular file.");
    }

    auto* fallbackPointer =
        fallbackKey != nullptr &&
            fallbackKey->keyId != keyId
        ? &fallbackBytes.value()
        : nullptr;
    return UpdateKeyResources(
        executablePath,
        bytes.value(),
        fallbackPointer);
}

PackageResult<ExecutablePackageKey> ReadExecutablePackageKey(
    const std::filesystem::path& executablePath,
    const KeyId& expectedKeyId) {
    LoadedModule module(LoadLibraryExW(
        executablePath.c_str(),
        nullptr,
        LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE));
    if (module.get() == nullptr) {
        return Failure<ExecutablePackageKey>(
            PackageErrorCode::IoFailed,
            "Loading the executable resource image failed.");
    }
    auto bytes = ReadResourceBytes(
        module.get(),
        kExecutableKeyResourceName);
    if (!bytes) {
        return PackageResult<ExecutablePackageKey>::Failure(
            bytes.error());
    }
    auto sensitiveBytes = std::move(bytes.value());
    SensitiveBytes eraseResource(sensitiveBytes);
    auto parsed =
        ParseExecutableKeyResource(sensitiveBytes, expectedKeyId);
    if (parsed ||
        parsed.error().code != PackageErrorCode::MissingKey) {
        return parsed;
    }
    auto fallbackBytes = ReadResourceBytes(
        module.get(),
        kExecutableFallbackKeyResourceName);
    if (!fallbackBytes) {
        return PackageResult<ExecutablePackageKey>::Failure(
            parsed.error());
    }
    auto sensitiveFallback = std::move(fallbackBytes.value());
    SensitiveBytes eraseFallback(sensitiveFallback);
    return ParseExecutableKeyResource(
        sensitiveFallback,
        expectedKeyId);
}

PackageResult<ExecutablePackageKey>
ReadExecutablePackageKeyFromModule(
    void* const moduleHandle,
    const KeyId& expectedKeyId) {
    const auto module = static_cast<HMODULE>(moduleHandle);
    if (module == nullptr) {
        return Failure<ExecutablePackageKey>(
            PackageErrorCode::IoFailed,
            "The executable module handle is null.");
    }
    auto bytes = ReadResourceBytes(
        module,
        kExecutableKeyResourceName);
    if (!bytes) {
        return PackageResult<ExecutablePackageKey>::Failure(
            bytes.error());
    }
    auto sensitiveBytes = std::move(bytes.value());
    SensitiveBytes eraseResource(sensitiveBytes);
    auto parsed =
        ParseExecutableKeyResource(sensitiveBytes, expectedKeyId);
    if (parsed ||
        parsed.error().code != PackageErrorCode::MissingKey) {
        return parsed;
    }
    auto fallbackBytes = ReadResourceBytes(
        module,
        kExecutableFallbackKeyResourceName);
    if (!fallbackBytes) {
        return PackageResult<ExecutablePackageKey>::Failure(
            parsed.error());
    }
    auto sensitiveFallback = std::move(fallbackBytes.value());
    SensitiveBytes eraseFallback(sensitiveFallback);
    return ParseExecutableKeyResource(
        sensitiveFallback,
        expectedKeyId);
}

} // namespace dbp::package
