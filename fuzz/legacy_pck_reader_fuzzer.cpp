#include "dbp/package/LegacyPckReader.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace {

class ScopedInputFile {
public:
    ScopedInputFile(const std::uint8_t* data, const std::size_t size) {
        static std::atomic<std::uint64_t> sequence{0};
        path_ = std::filesystem::temp_directory_path() /
            ("dbp-legacy-fuzz-" +
             std::to_string(GetCurrentProcessId()) + "-" +
             std::to_string(sequence.fetch_add(
                 1,
                 std::memory_order_relaxed)) +
             ".exe");
        std::ofstream output(
            path_,
            std::ios::binary | std::ios::trunc);
        if (size != 0) {
            output.write(
                reinterpret_cast<const char*>(data),
                static_cast<std::streamsize>(size));
        }
        valid_ = static_cast<bool>(output);
    }

    ~ScopedInputFile() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    bool valid() const noexcept {
        return valid_;
    }

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
    bool valid_ = false;
};

} // namespace

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    const std::size_t size) {
    constexpr std::size_t maximumInput = 4U * 1024U * 1024U;
    if (size > maximumInput) {
        return 0;
    }
    ScopedInputFile input(data, size);
    if (!input.valid()) {
        return 0;
    }

    dbp::package::LegacyPckLimits limits;
    limits.maximumImageBytes = maximumInput;
    limits.maximumArchiveBytes = maximumInput;
    limits.maximumEntryBytes = maximumInput;
    limits.maximumTotalPayloadBytes = maximumInput;
    limits.maximumEntryCount = 1'000;
    limits.maximumPathBytes = 1'024;
    (void)dbp::package::LegacyPckReader::OpenExecutable(
        input.path(),
        limits);
    return 0;
}
