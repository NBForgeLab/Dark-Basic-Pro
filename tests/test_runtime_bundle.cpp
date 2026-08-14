#include <gtest/gtest.h>

#include "RuntimeContract.h"
#include "RuntimeBundleResolver.h"
#include "Str.h"
#include "DBPCompiler.h"
#include "DB3.h"

#include <Windows.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void WriteCoreFixture(const std::filesystem::path& path,
                      const bool modern,
                      const bool completeLifecycle = true) {
    std::vector<std::string> exports{
        "?PassCmdLineHandlerPtr@@YAXPAX@Z",
        "?PassErrorHandlerPtr@@YAXPAX@Z",
        "?PassEscapePtr@@YAXPAX@Z",
        "?PassBreakOutPtr@@YAXPAX@Z",
        "?PassDataStatementPtr@@YAXPAD0@Z"};
    if (completeLifecycle) {
        const std::vector<std::string> lifecycle{
            "?PassDLLs@@YAXXZ", "?ConstructDLLs@@YAXXZ", "?GetGlobPtr@@YAKXZ",
            "?InitDisplay@@YAKKKKKPAUHINSTANCE__@@PAD@Z", "?CloseDisplay@@YAKXZ",
            "?CreateVariableSpace@@YAKK@Z", "?DeleteVariableSpace@@YAXXZ",
            "?CreateDataSpace@@YAKK@Z", "?DeleteDataSpace@@YAXXZ",
            "?DeleteSingleVariableAllocation@@YAXPAK@Z", "?UnDimDD@@YA_K_K@Z",
            "?Sync@@YAXXZ"};
        exports.insert(exports.end(), lifecycle.begin(), lifecycle.end());
    }
    if (modern) {
        exports.push_back("?PassStructurePatterns@@YAXPAXK@Z");
    }

    std::vector<unsigned char> bytes(0x1000, 0);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(bytes.data());
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(bytes.data() + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.NumberOfSections = 1;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER32);
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    nt->OptionalHeader.SizeOfHeaders = 0x200;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT] = {0x1000, 0x400};
    auto* section = IMAGE_FIRST_SECTION(nt);
    section->VirtualAddress = 0x1000;
    section->Misc.VirtualSize = 0x700;
    section->PointerToRawData = 0x200;
    section->SizeOfRawData = 0x700;
    auto* directory = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(bytes.data() + 0x200);
    directory->Base = 1;
    directory->NumberOfFunctions = static_cast<DWORD>(exports.size());
    directory->NumberOfNames = static_cast<DWORD>(exports.size());
    directory->AddressOfFunctions = 0x1100;
    directory->AddressOfNames = 0x1200;
    directory->AddressOfNameOrdinals = 0x1300;
    auto* functions = reinterpret_cast<DWORD*>(bytes.data() + 0x300);
    auto* names = reinterpret_cast<DWORD*>(bytes.data() + 0x400);
    auto* ordinals = reinterpret_cast<WORD*>(bytes.data() + 0x500);
    std::size_t stringOffset = 0x600;
    for (std::size_t index = 0; index < exports.size(); ++index) {
        functions[index] = 0x1500 + static_cast<DWORD>(index * 4);
        names[index] = 0x1000 + static_cast<DWORD>(stringOffset - 0x200);
        ordinals[index] = static_cast<WORD>(index);
        std::memcpy(bytes.data() + stringOffset, exports[index].c_str(),
                    exports[index].size() + 1);
        stringOffset += exports[index].size() + 1;
    }
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

class TemporaryRuntimeBundle {
public:
    explicit TemporaryRuntimeBundle(
        const bool modern, const bool completeLifecycle = true) {
        const auto suffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        root_ = std::filesystem::temp_directory_path() /
            ("dbpro_runtime_bundle_" + suffix);
        WriteCoreFixture(root_ / "plugins" / "DBProCore.dll", modern,
                         completeLifecycle);
        std::filesystem::create_directories(root_ / "plugins-user");
        std::filesystem::create_directories(root_ / "plugins-licensed");
        std::filesystem::create_directories(root_ / "effects");
    }

    ~TemporaryRuntimeBundle() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    const std::filesystem::path& root() const noexcept { return root_; }

private:
    std::filesystem::path root_;
};

} // namespace

TEST(RuntimeBundleIntegrationTest, CompilerKeepsHostCommandsWithSelectedCoreRuntime) {
    TemporaryRuntimeBundle bundle(true);
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto installRoot = std::filesystem::temp_directory_path() /
        ("dbpro_compiler_install_" + suffix);
    const auto compilerDirectory = installRoot / "Compiler";
    std::filesystem::create_directories(compilerDirectory / "LANG" / "ENGLISH");
    std::filesystem::create_directories(compilerDirectory / "plugins");
    std::filesystem::create_directories(compilerDirectory / "plugins-user");
    std::filesystem::create_directories(compilerDirectory / "plugins-licensed");
    std::filesystem::create_directories(installRoot / "temp");
    std::ofstream(compilerDirectory / "SETUP.INI")
        << "[SETTINGS]\nTEXTLANGUAGE=ENGLISH\n";
    std::ofstream(compilerDirectory / "LANG" / "ENGLISH" / "ERRORS.TXT");
    std::ofstream(compilerDirectory / "DBPDebugger.exe");
    std::ofstream(compilerDirectory / "plugins" / "compress.dll");

    auto compilerPath = (compilerDirectory / "DBPCompiler.exe").string();
    CDBPCompiler compiler(compilerPath.data());
    compiler.SetRuntimeRootOverride(bundle.root());

    ASSERT_TRUE(compiler.EstablishRequiredBaseFiles());
    EXPECT_EQ(
        std::filesystem::weakly_canonical(
            compiler.GetInternalFile(PATH_PLUGINSFOLDER)),
        std::filesystem::weakly_canonical(compilerDirectory / "plugins"));
    EXPECT_EQ(
        std::filesystem::weakly_canonical(
            compiler.GetInternalFile(PATH_PLUGINSUSERFOLDER)),
        std::filesystem::weakly_canonical(compilerDirectory / "plugins-user"));
    EXPECT_EQ(
        std::filesystem::weakly_canonical(
            compiler.GetInternalFile(PATH_PLUGINSLICENSEDFOLDER)),
        std::filesystem::weakly_canonical(
            compilerDirectory / "plugins-licensed"));

    std::error_code error;
    std::filesystem::remove_all(installRoot, error);
}

TEST(RuntimeBundleIntegrationTest, RejectsIncompleteHostCompilerInstallation) {
    TemporaryRuntimeBundle bundle(true);
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto installRoot = std::filesystem::temp_directory_path() /
        ("dbpro_incomplete_compiler_install_" + suffix);
    const auto compilerDirectory = installRoot / "Compiler";
    std::filesystem::create_directories(compilerDirectory);

    auto compilerPath = (compilerDirectory / "DBPCompiler.exe").string();
    CDBPCompiler compiler(compilerPath.data());
    compiler.SetRuntimeRootOverride(bundle.root());

    const bool previousHeadlessMode = db3::g_bHeadlessMode;
    db3::g_bHeadlessMode = true;
    const bool established = compiler.EstablishRequiredBaseFiles();
    db3::g_bHeadlessMode = previousHeadlessMode;

    EXPECT_FALSE(established);

    std::error_code error;
    std::filesystem::remove_all(installRoot, error);
}

TEST(RuntimeContractTest, ReportsOnlyMissingRequiredCapabilities) {
    const RuntimeCapabilities available{
        RuntimeCapability::CoreBootstrapV1,
        RuntimeCapability::CoreDataStatementsV1};
    const ProgramRuntimeRequirements required{
        RuntimeCapability::CoreBootstrapV1,
        RuntimeCapability::CoreStructurePatternsV1};

    EXPECT_EQ(
        MissingCapabilities(available, required),
        RuntimeCapabilities{RuntimeCapability::CoreStructurePatternsV1});
}

TEST(RuntimeContractTest, StructurePatternsAreRequiredOnlyForNonEmptyMetadata) {
    EXPECT_EQ(
        DeriveProgramRuntimeRequirements(0).count(
            RuntimeCapability::CoreStructurePatternsV1),
        0u);
    EXPECT_EQ(
        DeriveProgramRuntimeRequirements(1).count(
            RuntimeCapability::CoreStructurePatternsV1),
        1u);
}

TEST(RuntimeContractTest, AlwaysRequiresTheBaselineCoreContract) {
    const auto requirements = DeriveProgramRuntimeRequirements(0);

    EXPECT_EQ(requirements.count(RuntimeCapability::CoreBootstrapV1), 1u);
    EXPECT_EQ(requirements.count(RuntimeCapability::CoreDataStatementsV1), 1u);
    EXPECT_EQ(requirements.count(RuntimeCapability::CoreRuntimeErrorsV1), 1u);
}

TEST(RuntimeBundleResolverTest, ExplicitRootWinsAndIsCanonical) {
    TemporaryRuntimeBundle bundle(true);
    const RuntimeSelection selection{bundle.root(), "C:/unused/compiler/bin"};

    const auto result = RuntimeBundleResolver::Resolve(
        selection, DeriveProgramRuntimeRequirements(1));

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result.value().root, std::filesystem::weakly_canonical(bundle.root()));
    EXPECT_EQ(result.value().classification, RuntimeBundleClassification::LegacyUnversioned);
}

TEST(RuntimeBundleResolverTest, RejectsLegacyCoreWhenStructurePatternsRequired) {
    TemporaryRuntimeBundle bundle(false);

    const auto result = RuntimeBundleResolver::Resolve(
        RuntimeSelection{bundle.root(), "C:/unused/compiler/bin"},
        DeriveProgramRuntimeRequirements(1));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, RuntimeErrorCode::MissingCapability);
    EXPECT_EQ(result.error().capability,
              RuntimeCapability::CoreStructurePatternsV1);
}

TEST(RuntimeBundleResolverTest, AcceptsLegacyCoreWhenStructurePatternsUnused) {
    TemporaryRuntimeBundle bundle(false);

    const auto result = RuntimeBundleResolver::Resolve(
        RuntimeSelection{bundle.root(), "C:/unused/compiler/bin"},
        DeriveProgramRuntimeRequirements(0));

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result.value().capabilities.count(
                  RuntimeCapability::CoreStructurePatternsV1), 0u);
}

TEST(RuntimeBundleResolverTest, RejectsNonX86Core) {
    TemporaryRuntimeBundle bundle(true);
    auto corePath = bundle.root() / "plugins" / "DBProCore.dll";
    std::fstream stream(corePath, std::ios::binary | std::ios::in | std::ios::out);
    stream.seekp(0x84);
    const WORD machine = IMAGE_FILE_MACHINE_AMD64;
    stream.write(reinterpret_cast<const char*>(&machine), sizeof(machine));
    stream.close();

    const auto result = RuntimeBundleResolver::Resolve(
        RuntimeSelection{bundle.root(), "C:/unused/compiler/bin"},
        DeriveProgramRuntimeRequirements(0));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, RuntimeErrorCode::IncompatibleArchitecture);
}

TEST(RuntimeBundleResolverTest, RejectsIncompleteCoreLifecycleContract) {
    TemporaryRuntimeBundle bundle(false, false);

    const auto result = RuntimeBundleResolver::Resolve(
        RuntimeSelection{bundle.root(), "C:/unused/compiler/bin"},
        DeriveProgramRuntimeRequirements(0));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, RuntimeErrorCode::MissingCapability);
    EXPECT_EQ(result.error().capability, RuntimeCapability::CoreBootstrapV1);
}
