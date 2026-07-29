#include "DBProTools/Publisher/PublisherCli.h"

#include "DBProTools/Publisher/PublisherManifest.h"
#include "dbp/package/ApplicationPublisher.h"
#include "dbp/package/KeyProvider.h"

#include <nlohmann/json.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dbp::publisher {

namespace {

using package::PackageError;
using package::PackageErrorCode;
using package::PackageResult;

constexpr std::string_view kPublisherVersion = "1.0.0";

PackageError ArgumentError(std::string message) {
    return {
        PackageErrorCode::InvalidFormat,
        std::move(message),
        std::nullopt,
    };
}

PackageResult<PublisherArguments> ArgumentFailure(
    std::string message) {
    return PackageResult<PublisherArguments>::Failure(
        ArgumentError(std::move(message)));
}

bool IsOption(const std::wstring& value) {
    return value.size() > 1U && value.front() == L'-';
}

std::string Utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const auto required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) {
        return {};
    }
    std::string result(
        static_cast<std::size_t>(required),
        '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            required,
            nullptr,
            nullptr) != required) {
        return {};
    }
    return result;
}

std::string Utf8Path(
    const std::filesystem::path& path) {
    return Utf8(path.wstring());
}

const char* ErrorCodeName(
    const PackageErrorCode code) {
    switch (code) {
    case PackageErrorCode::InvalidFormat:
        return "invalid_format";
    case PackageErrorCode::UnsafePath:
        return "unsafe_path";
    case PackageErrorCode::LimitExceeded:
        return "limit_exceeded";
    case PackageErrorCode::MissingKey:
        return "missing_key";
    case PackageErrorCode::IoFailed:
        return "io_failed";
    case PackageErrorCode::PublicationFailed:
        return "publication_failed";
    case PackageErrorCode::CryptographyFailed:
        return "cryptography_failed";
    case PackageErrorCode::AuthenticationFailed:
        return "authentication_failed";
    case PackageErrorCode::IntegrityFailed:
        return "integrity_failed";
    case PackageErrorCode::CompressionFailed:
        return "compression_failed";
    case PackageErrorCode::UnexpectedEnd:
        return "unexpected_end";
    case PackageErrorCode::ArithmeticOverflow:
        return "arithmetic_overflow";
    case PackageErrorCode::UnsupportedVersion:
        return "unsupported_version";
    case PackageErrorCode::UnsupportedAlgorithm:
        return "unsupported_algorithm";
    }
    return "unknown";
}

const char* PublicationPhaseName(
    const package::ApplicationPublicationPhase phase) {
    switch (phase) {
    case package::ApplicationPublicationPhase::Package:
        return "package";
    case package::ApplicationPublicationPhase::Executable:
        return "executable";
    case package::ApplicationPublicationPhase::Descriptor:
        return "descriptor";
    case package::ApplicationPublicationPhase::Cleanup:
        return "cleanup";
    }
    return "unknown";
}

void EmitError(
    const PackageError& error,
    const bool jsonMode,
    std::ostream& output,
    std::ostream& diagnostic) {
    if (jsonMode) {
        nlohmann::json document{
            {"type", "error"},
            {"code", ErrorCodeName(error.code)},
            {"message", error.message},
            {"committed", error.applicationTupleCommitted},
        };
        if (error.applicationPublicationPhase) {
            document["phase"] = PublicationPhaseName(
                *error.applicationPublicationPhase);
        }
        output << document.dump() << '\n';
    } else {
        diagnostic << "error: " << error.message << '\n';
        if (error.applicationTupleCommitted) {
            diagnostic
                << "note: the application tuple was committed "
                   "before cleanup failed.\n";
        }
    }
}

class EnvironmentPublicationCheckpoint final
    : public package::PublicationCheckpoint {
public:
    PackageResult<bool> Reach(
        const package::PublicationStage stage) const override {
        std::array<wchar_t, 64> value{};
        const auto length = GetEnvironmentVariableW(
            L"DBP_TEST_FAIL_PUBLICATION_STAGE",
            value.data(),
            static_cast<DWORD>(value.size()));
        if (length == 0 || length >= value.size()) {
            return PackageResult<bool>::Success(true);
        }
        const std::wstring_view requested(
            value.data(),
            length);
        const auto matches =
            (stage == package::PublicationStage::PackagePublished &&
             requested == L"after-package") ||
            (stage == package::PublicationStage::ExecutablePublished &&
             requested == L"after-executable") ||
            (stage == package::PublicationStage::DescriptorPublished &&
             requested == L"after-descriptor") ||
            (stage == package::PublicationStage::CleanupStarted &&
             requested == L"during-cleanup");
        if (!matches) {
            return PackageResult<bool>::Success(true);
        }
        return PackageResult<bool>::Failure({
            PackageErrorCode::PublicationFailed,
            "A publication interruption was requested.",
            std::nullopt,
        });
    }
};

PackageResult<bool> WaitForManifestSnapshotTestGate() {
    const auto required = GetEnvironmentVariableW(
        L"DBP_TEST_MANIFEST_SNAPSHOT_GATE",
        nullptr,
        0);
    if (required == 0) {
        return PackageResult<bool>::Success(true);
    }
    std::wstring value(required, L'\0');
    const auto length = GetEnvironmentVariableW(
        L"DBP_TEST_MANIFEST_SNAPSHOT_GATE",
        value.data(),
        required);
    if (length == 0 || length >= required) {
        return PackageResult<bool>::Failure({
            PackageErrorCode::IoFailed,
            "Reading the manifest snapshot test gate failed.",
            std::nullopt,
        });
    }
    value.resize(length);
    const std::filesystem::path gate(value);
    std::error_code statusError;
    const auto gateStatus =
        std::filesystem::symlink_status(gate, statusError);
    if (statusError ||
        !std::filesystem::is_directory(gateStatus) ||
        std::filesystem::is_symlink(gateStatus)) {
        return PackageResult<bool>::Failure({
            PackageErrorCode::IoFailed,
            "The manifest snapshot test gate is unavailable or unsafe.",
            std::nullopt,
        });
    }

    const auto readyPath = gate / L"ready";
    const HANDLE ready = CreateFileW(
        readyPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (ready == INVALID_HANDLE_VALUE) {
        return PackageResult<bool>::Failure({
            PackageErrorCode::IoFailed,
            "Creating the manifest snapshot test gate failed.",
            std::nullopt,
        });
    }
    CloseHandle(ready);

    const auto continuePath = gate / L"continue";
    const auto deadline = GetTickCount64() + 10'000ULL;
    while (GetFileAttributesW(continuePath.c_str()) ==
           INVALID_FILE_ATTRIBUTES) {
        if (GetTickCount64() >= deadline) {
            return PackageResult<bool>::Failure({
                PackageErrorCode::IoFailed,
                "The manifest snapshot test gate timed out.",
                std::nullopt,
            });
        }
        Sleep(10);
    }
    return PackageResult<bool>::Success(true);
}

bool ContainsJsonOption(
    const std::vector<std::wstring>& arguments) {
    return std::find(
               arguments.begin(),
               arguments.end(),
               L"--json") != arguments.end();
}

} // namespace

int PublisherFailureExitCode(
    const PublisherFailurePhase phase,
    const PackageErrorCode code) noexcept {
    if (phase == PublisherFailurePhase::KeyGeneration) {
        return 7;
    }
    if (phase == PublisherFailurePhase::Manifest) {
        switch (code) {
        case PackageErrorCode::InvalidFormat:
        case PackageErrorCode::UnsupportedVersion:
        case PackageErrorCode::UnsupportedAlgorithm:
        case PackageErrorCode::LimitExceeded:
        case PackageErrorCode::UnsafePath:
            return 2;
        case PackageErrorCode::IoFailed:
        case PackageErrorCode::MissingKey:
            return 3;
        case PackageErrorCode::UnexpectedEnd:
        case PackageErrorCode::ArithmeticOverflow:
        case PackageErrorCode::CryptographyFailed:
        case PackageErrorCode::AuthenticationFailed:
        case PackageErrorCode::IntegrityFailed:
        case PackageErrorCode::CompressionFailed:
        case PackageErrorCode::PublicationFailed:
            return 7;
        }
        return 7;
    }
    switch (code) {
    case PackageErrorCode::MissingKey:
        return 3;
    case PackageErrorCode::PublicationFailed:
        return 5;
    case PackageErrorCode::UnexpectedEnd:
    case PackageErrorCode::ArithmeticOverflow:
        return 7;
    case PackageErrorCode::InvalidFormat:
    case PackageErrorCode::UnsupportedVersion:
    case PackageErrorCode::UnsupportedAlgorithm:
    case PackageErrorCode::LimitExceeded:
    case PackageErrorCode::UnsafePath:
    case PackageErrorCode::CryptographyFailed:
    case PackageErrorCode::AuthenticationFailed:
    case PackageErrorCode::IntegrityFailed:
    case PackageErrorCode::CompressionFailed:
    case PackageErrorCode::IoFailed:
        return 4;
    }
    return 7;
}

PackageResult<PublisherArguments>
ParsePublisherArguments(
    const std::vector<std::wstring>& arguments) {
    if (arguments.size() == 2U &&
        (arguments[1] == L"--help" ||
         arguments[1] == L"-h")) {
        PublisherArguments result;
        result.kind = PublisherCommandKind::Help;
        return PackageResult<PublisherArguments>::Success(
            std::move(result));
    }
    if (arguments.size() == 2U &&
        arguments[1] == L"--version") {
        PublisherArguments result;
        result.kind = PublisherCommandKind::Version;
        return PackageResult<PublisherArguments>::Success(
            std::move(result));
    }
    if (arguments.size() < 3U) {
        return ArgumentFailure(
            "Expected publish or validate and a manifest path.");
    }

    PublisherArguments result;
    if (arguments[1] == L"publish") {
        result.kind = PublisherCommandKind::Publish;
    } else if (arguments[1] == L"validate") {
        result.kind = PublisherCommandKind::Validate;
    } else {
        return ArgumentFailure("Unknown publisher command.");
    }
    if (arguments[2].empty() ||
        IsOption(arguments[2])) {
        return ArgumentFailure(
            "A manifest file path is required.");
    }
    result.manifestPath = arguments[2];
    std::error_code statusError;
    if (std::filesystem::is_directory(
            result.manifestPath,
            statusError) &&
        !statusError) {
        return ArgumentFailure(
            "The manifest path must name a file.");
    }

    for (std::size_t index = 3U;
         index < arguments.size();
         ++index) {
        const auto& option = arguments[index];
        if (option == L"--json") {
            if (result.json) {
                return ArgumentFailure(
                    "--json may only be specified once.");
            }
            result.json = true;
        } else if (option == L"--package-key-file") {
            if (result.packageKeyFile) {
                return ArgumentFailure(
                    "--package-key-file may only be specified once.");
            }
            if (index + 1U >= arguments.size() ||
                arguments[index + 1U].empty() ||
                IsOption(arguments[index + 1U])) {
                return ArgumentFailure(
                    "--package-key-file requires a binary key file path.");
            }
            result.packageKeyFile = arguments[++index];
        } else {
            return ArgumentFailure(
                "Unknown publisher option.");
        }
    }
    if (result.kind == PublisherCommandKind::Publish &&
        !result.packageKeyFile) {
        return ArgumentFailure(
            "publish requires --package-key-file.");
    }
    if (result.kind == PublisherCommandKind::Validate &&
        result.packageKeyFile) {
        return ArgumentFailure(
            "validate does not accept --package-key-file.");
    }
    return PackageResult<PublisherArguments>::Success(
        std::move(result));
}

namespace {

int RunPublisherProcessUnchecked(
    const std::vector<std::wstring>& arguments,
    std::ostream& standardOutput,
    std::ostream& standardError) {
    const auto parsed =
        ParsePublisherArguments(arguments);
    if (!parsed) {
        const auto jsonMode =
            ContainsJsonOption(arguments);
        EmitError(
            parsed.error(),
            jsonMode,
            standardOutput,
            standardError);
        return 2;
    }
    const auto& command = parsed.value();
    if (command.kind == PublisherCommandKind::Help) {
        standardOutput
            << "Usage:\n"
            << "  dbp-publish publish <manifest.json> "
               "--package-key-file <32-byte-file> [--json]\n"
            << "  dbp-publish validate <manifest.json> [--json]\n"
            << "  dbp-publish --help\n"
            << "  dbp-publish --version\n";
        return 0;
    }
    if (command.kind == PublisherCommandKind::Version) {
        standardOutput
            << "dbp-publish " << kPublisherVersion << '\n';
        return 0;
    }

    const auto manifest =
        ReadPublisherManifest(command.manifestPath);
    if (!manifest) {
        EmitError(
            manifest.error(),
            command.json,
            standardOutput,
            standardError);
        return PublisherFailureExitCode(
            PublisherFailurePhase::Manifest,
            manifest.error().code);
    }
    if (command.kind == PublisherCommandKind::Validate) {
        if (command.json) {
            nlohmann::json document{
                {"type", "validation"},
                {"status", "ok"},
                {"schemaVersion", manifest.value().schemaVersion},
                {"assetCount", manifest.value().assets.size()},
            };
            standardOutput << document.dump() << '\n';
        } else {
            standardOutput
                << "Manifest is valid ("
                << manifest.value().assets.size()
                << " assets).\n";
        }
        return 0;
    }

    package::CngCryptoProvider crypto;
    const auto keyIdBytes =
        crypto.RandomBytes(package::KeyId{}.size());
    if (!keyIdBytes) {
        EmitError(
            keyIdBytes.error(),
            command.json,
            standardOutput,
            standardError);
        return PublisherFailureExitCode(
            PublisherFailurePhase::KeyGeneration,
            keyIdBytes.error().code);
    }
    package::KeyId keyId{};
    std::copy(
        keyIdBytes.value().begin(),
        keyIdBytes.value().end(),
        keyId.begin());
    package::FileKeyProvider fileKeys(
        keyId,
        *command.packageKeyFile);
    auto key = fileKeys.Resolve(keyId);
    if (!key) {
        EmitError(
            key.error(),
            command.json,
            standardOutput,
            standardError);
        return 3;
    }
    package::MemoryKeyProvider keys(
        keyId,
        std::move(key.value()));
    package::ZstdCompressionCodec compression;
    package::Win32AtomicFilePublisher filePublisher;
    EnvironmentPublicationCheckpoint checkpoint;
    package::ApplicationPublisher publisher(
        crypto,
        compression,
        filePublisher,
        checkpoint);
    const auto request = BuildApplicationPublishRequest(
        manifest.value(),
        keyId);
    const auto snapshotGate =
        WaitForManifestSnapshotTestGate();
    if (!snapshotGate) {
        EmitError(
            snapshotGate.error(),
            command.json,
            standardOutput,
            standardError);
        return PublisherFailureExitCode(
            PublisherFailurePhase::Publication,
            snapshotGate.error().code);
    }
    const auto published =
        publisher.Publish(request, keys);
    if (!published) {
        EmitError(
            published.error(),
            command.json,
            standardOutput,
            standardError);
        return PublisherFailureExitCode(
            PublisherFailurePhase::Publication,
            published.error().code);
    }
    if (command.json) {
        nlohmann::json document{
            {"type", "result"},
            {"status", "ok"},
            {"executable", Utf8Path(
                published.value().executablePath)},
            {"descriptor", Utf8Path(
                published.value().descriptorPath)},
            {"package", Utf8Path(
                published.value().package.packagePath)},
        };
        standardOutput << document.dump() << '\n';
    } else {
        standardOutput
            << "Published "
            << Utf8Path(
                published.value().executablePath)
            << '\n';
    }
    return 0;
}

} // namespace

int RunPublisherProcess(
    const std::vector<std::wstring>& arguments,
    std::ostream& standardOutput,
    std::ostream& standardError) {
    try {
        return RunPublisherProcessUnchecked(
            arguments,
            standardOutput,
            standardError);
    } catch (...) {
        if (ContainsJsonOption(arguments)) {
            nlohmann::json document{
                {"type", "error"},
                {"code", "invariant"},
                {
                    "message",
                    "An unexpected internal publisher failure occurred.",
                },
            };
            standardOutput << document.dump() << '\n';
        } else {
            standardError
                << "error: An unexpected internal publisher "
                   "failure occurred.\n";
        }
        return 7;
    }
}

} // namespace dbp::publisher
