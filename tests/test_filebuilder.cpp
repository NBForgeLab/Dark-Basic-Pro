#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <utility>
#include <windows.h>
#include "FileBuilder.h"
#include "DataTable.h"

// ---------------------------------------------------------------------------
// CFileBuilder contracts
// ---------------------------------------------------------------------------

// Contract: the derived PCK filename must be returned as an owning std::string
// by value. The legacy contract wrote through an unbounded caller-supplied
// LPSTR buffer (no size limit - overflow risk for long EXE paths).
TEST(FileBuilderTest, GetPCKFileFromEXEFileReturnsOwningString) {
    static_assert(std::is_same_v<decltype(std::declval<CFileBuilder&>().GetPCKFileFromEXEFile(std::declval<LPSTR>())), std::string>,
                  "GetPCKFileFromEXEFile must return std::string by value");
}

// The .exe extension is replaced with .pck, preserving the rest of the path.
TEST(FileBuilderTest, GetPCKFileFromEXEFileReplacesExtension) {
    CFileBuilder builder;

    char exeName[] = "C:\\Projects\\My Game\\game.exe";
    EXPECT_EQ(builder.GetPCKFileFromEXEFile(exeName), "C:\\Projects\\My Game\\game.pck");
}

// Relative paths keep their form; only the last four characters are swapped.
TEST(FileBuilderTest, GetPCKFileFromEXEFileHandlesRelativePath) {
    CFileBuilder builder;

    char exeName[] = "output.exe";
    EXPECT_EQ(builder.GetPCKFileFromEXEFile(exeName), "output.pck");
}

// ---------------------------------------------------------------------------
// CDataTable memory contracts
// ---------------------------------------------------------------------------

// The chain owns every node and string added to it (unique_ptr members free
// them on destruction); FindString locates entries by stored index.
TEST(DataTableTest, AddStringThenFindStringReturnsIndex) {
    CDataTable table;

    char text[] = "hello world";
    ASSERT_TRUE(table.AddString(text, 7));

    EXPECT_EQ(table.FindString(text), 7u);
}

// AddUniqueString must reject a duplicate and hand back the existing index
// without leaking or double-owning the rejected node.
TEST(DataTableTest, AddUniqueStringRejectsDuplicate) {
    CDataTable table;

    char text[] = "unique entry";
    DWORD dwIndex = 3;
    ASSERT_TRUE(table.AddUniqueString(text, &dwIndex));

    DWORD dwDuplicateIndex = 99;
    EXPECT_FALSE(table.AddUniqueString(text, &dwDuplicateIndex));
    EXPECT_EQ(dwDuplicateIndex, 3u);
}

// AddTwoStrings stores both strings in the owned node.
TEST(DataTableTest, AddTwoStringsStoresBothStrings) {
    CDataTable table;

    char first[] = "first";
    char second[] = "second";
    DWORD dwIndex = 5;
    ASSERT_TRUE(table.AddTwoStrings(first, second, &dwIndex));

    CDataTable* pNode = table.GetNext();
    ASSERT_NE(pNode, nullptr);
    ASSERT_NE(pNode->GetString(), nullptr);
    ASSERT_NE(pNode->GetString2(), nullptr);
    EXPECT_STREQ(pNode->GetString()->GetStr(), "first");
    EXPECT_STREQ(pNode->GetString2()->GetStr(), "second");
    EXPECT_EQ(pNode->GetIndex(), 5u);
}
