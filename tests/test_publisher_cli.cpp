#include <gtest/gtest.h>

#include "DBProTools/Publisher/PublisherCli.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <aclapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace dbp::publisher;

class ScopedPublisherDirectory {
public:
    explicit ScopedPublisherDirectory(
        const std::string& prefix) {
        static std::atomic<std::uint64_t> sequence{0};
        const auto unique =
            static_cast<std::uint64_t>(
                std::chrono::steady_clock::now()
                    .time_since_epoch()
                    .count()) ^
            sequence.fetch_add(1, std::memory_order_relaxed);
        path_ = std::filesystem::temp_directory_path() /
            (prefix + std::to_string(unique));
        std::filesystem::create_directories(path_);
    }

    ~ScopedPublisherDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    ScopedPublisherDirectory(
        const ScopedPublisherDirectory&) = delete;
    ScopedPublisherDirectory& operator=(
        const ScopedPublisherDirectory&) = delete;

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

bool RestrictFileToOwner(
    const std::filesystem::path& path) {
    PSID owner = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (GetNamedSecurityInfoW(
            path.c_str(),
            SE_FILE_OBJECT,
            OWNER_SECURITY_INFORMATION,
            &owner,
            nullptr,
            nullptr,
            nullptr,
            &descriptor) != ERROR_SUCCESS) {
        return false;
    }
    EXPLICIT_ACCESSW access{};
    access.grfAccessPermissions = GENERIC_ALL;
    access.grfAccessMode = SET_ACCESS;
    access.grfInheritance = NO_INHERITANCE;
    access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    access.Trustee.TrusteeType = TRUSTEE_IS_USER;
    access.Trustee.ptstrName = static_cast<LPWSTR>(owner);
    PACL acl = nullptr;
    const auto aclResult =
        SetEntriesInAclW(1, &access, nullptr, &acl);
    const auto setResult = aclResult == ERROR_SUCCESS
        ? SetNamedSecurityInfoW(
            const_cast<LPWSTR>(path.c_str()),
            SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION |
                PROTECTED_DACL_SECURITY_INFORMATION,
            nullptr,
            nullptr,
            acl,
            nullptr)
        : aclResult;
    if (acl != nullptr) {
        LocalFree(acl);
    }
    LocalFree(descriptor);
    return setResult == ERROR_SUCCESS;
}

TEST(PublisherCliTest, ParsesHelpAndVersionCommands) {
    const auto help = ParsePublisherArguments(
        {L"dbp-publish", L"--help"});
    ASSERT_TRUE(help) << help.error().message;
    EXPECT_EQ(help.value().kind, PublisherCommandKind::Help);

    const auto version = ParsePublisherArguments(
        {L"dbp-publish", L"--version"});
    ASSERT_TRUE(version) << version.error().message;
    EXPECT_EQ(
        version.value().kind,
        PublisherCommandKind::Version);
}

TEST(PublisherCliTest, ParsesCompletePublishCommand) {
    const auto parsed = ParsePublisherArguments({
        L"dbp-publish",
        L"publish",
        L"D:\\project\\manifest.json",
        L"--package-key-file",
        L"D:\\keys\\release.key",
        L"--json",
    });

    ASSERT_TRUE(parsed) << parsed.error().message;
    EXPECT_EQ(
        parsed.value().kind,
        PublisherCommandKind::Publish);
    EXPECT_EQ(
        parsed.value().manifestPath,
        std::filesystem::path(
            L"D:\\project\\manifest.json"));
    ASSERT_TRUE(parsed.value().packageKeyFile.has_value());
    EXPECT_EQ(
        *parsed.value().packageKeyFile,
        std::filesystem::path(
            L"D:\\keys\\release.key"));
    EXPECT_TRUE(parsed.value().json);
}

TEST(PublisherCliTest, ParsesValidateWithoutAKey) {
    const auto parsed = ParsePublisherArguments({
        L"dbp-publish",
        L"validate",
        L"manifest.json",
        L"--json",
    });

    ASSERT_TRUE(parsed) << parsed.error().message;
    EXPECT_EQ(
        parsed.value().kind,
        PublisherCommandKind::Validate);
    EXPECT_FALSE(parsed.value().packageKeyFile.has_value());
    EXPECT_TRUE(parsed.value().json);
}

TEST(
    PublisherCliTest,
    RejectsMissingValuesDuplicatesRawKeysAndUnknownOptions) {
    for (const auto& arguments :
         std::vector<std::vector<std::wstring>>{
             {L"dbp-publish", L"publish", L"manifest.json"},
             {L"dbp-publish", L"publish", L"manifest.json",
              L"--package-key-file"},
             {L"dbp-publish", L"publish", L"manifest.json",
              L"--package-key-file", L"one.key",
              L"--package-key-file", L"two.key"},
             {L"dbp-publish", L"publish", L"manifest.json",
              L"--package-key", L"secret"},
             {L"dbp-publish", L"validate", L"manifest.json",
              L"--unknown"},
             {L"dbp-publish", L"--help", L"--json"},
         }) {
        const auto parsed =
            ParsePublisherArguments(arguments);
        ASSERT_FALSE(parsed);
        EXPECT_EQ(
            parsed.error().code,
            dbp::package::PackageErrorCode::InvalidFormat);
    }
}

TEST(PublisherCliTest, RejectsDirectoryManifestPath) {
    const auto parsed = ParsePublisherArguments({
        L"dbp-publish",
        L"validate",
        std::filesystem::temp_directory_path().wstring(),
    });

    ASSERT_FALSE(parsed);
    EXPECT_EQ(
        parsed.error().code,
        dbp::package::PackageErrorCode::InvalidFormat);
}

TEST(PublisherCliTest, RunsHelpAndVersionWithoutDiagnostics) {
    std::ostringstream output;
    std::ostringstream diagnostic;

    EXPECT_EQ(
        RunPublisherProcess(
            {L"dbp-publish", L"--help"},
            output,
            diagnostic),
        0);
    EXPECT_NE(output.str().find("Usage:"), std::string::npos);
    EXPECT_TRUE(diagnostic.str().empty());

    output.str({});
    output.clear();
    EXPECT_EQ(
        RunPublisherProcess(
            {L"dbp-publish", L"--version"},
            output,
            diagnostic),
        0);
    EXPECT_EQ(output.str(), "dbp-publish 1.0.0\n");
    EXPECT_TRUE(diagnostic.str().empty());
}

TEST(PublisherCliTest, EmitsOneJsonErrorAndNoDiagnosticNoise) {
    std::ostringstream output;
    std::ostringstream diagnostic;

    EXPECT_EQ(
        RunPublisherProcess(
            {
                L"dbp-publish",
                L"publish",
                L"manifest.json",
                L"--json",
            },
            output,
            diagnostic),
        2);

    const auto result = output.str();
    EXPECT_NE(result.find("\"type\":\"error\""), std::string::npos);
    EXPECT_NE(
        result.find("\"code\":\"invalid_format\""),
        std::string::npos);
    EXPECT_EQ(
        std::count(result.begin(), result.end(), '\n'),
        1);
    EXPECT_TRUE(diagnostic.str().empty());
}

TEST(PublisherCliTest, ValidatesManifestAsMachineReadableOutput) {
    ScopedPublisherDirectory directory(
        "dbp-publisher-cli-validate-");
    const auto& root = directory.path();

    {
        std::ofstream host(root / "host.exe", std::ios::binary);
        host << "MZ";
        ASSERT_TRUE(host);
    }
    {
        std::ofstream asset(root / "asset.bin", std::ios::binary);
        asset << "asset";
        ASSERT_TRUE(asset);
    }
    const auto manifestPath = root / "publisher.json";
    {
        std::ofstream manifest(manifestPath, std::ios::binary);
        manifest << R"json({
            "schemaVersion": 1,
            "hostExecutable": "host.exe",
            "outputExecutable": "dist/game.exe",
            "assets": [{
                "source": "asset.bin",
                "destination": "media/asset.bin"
            }]
        })json";
        ASSERT_TRUE(manifest);
    }

    std::ostringstream output;
    std::ostringstream diagnostic;
    EXPECT_EQ(
        RunPublisherProcess(
            {
                L"dbp-publish",
                L"publish",
                manifestPath.wstring(),
                L"--package-key-file",
                (root / "missing-private-release.key").wstring(),
                L"--json",
            },
            output,
            diagnostic),
        3);
    EXPECT_TRUE(diagnostic.str().empty());
    EXPECT_EQ(
        output.str().find("missing-private-release.key"),
        std::string::npos);

    output.str({});
    output.clear();
    const auto exitCode = RunPublisherProcess(
        {
            L"dbp-publish",
            L"validate",
            manifestPath.wstring(),
            L"--json",
        },
        output,
        diagnostic);

    EXPECT_EQ(exitCode, 0);
    const auto result = output.str();
    EXPECT_NE(
        result.find("\"type\":\"validation\""),
        std::string::npos);
    EXPECT_NE(result.find("\"status\":\"ok\""), std::string::npos);
    EXPECT_NE(result.find("\"assetCount\":1"), std::string::npos);
    EXPECT_EQ(
        std::count(result.begin(), result.end(), '\n'),
        1);
    EXPECT_TRUE(diagnostic.str().empty());
}

TEST(PublisherCliTest, MapsEveryFailureClassToDocumentedExitCode) {
    using dbp::package::PackageErrorCode;

    EXPECT_EQ(
        PublisherFailureExitCode(
            PublisherFailurePhase::Manifest,
            PackageErrorCode::InvalidFormat),
        2);
    EXPECT_EQ(
        PublisherFailureExitCode(
            PublisherFailurePhase::Manifest,
            PackageErrorCode::IoFailed),
        3);
    EXPECT_EQ(
        PublisherFailureExitCode(
            PublisherFailurePhase::KeyGeneration,
            PackageErrorCode::CryptographyFailed),
        7);
    EXPECT_EQ(
        PublisherFailureExitCode(
            PublisherFailurePhase::Publication,
            PackageErrorCode::MissingKey),
        3);
    EXPECT_EQ(
        PublisherFailureExitCode(
            PublisherFailurePhase::Publication,
            PackageErrorCode::CompressionFailed),
        4);
    EXPECT_EQ(
        PublisherFailureExitCode(
            PublisherFailurePhase::Publication,
            PackageErrorCode::PublicationFailed),
        5);
    EXPECT_EQ(
        PublisherFailureExitCode(
            PublisherFailurePhase::Publication,
            PackageErrorCode::ArithmeticOverflow),
        7);
    EXPECT_EQ(
        PublisherFailureExitCode(
            PublisherFailurePhase::Publication,
            PackageErrorCode::UnexpectedEnd),
        7);
}

TEST(PublisherCliTest, PublishesRealPeAndEmitsSecretFreeJson) {
    ScopedPublisherDirectory directory(
        "dbp-publisher-cli-publish-");
    const auto& root = directory.path();
    ASSERT_TRUE(std::filesystem::create_directory(root / "dist"));

    std::array<wchar_t, 32'768> modulePath{};
    const auto moduleLength = GetModuleFileNameW(
        nullptr,
        modulePath.data(),
        static_cast<DWORD>(modulePath.size()));
    ASSERT_GT(moduleLength, 0U);
    ASSERT_LT(moduleLength, modulePath.size());
    const auto hostPath = root / "host.exe";
    ASSERT_TRUE(std::filesystem::copy_file(
        std::filesystem::path(
            modulePath.data(),
            modulePath.data() + moduleLength),
        hostPath));

    {
        std::ofstream asset(root / "asset.bin", std::ios::binary);
        asset << "authenticated asset";
        ASSERT_TRUE(asset);
    }
    const auto keyPath = root / "private-release.key";
    {
        std::ofstream key(keyPath, std::ios::binary);
        for (std::uint8_t value = 0; value < 32U; ++value) {
            key.put(static_cast<char>(value));
        }
        ASSERT_TRUE(key);
    }
    ASSERT_TRUE(RestrictFileToOwner(keyPath));

    const auto manifestPath = root / "publisher.json";
    {
        std::ofstream manifest(manifestPath, std::ios::binary);
        manifest << R"json({
            "schemaVersion": 1,
            "hostExecutable": "host.exe",
            "outputExecutable": "dist/game.exe",
            "mode": "application",
            "assets": [{
                "source": "asset.bin",
                "destination": "media/asset.bin",
                "compress": true
            }]
        })json";
        ASSERT_TRUE(manifest);
    }

    std::ostringstream output;
    std::ostringstream diagnostic;
    const auto exitCode = RunPublisherProcess(
        {
            L"dbp-publish",
            L"publish",
            manifestPath.wstring(),
            L"--package-key-file",
            keyPath.wstring(),
            L"--json",
        },
        output,
        diagnostic);

    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(diagnostic.str().empty());
    const auto result = output.str();
    EXPECT_NE(result.find("\"type\":\"result\""), std::string::npos);
    EXPECT_NE(result.find("\"status\":\"ok\""), std::string::npos);
    EXPECT_EQ(result.find("private-release.key"), std::string::npos);
    EXPECT_EQ(result.find("authenticated asset"), std::string::npos);
    EXPECT_EQ(
        std::count(result.begin(), result.end(), '\n'),
        1);
    EXPECT_TRUE(std::filesystem::is_regular_file(
        root / "dist/game.exe"));
    EXPECT_TRUE(std::filesystem::is_regular_file(
        root / "dist/game.dbpakref"));
    EXPECT_EQ(
        std::count_if(
            std::filesystem::directory_iterator(root / "dist"),
            std::filesystem::directory_iterator{},
            [](const std::filesystem::directory_entry& entry) {
                return entry.path().extension() == L".dbpak";
            }),
        1);

    output.str({});
    output.clear();
    ASSERT_TRUE(SetEnvironmentVariableW(
        L"DBP_TEST_FAIL_PUBLICATION_STAGE",
        L"after-package"));
    const auto interruptedExitCode = RunPublisherProcess(
        {
            L"dbp-publish",
            L"publish",
            manifestPath.wstring(),
            L"--package-key-file",
            keyPath.wstring(),
            L"--json",
        },
        output,
        diagnostic);
    ASSERT_TRUE(SetEnvironmentVariableW(
        L"DBP_TEST_FAIL_PUBLICATION_STAGE",
        nullptr));

    EXPECT_EQ(interruptedExitCode, 5);
    EXPECT_NE(
        output.str().find("\"code\":\"publication_failed\""),
        std::string::npos);
    EXPECT_TRUE(diagnostic.str().empty());
    EXPECT_TRUE(std::filesystem::is_regular_file(
        root / "dist/game.exe"));
    EXPECT_TRUE(std::filesystem::is_regular_file(
        root / "dist/game.dbpakref"));

    output.str({});
    output.clear();
    ASSERT_TRUE(SetEnvironmentVariableW(
        L"DBP_TEST_FAIL_PUBLICATION_STAGE",
        L"during-cleanup"));
    const auto cleanupExitCode = RunPublisherProcess(
        {
            L"dbp-publish",
            L"publish",
            manifestPath.wstring(),
            L"--package-key-file",
            keyPath.wstring(),
            L"--json",
        },
        output,
        diagnostic);
    ASSERT_TRUE(SetEnvironmentVariableW(
        L"DBP_TEST_FAIL_PUBLICATION_STAGE",
        nullptr));

    EXPECT_EQ(cleanupExitCode, 5);
    EXPECT_NE(
        output.str().find("\"committed\":true"),
        std::string::npos);
    EXPECT_NE(
        output.str().find("\"phase\":\"cleanup\""),
        std::string::npos);
    EXPECT_TRUE(std::filesystem::is_regular_file(
        root / "dist/game.exe"));
    EXPECT_TRUE(std::filesystem::is_regular_file(
        root / "dist/game.dbpakref"));
}

} // namespace
