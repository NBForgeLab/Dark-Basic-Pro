#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <iterator>
#include <filesystem>

// ---------------------------------------------------------------------------
// Characterization tests for Task #15:
//   Replace ALL strcpy/strcat/wsprintf with std::string/snprintf
//   in EXEBlock.cpp and FileBuilder.cpp
//
// These tests scan the source of the two files and assert that none of the
// unsafe C-string patterns remain. They FAIL on the legacy code and must
// stay GREEN after the replacement refactor.
// ---------------------------------------------------------------------------

namespace {

std::string ReadSourceFile(const char* relativePath)
{
    const std::filesystem::path root(DBP_TEST_SOURCE_ROOT);
    const auto full = root / relativePath;
    std::ifstream input(full, std::ios::binary);
    if (!input)
        return {};
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    };
}

// Count occurrences of a regex pattern in text (non-overlapping).
int CountPatternMatches(const std::string& text, const std::string& pattern)
{
    std::regex re(pattern);
    auto begin = std::sregex_iterator(text.begin(), text.end(), re);
    auto end = std::sregex_iterator();
    return static_cast<int>(std::distance(begin, end));
}

struct UnsafePattern {
    const char* pattern;
    const char* description;
};

// Patterns that must NOT appear in the target files.
// We match the function-call forms only (not comments) by requiring
// an opening paren.  The patterns intentionally avoid matching inside
// comment lines (// or block comments) – the test strips those first.
const UnsafePattern kForbiddenPatterns[] = {
    { R"(\bstrcpy\s*\()",     "strcpy("   },
    { R"(\bstrcat\s*\()",     "strcat("   },
    { R"(\bwsprintf\s*\()",   "wsprintf(" },
    { R"(\bwsprintfA\s*\()",  "wsprintfA("},
    { R"(\bwsprintfW\s*\()",  "wsprintfW("},
};

std::string StripComments(const std::string& src)
{
    std::string result;
    result.reserve(src.size());
    for (size_t i = 0; i < src.size(); ++i)
    {
        // Line comment
        if (i + 1 < src.size() && src[i] == '/' && src[i + 1] == '/')
        {
            // Skip to end of line
            while (i < src.size() && src[i] != '\n')
                ++i;
            if (i < src.size())
                result += '\n';
            continue;
        }
        // Block comment
        if (i + 1 < src.size() && src[i] == '/' && src[i + 1] == '*')
        {
            i += 2;
            while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/'))
                ++i;
            i += 1; // skip past '/'
            continue;
        }
        result += src[i];
    }
    return result;
}

void AssertNoUnsafePatterns(const std::string& filePath, const std::string& source)
{
    ASSERT_FALSE(source.empty())
        << "Could not read source file: " << filePath;

    const std::string code = StripComments(source);

    for (const auto& entry : kForbiddenPatterns)
    {
        const int count = CountPatternMatches(code, entry.pattern);
        EXPECT_EQ(count, 0)
            << "Found " << count << " occurrence(s) of " << entry.description
            << " in " << filePath;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// EXEBlock.cpp must not contain unsafe C-string calls
// ---------------------------------------------------------------------------
TEST(UnsafeStringPatternsTest, EXEBlock_HasNoUnsafeStringCalls)
{
    const char relativePath[] = "DBProCompiler/DBPCompiler/EXEBlock.cpp";
    const auto source = ReadSourceFile(relativePath);
    AssertNoUnsafePatterns(relativePath, source);
}

// ---------------------------------------------------------------------------
// FileBuilder.cpp must not contain unsafe C-string calls
// ---------------------------------------------------------------------------
TEST(UnsafeStringPatternsTest, FileBuilder_HasNoUnsafeStringCalls)
{
    const char relativePath[] = "DBProCompiler/DBPCompiler/FileBuilder.cpp";
    const auto source = ReadSourceFile(relativePath);
    AssertNoUnsafePatterns(relativePath, source);
}
