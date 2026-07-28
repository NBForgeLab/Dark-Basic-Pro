#pragma once

#include "dbp/package/PackageError.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dbp::package {

struct LegacyPckLimits {
    std::uint64_t maximumImageBytes = 1024ULL * 1024ULL * 1024ULL;
    std::uint64_t maximumArchiveBytes = 512ULL * 1024ULL * 1024ULL;
    std::uint64_t maximumEntryBytes = 512ULL * 1024ULL * 1024ULL;
    std::uint64_t maximumTotalPayloadBytes =
        1024ULL * 1024ULL * 1024ULL;
    std::uint32_t maximumEntryCount = 65'536U;
    std::uint32_t maximumPathBytes = 4'096U;
};

struct LegacyPckEntry {
    std::string path;
    std::shared_ptr<const std::vector<std::uint8_t>> bytes;
};

class LegacyPckReader {
public:
    static PackageResult<std::unique_ptr<LegacyPckReader>> OpenExecutable(
        const std::filesystem::path& executablePath,
        const LegacyPckLimits& limits = {});

    ~LegacyPckReader() = default;
    LegacyPckReader(const LegacyPckReader&) = delete;
    LegacyPckReader& operator=(const LegacyPckReader&) = delete;

    const std::vector<LegacyPckEntry>& entries() const noexcept {
        return entries_;
    }

    const LegacyPckEntry* FindEntry(
        std::string_view canonicalPath) const noexcept;

    std::uint32_t applicationKind() const noexcept {
        return applicationKind_;
    }

private:
    LegacyPckReader(
        std::vector<LegacyPckEntry> entries,
        std::uint32_t applicationKind) noexcept;

    std::vector<LegacyPckEntry> entries_;
    std::uint32_t applicationKind_ = 0;
};

} // namespace dbp::package
