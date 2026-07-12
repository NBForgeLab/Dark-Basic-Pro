#include "CompilerArguments.h"

#include <algorithm>
#include <cctype>

namespace {

bool IsOption(const std::string& value) {
    return !value.empty() && value.front() == '-';
}

std::string Lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

} // namespace

CompilerArgumentsResult ParseCompilerArguments(
    const std::vector<std::string>& arguments) {
    CompilerArguments parsed;
    for (std::size_t index = arguments.empty() ? 0 : 1;
         index < arguments.size(); ++index) {
        const auto& argument = arguments[index];
        if (argument == "--runtime-root") {
            if (parsed.runtimeRoot) {
                return CompilerArgumentsResult::Failure(
                    "--runtime-root may only be specified once.");
            }
            if (index + 1 >= arguments.size() || IsOption(arguments[index + 1])) {
                return CompilerArgumentsResult::Failure(
                    "--runtime-root requires a directory path.");
            }
            parsed.runtimeRoot = arguments[++index];
        } else if (argument == "--json") {
            parsed.json = true;
        } else if (argument == "--trace") {
            parsed.trace = true;
        } else if (argument == "--debug") {
            parsed.debug = true;
        } else if (argument == "--help" || argument == "-h" || argument == "/?") {
            parsed.help = true;
        } else if (argument == "--emit-final-source") {
            parsed.emitFinalSource = true;
        } else if (argument == "--legacy-final-source") {
            parsed.legacyFinalSource = true;
        } else if (IsOption(argument)) {
            return CompilerArgumentsResult::Failure(
                "Unknown compiler option: " + argument);
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
        Lowercase(parsed.inputPath.extension().string()) != ".dbpro") {
        return CompilerArgumentsResult::Failure(
            "--legacy-final-source requires a DBPro project input.");
    }
    return CompilerArgumentsResult::Success(std::move(parsed));
}
