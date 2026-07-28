#include "dbp/package/PackagePath.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace dbp::package {

namespace {

PackageError UnsafePathError(std::string message) {
    return {
        PackageErrorCode::UnsafePath,
        std::move(message),
        std::nullopt,
    };
}

PackageResult<std::wstring> Utf8ToWide(const std::string_view path) {
    if (path.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return PackageResult<std::wstring>::Failure(
            UnsafePathError("Package path exceeds the Windows UTF-8 conversion limit."));
    }

    const auto inputSize = static_cast<int>(path.size());
    const int required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        path.data(),
        inputSize,
        nullptr,
        0);
    if (required <= 0) {
        return PackageResult<std::wstring>::Failure(
            UnsafePathError("Package path is not valid UTF-8."));
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    const int converted = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        path.data(),
        inputSize,
        result.data(),
        required);
    if (converted != required) {
        return PackageResult<std::wstring>::Failure(
            UnsafePathError("Package path UTF-8 conversion failed."));
    }
    return PackageResult<std::wstring>::Success(std::move(result));
}

PackageResult<std::wstring> NormalizeWide(const std::wstring& path) {
    const int required = NormalizeString(
        NormalizationC,
        path.data(),
        static_cast<int>(path.size()),
        nullptr,
        0);
    if (required <= 0) {
        return PackageResult<std::wstring>::Failure(
            UnsafePathError("Package path Unicode normalization failed."));
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    const int normalized = NormalizeString(
        NormalizationC,
        path.data(),
        static_cast<int>(path.size()),
        result.data(),
        required);
    if (normalized <= 0 || normalized > required) {
        return PackageResult<std::wstring>::Failure(
            UnsafePathError("Package path Unicode normalization failed."));
    }
    result.resize(static_cast<std::size_t>(normalized));
    return PackageResult<std::wstring>::Success(std::move(result));
}

PackageResult<std::string> WideToUtf8(const std::wstring& path) {
    const int required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        path.data(),
        static_cast<int>(path.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) {
        return PackageResult<std::string>::Failure(
            UnsafePathError("Package path UTF-8 encoding failed."));
    }

    std::string result(static_cast<std::size_t>(required), '\0');
    const int converted = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        path.data(),
        static_cast<int>(path.size()),
        result.data(),
        required,
        nullptr,
        nullptr);
    if (converted != required) {
        return PackageResult<std::string>::Failure(
            UnsafePathError("Package path UTF-8 encoding failed."));
    }
    return PackageResult<std::string>::Success(std::move(result));
}

PackageResult<std::string> NormalizeUtf8(const std::string_view path) {
    const auto wide = Utf8ToWide(path);
    if (!wide) {
        return PackageResult<std::string>::Failure(wide.error());
    }
    const auto normalized = NormalizeWide(wide.value());
    if (!normalized) {
        return PackageResult<std::string>::Failure(normalized.error());
    }
    return WideToUtf8(normalized.value());
}

bool IsAsciiDrivePrefix(const std::string_view path) {
    if (path.size() < 2 || path[1] != ':') {
        return false;
    }
    const char first = path[0];
    return (first >= 'A' && first <= 'Z') ||
        (first >= 'a' && first <= 'z');
}

bool ContainsWindowsForbiddenCharacter(const std::string_view component) {
    constexpr std::string_view forbidden = R"(<>:"|?*)";
    return std::any_of(
        component.begin(),
        component.end(),
        [forbidden](const unsigned char character) {
            return character < 0x20U ||
                forbidden.find(static_cast<char>(character)) !=
                    std::string_view::npos;
        });
}

std::string AsciiUpper(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char character) {
            if (character >= 'a' && character <= 'z') {
                return static_cast<char>(character - ('a' - 'A'));
            }
            return static_cast<char>(character);
        });
    return value;
}

bool IsReservedWindowsName(const std::string_view component) {
    const auto dot = component.find('.');
    const std::string base = AsciiUpper(
        std::string(component.substr(0, dot)));
    constexpr std::array<std::string_view, 22> reserved{
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
    };
    return std::find(reserved.begin(), reserved.end(), base) != reserved.end();
}

PackageResult<std::string> ValidateCanonicalPath(
    const std::string& path) {
    constexpr std::size_t maximumPathBytes = 1024;
    if (path.empty()) {
        return PackageResult<std::string>::Failure(
            UnsafePathError("Package path is empty."));
    }
    if (path.size() > maximumPathBytes) {
        return PackageResult<std::string>::Failure(
            UnsafePathError("Package path exceeds 1024 UTF-8 bytes."));
    }
    if (path.front() == '/' || IsAsciiDrivePrefix(path)) {
        return PackageResult<std::string>::Failure(
            UnsafePathError("Package path must be relative."));
    }
    if (path.back() == '/') {
        return PackageResult<std::string>::Failure(
            UnsafePathError("Package path must not end with a separator."));
    }
    if (path.find('\\') != std::string::npos) {
        return PackageResult<std::string>::Failure(
            UnsafePathError("Persisted package paths must use '/' separators."));
    }

    std::size_t componentStart = 0;
    while (componentStart < path.size()) {
        const auto separator = path.find('/', componentStart);
        const auto componentEnd =
            separator == std::string::npos ? path.size() : separator;
        const std::string_view component(
            path.data() + componentStart,
            componentEnd - componentStart);

        if (component.empty() || component == "." || component == "..") {
            return PackageResult<std::string>::Failure(
                UnsafePathError("Package path contains an unsafe component."));
        }
        if (component.back() == ' ' || component.back() == '.') {
            return PackageResult<std::string>::Failure(
                UnsafePathError("Package path component has an ambiguous Windows suffix."));
        }
        if (ContainsWindowsForbiddenCharacter(component)) {
            return PackageResult<std::string>::Failure(
                UnsafePathError("Package path contains a Windows-forbidden character."));
        }
        if (IsReservedWindowsName(component)) {
            return PackageResult<std::string>::Failure(
                UnsafePathError("Package path contains a reserved Windows device name."));
        }

        if (separator == std::string::npos) {
            break;
        }
        componentStart = separator + 1;
    }

    return PackageResult<std::string>::Success(path);
}

int CompareOrdinalIgnoreCase(
    const std::wstring& left,
    const std::wstring& right) {
    return CompareStringOrdinal(
        left.data(),
        static_cast<int>(left.size()),
        right.data(),
        static_cast<int>(right.size()),
        TRUE);
}

} // namespace

PackageResult<std::string> NormalizePackageInputPath(
    const std::string_view path) {
    std::string separated(path);
    std::replace(separated.begin(), separated.end(), '\\', '/');

    const auto normalized = NormalizeUtf8(separated);
    if (!normalized) {
        return normalized;
    }
    return ValidateCanonicalPath(normalized.value());
}

PackageResult<std::string> ValidatePersistedPackagePath(
    const std::string_view path) {
    if (path.find('\\') != std::string_view::npos) {
        return PackageResult<std::string>::Failure(
            UnsafePathError("Persisted package paths must use '/' separators."));
    }

    const auto normalized = NormalizeUtf8(path);
    if (!normalized) {
        return normalized;
    }
    if (normalized.value() != path) {
        return PackageResult<std::string>::Failure(
            UnsafePathError("Persisted package path is not Unicode NFC."));
    }
    return ValidateCanonicalPath(normalized.value());
}

PackageResult<std::vector<std::string>> ValidateAndSortPackagePaths(
    const std::vector<std::string>& paths) {
    std::vector<std::string> sorted;
    sorted.reserve(paths.size());
    std::vector<std::wstring> widePaths;
    widePaths.reserve(paths.size());

    for (const auto& path : paths) {
        const auto validated = ValidatePersistedPackagePath(path);
        if (!validated) {
            return PackageResult<std::vector<std::string>>::Failure(
                validated.error());
        }
        sorted.push_back(validated.value());

        const auto wide = Utf8ToWide(validated.value());
        if (!wide) {
            return PackageResult<std::vector<std::string>>::Failure(
                wide.error());
        }
        widePaths.push_back(wide.value());
    }

    std::sort(sorted.begin(), sorted.end());
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) {
        return PackageResult<std::vector<std::string>>::Failure(
            UnsafePathError("Package contains a duplicate path."));
    }

    std::sort(
        widePaths.begin(),
        widePaths.end(),
        [](const std::wstring& left, const std::wstring& right) {
            return CompareOrdinalIgnoreCase(left, right) == CSTR_LESS_THAN;
        });
    for (std::size_t index = 1; index < widePaths.size(); ++index) {
        if (CompareOrdinalIgnoreCase(
                widePaths[index - 1],
                widePaths[index]) == CSTR_EQUAL) {
            return PackageResult<std::vector<std::string>>::Failure(
                UnsafePathError(
                    "Package paths collide under Windows ordinal case folding."));
        }
    }

    return PackageResult<std::vector<std::string>>::Success(
        std::move(sorted));
}

} // namespace dbp::package
