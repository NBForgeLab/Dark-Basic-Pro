#include "CompilerArguments.h"
#include "TextConvert.h"

#include <algorithm>
#include <cctype>
#include <cwctype>

namespace {

bool IsOption(const std::string& value) {
    return !value.empty() && value.front() == '-';
}

bool IsOption(const std::wstring& value) {
    return !value.empty() && value.front() == L'-';
}

std::string Lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

std::wstring Lowercase(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](const wchar_t character) {
            return static_cast<wchar_t>(std::towlower(character));
        });
    return value;
}

} // namespace

CompilerArgumentsResult ParseCompilerArguments(
    const std::vector<std::string>& arguments) {
    std::vector<std::wstring> wideArguments;
    wideArguments.reserve(arguments.size());
    for (const auto& argument : arguments) {
        wideArguments.push_back(TextConvert::UTF8ToUTF16(argument));
    }
    return ParseWideCompilerArguments(wideArguments);
}

CompilerArgumentsResult ParseWideCompilerArguments(
    const std::vector<std::wstring>& arguments) {
    CompilerArguments parsed;
    for (std::size_t index = arguments.empty() ? 0 : 1;
         index < arguments.size(); ++index) {
        const auto& argument = arguments[index];
        if (argument == L"--runtime-root") {
            if (parsed.runtimeRoot) {
                return CompilerArgumentsResult::Failure(
                    "--runtime-root may only be specified once.");
            }
            if (index + 1 >= arguments.size() || IsOption(arguments[index + 1])) {
                return CompilerArgumentsResult::Failure(
                    "--runtime-root requires a directory path.");
            }
            parsed.runtimeRoot = arguments[++index];
        } else if (argument == L"--output") {
            if (parsed.outputPath) {
                return CompilerArgumentsResult::Failure(
                    "--output may only be specified once.");
            }
            if (index + 1 >= arguments.size() || IsOption(arguments[index + 1])) {
                return CompilerArgumentsResult::Failure(
                    "--output requires an executable file path.");
            }
            parsed.outputPath = arguments[++index];
        } else if (argument == L"--package-key-file") {
            if (parsed.packageKeyFile) {
                return CompilerArgumentsResult::Failure(
                    "--package-key-file may only be specified once.");
            }
            if (index + 1 >= arguments.size() ||
                IsOption(arguments[index + 1])) {
                return CompilerArgumentsResult::Failure(
                    "--package-key-file requires a binary key file path.");
            }
            parsed.packageKeyFile = arguments[++index];
        } else if (argument == L"--json") {
            parsed.json = true;
        } else if (argument == L"--trace") {
            parsed.trace = true;
        } else if (argument == L"--debug") {
            parsed.debug = true;
        } else if (argument == L"--help" || argument == L"-h" || argument == L"/?") {
            parsed.help = true;
        } else if (argument == L"--emit-final-source") {
            parsed.emitFinalSource = true;
        } else if (argument == L"--legacy-final-source") {
            parsed.legacyFinalSource = true;
        } else if (IsOption(argument)) {
            return CompilerArgumentsResult::Failure(
                "Unknown compiler option: " + TextConvert::UTF16ToUTF8(argument));
        } else if (!parsed.inputPath.empty()) {
            return CompilerArgumentsResult::Failure(
                "Only one input file may be specified.");
        } else {
            parsed.inputPath = argument;
        }
    }

    if (parsed.emitFinalSource && parsed.legacyFinalSource) {
        return CompilerArgumentsResult::Failure(
            "--emit-final-source conflicts with --legacy-final-source.");
    }
    if (parsed.legacyFinalSource &&
        Lowercase(parsed.inputPath.extension().wstring()) != L".dbpro") {
        return CompilerArgumentsResult::Failure(
            "--legacy-final-source requires a DBPro project input.");
    }
    if (parsed.outputPath &&
        Lowercase(parsed.inputPath.extension().wstring()) != L".dbpro") {
        return CompilerArgumentsResult::Failure(
            "--output requires a DBPro project input.");
    }
    if (parsed.outputPath &&
        Lowercase(parsed.outputPath->extension().wstring()) != L".exe") {
        return CompilerArgumentsResult::Failure(
            "--output requires an .exe file path.");
    }
    if (parsed.packageKeyFile &&
        Lowercase(parsed.inputPath.extension().wstring()) != L".dbpro") {
        return CompilerArgumentsResult::Failure(
            "--package-key-file requires a DBPro project input.");
    }
    return CompilerArgumentsResult::Success(std::move(parsed));
}
