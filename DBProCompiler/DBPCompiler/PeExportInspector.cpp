#include "PeExportInspector.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <vector>

namespace {

template <typename T>
const T* ReadObject(const std::vector<std::byte>& bytes, const std::size_t offset) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) {
        return nullptr;
    }
    return reinterpret_cast<const T*>(bytes.data() + offset);
}

bool CheckedMultiply(const std::size_t left,
                     const std::size_t right,
                     std::size_t& result) {
    if (left != 0 && right > (std::numeric_limits<std::size_t>::max)() / left) {
        return false;
    }
    result = left * right;
    return true;
}

bool CheckedAdd(const std::size_t left,
                const std::size_t right,
                std::size_t& result) {
    if (left > (std::numeric_limits<std::size_t>::max)() - right) {
        return false;
    }
    result = left + right;
    return true;
}

std::optional<std::size_t> RvaToOffset(
    const std::vector<std::byte>& bytes,
    const IMAGE_SECTION_HEADER* sections,
    const WORD sectionCount,
    const DWORD rva,
    const std::size_t requiredSize) {
    for (WORD index = 0; index < sectionCount; ++index) {
        const auto& section = sections[index];
        const DWORD sectionSize = (std::max)(
            section.Misc.VirtualSize, section.SizeOfRawData);
        if (rva < section.VirtualAddress ||
            rva - section.VirtualAddress >= sectionSize) {
            continue;
        }

        const std::uint64_t relative = rva - section.VirtualAddress;
        const std::uint64_t offset =
            static_cast<std::uint64_t>(section.PointerToRawData) + relative;
        if (offset > bytes.size() || requiredSize > bytes.size() - offset) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(offset);
    }
    return std::nullopt;
}

std::optional<std::string> ReadCString(
    const std::vector<std::byte>& bytes,
    const std::size_t offset) {
    if (offset >= bytes.size()) {
        return std::nullopt;
    }
    const auto* begin = reinterpret_cast<const char*>(bytes.data() + offset);
    const auto* end = reinterpret_cast<const char*>(bytes.data() + bytes.size());
    const auto* terminator = std::find(begin, end, '\0');
    if (terminator == end) {
        return std::nullopt;
    }
    return std::string(begin, terminator);
}

PeInspectionResult<PeImageInfo> Malformed(
    const std::filesystem::path& path,
    const std::string& detail) {
    return PeInspectionResult<PeImageInfo>::Failure({
        PeInspectionErrorCode::MalformedImage,
        "Malformed PE image: " + detail,
        path});
}

} // namespace

PeInspectionResult<PeImageInfo> PeExportInspector::Inspect(
    const std::filesystem::path& path) {
    std::error_code filesystemError;
    if (!std::filesystem::exists(path, filesystemError)) {
        return PeInspectionResult<PeImageInfo>::Failure({
            PeInspectionErrorCode::FileNotFound,
            "PE image does not exist.",
            path});
    }

    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        return PeInspectionResult<PeImageInfo>::Failure({
            PeInspectionErrorCode::FileUnreadable,
            "PE image could not be opened.",
            path});
    }
    const auto length = stream.tellg();
    if (length < 0) {
        return PeInspectionResult<PeImageInfo>::Failure({
            PeInspectionErrorCode::FileUnreadable,
            "PE image length could not be read.",
            path});
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    stream.seekg(0);
    if (!bytes.empty()) {
        stream.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!stream) {
            return PeInspectionResult<PeImageInfo>::Failure({
                PeInspectionErrorCode::FileUnreadable,
                "PE image could not be read completely.",
                path});
        }
    }

    const auto* dos = ReadObject<IMAGE_DOS_HEADER>(bytes, 0);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0) {
        return Malformed(path, "invalid DOS header");
    }
    const auto ntOffset = static_cast<std::size_t>(dos->e_lfanew);
    const auto* signature = ReadObject<DWORD>(bytes, ntOffset);
    const auto* fileHeader = ReadObject<IMAGE_FILE_HEADER>(
        bytes, ntOffset + sizeof(DWORD));
    if (!signature || *signature != IMAGE_NT_SIGNATURE || !fileHeader) {
        return Malformed(path, "invalid NT header");
    }

    PeImageInfo info;
    if (fileHeader->Machine == IMAGE_FILE_MACHINE_I386) {
        info.machine = PeMachine::X86;
    } else if (fileHeader->Machine == IMAGE_FILE_MACHINE_AMD64) {
        info.machine = PeMachine::X64;
    } else {
        // An architecture we do not support (ARM64, ARM, ...) must never be
        // mistaken for x86. The resolver rejects anything but X86/X64, so
        // without this fallback an unknown core would silently pass as X86.
        info.machine = PeMachine::Other;
    }

    const auto optionalOffset = ntOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
    const auto* optionalMagic = ReadObject<WORD>(bytes, optionalOffset);
    if (!optionalMagic ||
        fileHeader->SizeOfOptionalHeader < sizeof(WORD) ||
        optionalOffset > bytes.size() ||
        fileHeader->SizeOfOptionalHeader > bytes.size() - optionalOffset) {
        return Malformed(path, "invalid optional header");
    }

    IMAGE_DATA_DIRECTORY exportData{};
    if (*optionalMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        if (fileHeader->SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32)) {
            return Malformed(path, "truncated PE32 optional header");
        }
        exportData = ReadObject<IMAGE_OPTIONAL_HEADER32>(bytes, optionalOffset)
            ->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    } else if (*optionalMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (fileHeader->SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64)) {
            return Malformed(path, "truncated PE32+ optional header");
        }
        exportData = ReadObject<IMAGE_OPTIONAL_HEADER64>(bytes, optionalOffset)
            ->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    } else {
        return Malformed(path, "unknown optional-header format");
    }

    const auto sectionOffset = optionalOffset + fileHeader->SizeOfOptionalHeader;
    std::size_t sectionBytes = 0;
    if (!CheckedMultiply(fileHeader->NumberOfSections,
                         sizeof(IMAGE_SECTION_HEADER), sectionBytes) ||
        sectionOffset > bytes.size() || sectionBytes > bytes.size() - sectionOffset) {
        return Malformed(path, "truncated section table");
    }
    const auto* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(
        bytes.data() + sectionOffset);

    if (exportData.VirtualAddress == 0 || exportData.Size == 0) {
        return PeInspectionResult<PeImageInfo>::Success(std::move(info));
    }
    const auto directoryOffset = RvaToOffset(
        bytes, sections, fileHeader->NumberOfSections,
        exportData.VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY));
    if (!directoryOffset) {
        return Malformed(path, "export directory is outside the image");
    }
    const auto* directory = ReadObject<IMAGE_EXPORT_DIRECTORY>(
        bytes, *directoryOffset);

    std::size_t namesSize = 0;
    std::size_t ordinalsSize = 0;
    if (!CheckedMultiply(directory->NumberOfNames, sizeof(DWORD), namesSize) ||
        !CheckedMultiply(directory->NumberOfNames, sizeof(WORD), ordinalsSize)) {
        return Malformed(path, "export table count overflows");
    }
    const auto namesOffset = RvaToOffset(
        bytes, sections, fileHeader->NumberOfSections,
        directory->AddressOfNames, namesSize);
    const auto ordinalsOffset = RvaToOffset(
        bytes, sections, fileHeader->NumberOfSections,
        directory->AddressOfNameOrdinals, ordinalsSize);
    if (!namesOffset || !ordinalsOffset) {
        return Malformed(path, "export name table is outside the image");
    }

    const auto* names = reinterpret_cast<const DWORD*>(bytes.data() + *namesOffset);
    const auto* ordinals = reinterpret_cast<const WORD*>(bytes.data() + *ordinalsOffset);
    for (DWORD index = 0; index < directory->NumberOfNames; ++index) {
        if (ordinals[index] >= directory->NumberOfFunctions) {
            return Malformed(path, "export ordinal is outside the function table");
        }
        const auto nameOffset = RvaToOffset(
            bytes, sections, fileHeader->NumberOfSections, names[index], 1);
        if (!nameOffset) {
            return Malformed(path, "export name is outside the image");
        }
        const auto name = ReadCString(bytes, *nameOffset);
        if (!name) {
            return Malformed(path, "export name is not terminated");
        }
        info.exports.insert(*name);
    }

    return PeInspectionResult<PeImageInfo>::Success(std::move(info));
}
