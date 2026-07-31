#include <gtest/gtest.h>

#include "TextLineSplitter.h"

#include <string>
#include <vector>

namespace {

std::vector<std::string> SplitAll(const std::string& text) {
    TextLineCursor cursor;
    TextLineCursorInit(&cursor, text.c_str(), static_cast<unsigned long>(text.size()));

    std::vector<std::string> lines;
    const char* lineStart = nullptr;
    unsigned long lineLength = 0;
    while (TextLineCursorNext(&cursor, &lineStart, &lineLength)) {
        lines.emplace_back(lineStart, lineLength);
    }
    return lines;
}

TEST(TextLineSplitterTest, SplitsCrLfTerminatedLines) {
    const auto lines = SplitAll("alpha\r\nbeta\r\ngamma\r\n");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "alpha");
    EXPECT_EQ(lines[1], "beta");
    EXPECT_EQ(lines[2], "gamma");
}

TEST(TextLineSplitterTest, SplitsLfOnlyLines) {
    // Regression: git eol normalisation rewrites game text assets (FPI
    // scripts) to LF-only; the legacy CRLF-only scan collapsed the whole
    // file into a single line and the script parser saw zero commands.
    const auto lines = SplitAll("alpha\nbeta\ngamma\n");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "alpha");
    EXPECT_EQ(lines[1], "beta");
    EXPECT_EQ(lines[2], "gamma");
}

TEST(TextLineSplitterTest, SplitsCrOnlyLines) {
    const auto lines = SplitAll("alpha\rbeta\rgamma\r");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "alpha");
    EXPECT_EQ(lines[1], "beta");
    EXPECT_EQ(lines[2], "gamma");
}

TEST(TextLineSplitterTest, SplitsMixedLineEndings) {
    const auto lines = SplitAll("one\r\ntwo\nthree\rfour");
    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[0], "one");
    EXPECT_EQ(lines[1], "two");
    EXPECT_EQ(lines[2], "three");
    EXPECT_EQ(lines[3], "four");
}

TEST(TextLineSplitterTest, EmitsFinalLineWithoutTrailingNewline) {
    const auto lines = SplitAll("alpha\r\nbeta");
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "alpha");
    EXPECT_EQ(lines[1], "beta");
}

TEST(TextLineSplitterTest, PreservesBlankLines) {
    // FPI files separate sections with blank lines; each blank line must
    // stay an empty array entry so line indices remain stable.
    const auto lines = SplitAll("alpha\r\n\r\nbeta\n\ngamma");
    ASSERT_EQ(lines.size(), 5u);
    EXPECT_EQ(lines[0], "alpha");
    EXPECT_EQ(lines[1], "");
    EXPECT_EQ(lines[2], "beta");
    EXPECT_EQ(lines[3], "");
    EXPECT_EQ(lines[4], "gamma");
}

TEST(TextLineSplitterTest, EmptyInputYieldsNoLines) {
    const auto lines = SplitAll("");
    EXPECT_TRUE(lines.empty());
}

TEST(TextLineSplitterTest, CrLfIsOneBreakNotTwo) {
    const auto lines = SplitAll("alpha\r\nbeta");
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "alpha");
    EXPECT_EQ(lines[1], "beta");
}

TEST(TextLineSplitterTest, ParsesRealFpiScriptWithLfEndings) {
    // Mirrors the exact failure observed in the cetron1e title screen:
    // an LF-only titlepage.fpi must still yield one entry per line.
    const std::string fpi =
        ";AIScript from Wizard\n"
        ";Header\n"
        "desc          = Title Page Wizard V103\n"
        ";Script\n"
        ":state=0:hudreset,hudmake=display\n"
        ":state=0:state=2\n";
    const auto lines = SplitAll(fpi);
    ASSERT_EQ(lines.size(), 6u);
    EXPECT_EQ(lines[2], "desc          = Title Page Wizard V103");
    EXPECT_EQ(lines[4], ":state=0:hudreset,hudmake=display");
}

}  // namespace
