#include "dbp/package/KeyProvider.h"

#include "dbp/package/PackagePath.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <aclapi.h>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace dbp::package {

namespace {

constexpr char manifestKeyInfo[] = "DBP-PAK-v2/manifest";
constexpr char entryKeyInfoPrefix[] = "DBP-PAK-v2/entry/";

PackageError MissingKeyError() {
    return {
        PackageErrorCode::MissingKey,
        "The requested package key is unavailable.",
        std::nullopt,
    };
}

PackageResult<SecureBuffer> CloneValidatedMasterKey(
    const SecureBuffer& masterKey) {
    if (masterKey.size() != kPackageMasterKeySize) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }
    return PackageResult<SecureBuffer>::Success(
        SecureBuffer::FromBytes(masterKey.CopyBytes()));
}

std::vector<std::uint8_t> Bytes(
    const char* const begin,
    const std::size_t size) {
    return std::vector<std::uint8_t>(begin, begin + size);
}

std::vector<std::uint8_t> PackageSalt(
    const PackageId& packageId) {
    return std::vector<std::uint8_t>(
        packageId.begin(),
        packageId.end());
}

bool GrantsFileRead(const ACCESS_MASK mask) noexcept {
    constexpr ACCESS_MASK readMask =
        GENERIC_READ | GENERIC_ALL | FILE_READ_DATA;
    return (mask & readMask) != 0;
}

bool IsOwnerOrPrivileged(
    const PSID sid,
    const PSID owner) noexcept {
    return EqualSid(sid, owner) ||
        IsWellKnownSid(sid, WinLocalSystemSid) ||
        IsWellKnownSid(sid, WinBuiltinAdministratorsSid);
}

PSID ObjectAceSid(const ACCESS_ALLOWED_OBJECT_ACE* ace) noexcept {
    auto* cursor = reinterpret_cast<const std::uint8_t*>(
        &ace->SidStart);
    if ((ace->Flags & ACE_OBJECT_TYPE_PRESENT) != 0) {
        cursor += sizeof(GUID);
    }
    if ((ace->Flags & ACE_INHERITED_OBJECT_TYPE_PRESENT) != 0) {
        cursor += sizeof(GUID);
    }
    return const_cast<PSID>(
        static_cast<const void*>(cursor));
}

bool HasOwnerRestrictedReadAcl(
    const HANDLE handle) noexcept {
    PSID owner = nullptr;
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    const auto securityResult = GetSecurityInfo(
        handle,
        SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &owner,
        nullptr,
        &dacl,
        nullptr,
        &descriptor);
    if (securityResult != ERROR_SUCCESS ||
        owner == nullptr ||
        dacl == nullptr) {
        if (descriptor != nullptr) {
            LocalFree(descriptor);
        }
        return false;
    }

    bool restricted = true;
    for (DWORD index = 0;
         index < dacl->AceCount && restricted;
         ++index) {
        void* rawAce = nullptr;
        if (!GetAce(dacl, index, &rawAce) ||
            rawAce == nullptr) {
            restricted = false;
            break;
        }
        const auto* header =
            static_cast<const ACE_HEADER*>(rawAce);
        ACCESS_MASK mask = 0;
        PSID sid = nullptr;
        if (header->AceType == ACCESS_ALLOWED_ACE_TYPE) {
            const auto* ace =
                static_cast<const ACCESS_ALLOWED_ACE*>(rawAce);
            mask = ace->Mask;
            sid = const_cast<PSID>(
                static_cast<const void*>(&ace->SidStart));
        } else if (
            header->AceType ==
                ACCESS_ALLOWED_OBJECT_ACE_TYPE) {
            const auto* ace =
                static_cast<const ACCESS_ALLOWED_OBJECT_ACE*>(
                    rawAce);
            mask = ace->Mask;
            sid = ObjectAceSid(ace);
        } else if (
            header->AceType == ACCESS_ALLOWED_CALLBACK_ACE_TYPE ||
            header->AceType ==
                ACCESS_ALLOWED_CALLBACK_OBJECT_ACE_TYPE) {
            restricted = false;
            break;
        } else {
            continue;
        }
        if (!IsValidSid(sid) ||
            (GrantsFileRead(mask) &&
             !IsOwnerOrPrivileged(sid, owner))) {
            restricted = false;
        }
    }
    LocalFree(descriptor);
    return restricted;
}

class KeyFileHandle {
public:
    explicit KeyFileHandle(const HANDLE value) noexcept
        : value_(value) {}
    ~KeyFileHandle() {
        if (value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }
    KeyFileHandle(const KeyFileHandle&) = delete;
    KeyFileHandle& operator=(const KeyFileHandle&) = delete;
    HANDLE get() const noexcept { return value_; }
private:
    HANDLE value_;
};

} // namespace

MemoryKeyProvider::MemoryKeyProvider(
    const KeyId keyId,
    SecureBuffer masterKey) noexcept
    : keyId_(keyId),
      masterKey_(std::move(masterKey)) {}

PackageResult<SecureBuffer> MemoryKeyProvider::Resolve(
    const KeyId& keyId) const {
    if (keyId != keyId_) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }
    return CloneValidatedMasterKey(masterKey_);
}

FileKeyProvider::FileKeyProvider(
    const KeyId keyId,
    std::filesystem::path keyFile) noexcept
    : keyId_(keyId),
      keyFile_(std::move(keyFile)) {}

PackageResult<SecureBuffer> FileKeyProvider::Resolve(
    const KeyId& keyId) const {
    if (keyId != keyId_) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }

    KeyFileHandle input(CreateFileW(
        keyFile_.c_str(),
        GENERIC_READ | READ_CONTROL,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr));
    FILE_ATTRIBUTE_TAG_INFO tag{};
    LARGE_INTEGER size{};
    if (input.get() == INVALID_HANDLE_VALUE ||
        !GetFileInformationByHandleEx(
            input.get(),
            FileAttributeTagInfo,
            &tag,
            sizeof(tag)) ||
        (tag.FileAttributes &
            (FILE_ATTRIBUTE_DIRECTORY |
             FILE_ATTRIBUTE_REPARSE_POINT)) != 0 ||
        !GetFileSizeEx(input.get(), &size) ||
        size.QuadPart !=
            static_cast<LONGLONG>(kPackageMasterKeySize) ||
        !HasOwnerRestrictedReadAcl(input.get())) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }

    auto bytes = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(kPackageMasterKeySize));
    DWORD bytesRead = 0;
    if (!ReadFile(
            input.get(),
            bytes.data(),
            static_cast<DWORD>(bytes.size()),
            &bytesRead,
            nullptr) ||
        bytesRead != bytes.size()) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }

    return PackageResult<SecureBuffer>::Success(
        std::move(bytes));
}

PackageKeyDeriver::PackageKeyDeriver(
    const CryptoProvider& crypto) noexcept
    : crypto_(crypto) {}

PackageResult<SecureBuffer> PackageKeyDeriver::DeriveManifestKey(
    const SecureBuffer& masterKey,
    const PackageId& packageId) const {
    if (masterKey.size() != kPackageMasterKeySize) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }
    const auto info = Bytes(
        manifestKeyInfo,
        sizeof(manifestKeyInfo) - 1);
    return crypto_.HkdfSha256(
        masterKey,
        PackageSalt(packageId),
        info,
        kPackageMasterKeySize);
}

PackageResult<SecureBuffer> PackageKeyDeriver::DeriveEntryKey(
    const SecureBuffer& masterKey,
    const PackageId& packageId,
    const std::string_view canonicalPath) const {
    if (masterKey.size() != kPackageMasterKeySize) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }

    const auto path = ValidatePersistedPackagePath(canonicalPath);
    if (!path) {
        return PackageResult<SecureBuffer>::Failure(
            path.error());
    }
    const auto pathBytes = std::vector<std::uint8_t>(
        path.value().begin(),
        path.value().end());
    const auto pathDigest = crypto_.Sha256(pathBytes);
    if (!pathDigest) {
        return PackageResult<SecureBuffer>::Failure(
            pathDigest.error());
    }

    auto info = Bytes(
        entryKeyInfoPrefix,
        sizeof(entryKeyInfoPrefix) - 1);
    info.insert(
        info.end(),
        pathDigest.value().begin(),
        pathDigest.value().end());
    return crypto_.HkdfSha256(
        masterKey,
        PackageSalt(packageId),
        info,
        kPackageMasterKeySize);
}

} // namespace dbp::package
