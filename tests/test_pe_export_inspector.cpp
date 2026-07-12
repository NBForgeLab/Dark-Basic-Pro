#include <gtest/gtest.h>

#include "PeExportInspector.h"

#include <Windows.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

class TemporaryPe {
public:
    TemporaryPe(const std::vector<std::string>& exports, const WORD machine) {
        const auto suffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        path_ = std::filesystem::temp_directory_path() /
            ("dbpro_pe_fixture_" + suffix + ".dll");

        std::vector<unsigned char> bytes(0x1000, 0);
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(bytes.data());
        dos->e_magic = IMAGE_DOS_SIGNATURE;
        dos->e_lfanew = 0x80;

        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(bytes.data() + 0x80);
        nt->Signature = IMAGE_NT_SIGNATURE;
        nt->FileHeader.Machine = machine;
        nt->FileHeader.NumberOfSections = 1;
        nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER32);
        nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
        nt->OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
        nt->OptionalHeader.SizeOfHeaders = 0x200;
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT] = {0x1000, 0x300};

        auto* section = IMAGE_FIRST_SECTION(nt);
        std::memcpy(section->Name, ".rdata", 6);
        section->VirtualAddress = 0x1000;
        section->Misc.VirtualSize = 0x600;
        section->PointerToRawData = 0x200;
        section->SizeOfRawData = 0x600;

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
            functions[index] = 0x1400 + static_cast<DWORD>(index * 4);
            names[index] = 0x1000 + static_cast<DWORD>(stringOffset - 0x200);
            ordinals[index] = static_cast<WORD>(index);
            std::memcpy(bytes.data() + stringOffset, exports[index].c_str(), exports[index].size() + 1);
            stringOffset += exports[index].size() + 1;
        }

        std::ofstream stream(path_, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }

    explicit TemporaryPe(const std::string& rawBytes) {
        const auto suffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        path_ = std::filesystem::temp_directory_path() /
            ("dbpro_pe_fixture_" + suffix + ".dll");
        std::ofstream stream(path_, std::ios::binary);
        stream.write(rawBytes.data(), static_cast<std::streamsize>(rawBytes.size()));
    }

    ~TemporaryPe() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

constexpr const char* kStructurePatternsExport =
    "?PassStructurePatterns@@YAXPAXK@Z";

} // namespace

TEST(PeExportInspectorTest, DetectsModernStructurePatternsExport) {
    TemporaryPe fixture({"?PassDataStatementPtr@@YAXPAD0@Z", kStructurePatternsExport},
                        IMAGE_FILE_MACHINE_I386);

    const auto result = PeExportInspector::Inspect(fixture.path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result.value().machine, PeMachine::X86);
    EXPECT_EQ(result.value().exports.count(kStructurePatternsExport), 1u);
}

TEST(PeExportInspectorTest, IdentifiesLegacyCoreWithoutStructurePatterns) {
    TemporaryPe fixture({"?PassDataStatementPtr@@YAXPAD0@Z"},
                        IMAGE_FILE_MACHINE_I386);

    const auto result = PeExportInspector::Inspect(fixture.path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result.value().exports.count(kStructurePatternsExport), 0u);
}

TEST(PeExportInspectorTest, RejectsTruncatedPeFile) {
    TemporaryPe fixture("MZ");

    const auto result = PeExportInspector::Inspect(fixture.path());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PeInspectionErrorCode::MalformedImage);
}

TEST(PeExportInspectorTest, ReportsNonX86Architecture) {
    TemporaryPe fixture({}, IMAGE_FILE_MACHINE_AMD64);

    const auto result = PeExportInspector::Inspect(fixture.path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result.value().machine, PeMachine::X64);
}
