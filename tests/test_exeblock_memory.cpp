#include <gtest/gtest.h>
#include <cstring>
#include <memory>
#include "EXEBlock.h"

// Memory-safety specification for CEXEBlock::Clear():
// after Clear() every owned allocation is released, every owning pointer
// is null and every element counter is zero, so later consumers such as
// FreeUptoDisplay() can never iterate a stale count over a null array.

namespace {

char* MakeString(const char* pText)
{
    char* pStr = new char[strlen(pText) + 1];
    strcpy(pStr, pText);
    return pStr;
}

void PopulateExeBlock(CEXEBlock& exe)
{
    exe.m_pInitialAppName = MakeString("app");

    exe.m_dwNumberOfDLLs = 2;
    exe.m_pDLLIndexArray = exe.CreateArray(2);
    exe.m_pDLLLoadedAlreadyArray = exe.CreateArray(2);
    exe.m_pDLLFilenameArray = exe.CreatePtrArray(2);
    exe.m_pDLLFilenameArray[0] = (uintptr_t)MakeString("core.dll");
    exe.m_pDLLFilenameArray[1] = (uintptr_t)MakeString("plugin.dll");

    exe.m_dwNumberOfReferences = 3;
    exe.m_pRefArray = exe.CreateArray(3);
    exe.m_pRefTypeArray = exe.CreateArray(3);
    exe.m_pRefIndexArray = exe.CreateArray(3);

    exe.m_dwNumberOfRuntimeErrorStrings = 1;
    exe.m_pRuntimeErrorStringsArray = exe.CreatePtrArray(1);
    exe.m_pRuntimeErrorStringsArray[0] = (uintptr_t)MakeString("runtime error");

    exe.m_dwNumberOfCommands = 1;
    exe.m_pCommandDLLIdArray = exe.CreateArray(1);
    exe.m_pCommandDLLCallArray = exe.CreatePtrArray(1);
    exe.m_pCommandDLLCallArray[0] = (uintptr_t)MakeString("Command");

    exe.m_dwNumberOfStrings = 1;
    exe.m_pStringsArray = exe.CreatePtrArray(1);
    exe.m_pStringsArray[0] = (uintptr_t)MakeString("hello");

    exe.m_dwNumberOfDataItems = 1;
    exe.m_pDataArray = new char[10]();
    exe.m_pDataStringsArray = exe.CreatePtrArray(1);
    exe.m_pDataStringsArray[0] = (uintptr_t)MakeString("data");

    exe.m_dwDynamicVarsQuantity = 2;
    exe.m_pDynamicVarsArray = exe.CreateArray(2);
    exe.m_pDynamicVarsArrayType = exe.CreateArray(2);

    exe.m_dwUsertypeStringPatternQuantity = 4;
    exe.m_pUsertypeStringPatternArray = new char[4]();
}

} // namespace

TEST(EXEBlockMemoryTest, ClearNullsAllOwnedPointers) {
    CEXEBlock exe;
    PopulateExeBlock(exe);

    exe.Clear();

    EXPECT_EQ(exe.m_pInitialAppName, nullptr);
    EXPECT_EQ(exe.m_pDLLIndexArray, nullptr);
    EXPECT_EQ(exe.m_pDLLFilenameArray, nullptr);
    EXPECT_EQ(exe.m_pDLLLoadedAlreadyArray, nullptr);
    EXPECT_EQ(exe.m_pRefArray, nullptr);
    EXPECT_EQ(exe.m_pRefTypeArray, nullptr);
    EXPECT_EQ(exe.m_pRefIndexArray, nullptr);
    EXPECT_EQ(exe.m_pRuntimeErrorStringsArray, nullptr);
    EXPECT_EQ(exe.m_pCommandDLLIdArray, nullptr);
    EXPECT_EQ(exe.m_pCommandDLLCallArray, nullptr);
    EXPECT_EQ(exe.m_pStringsArray, nullptr);
    EXPECT_EQ(exe.m_pDataArray, nullptr);
    EXPECT_EQ(exe.m_pDataStringsArray, nullptr);
    EXPECT_EQ(exe.m_pDynamicVarsArray, nullptr);
    EXPECT_EQ(exe.m_pDynamicVarsArrayType, nullptr);
    EXPECT_EQ(exe.m_pUsertypeStringPatternArray, nullptr);
}

TEST(EXEBlockMemoryTest, ClearResetsAllElementCounters) {
    CEXEBlock exe;
    PopulateExeBlock(exe);

    exe.Clear();

    EXPECT_EQ(exe.m_dwNumberOfDLLs, 0U);
    EXPECT_EQ(exe.m_dwNumberOfReferences, 0U);
    EXPECT_EQ(exe.m_dwNumberOfCommands, 0U);
    EXPECT_EQ(exe.m_dwNumberOfStrings, 0U);
    // Regression: these four counters were previously left stale after
    // Clear(), letting FreeUptoDisplay() walk null arrays.
    EXPECT_EQ(exe.m_dwNumberOfRuntimeErrorStrings, 0U);
    EXPECT_EQ(exe.m_dwNumberOfDataItems, 0U);
    EXPECT_EQ(exe.m_dwDynamicVarsQuantity, 0U);
    EXPECT_EQ(exe.m_dwUsertypeStringPatternQuantity, 0U);
}

TEST(EXEBlockMemoryTest, ClearIsIdempotent) {
    CEXEBlock exe;
    PopulateExeBlock(exe);

    exe.Clear();
    exe.Clear(); // second Clear() must be a safe no-op

    EXPECT_EQ(exe.m_pDLLFilenameArray, nullptr);
    EXPECT_EQ(exe.m_dwNumberOfDLLs, 0U);
}

TEST(EXEBlockMemoryTest, DestructorAfterClearDoesNotDoubleFree) {
    // Destructor invokes Clear() again; must be safe after manual Clear().
    auto exe = std::make_unique<CEXEBlock>();
    PopulateExeBlock(*exe);
    exe->Clear();
    exe.reset(); // no crash / heap corruption expected
    SUCCEED();
}
