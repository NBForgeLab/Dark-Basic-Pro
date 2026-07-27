#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include "EXEBlock.h"

// Characterization pins for the raw-memory utility layer of CEXEBlock
// (CreateArray / CreatePtrArray / RecreateArray). These lock the two
// observable properties a RAII refactor must preserve:
//   1. freshly created arrays are zero-initialised, and
//   2. RecreateArray keeps the existing prefix and zero-fills the growth.
// They are GREEN on the legacy hand-rolled new[]+loop implementation and
// must stay GREEN once the internals move to std::make_unique<T[]>.
//
// These utilities touch no globals, so a stack CEXEBlock is sufficient.

TEST(EXEBlockArrayTest, CreateArrayReturnsZeroInitialisedBlock) {
    CEXEBlock exe;
    const DWORD count = 8;
    DWORD* pArray = exe.CreateArray(count);
    ASSERT_NE(pArray, nullptr);
    for (DWORD i = 0; i < count; ++i)
        EXPECT_EQ(pArray[i], 0U) << "index " << i;
    delete[] pArray;
}

TEST(EXEBlockArrayTest, CreatePtrArrayReturnsZeroInitialisedBlock) {
    CEXEBlock exe;
    const DWORD count = 8;
    uintptr_t* pArray = exe.CreatePtrArray(count);
    ASSERT_NE(pArray, nullptr);
    for (DWORD i = 0; i < count; ++i)
        EXPECT_EQ(pArray[i], 0U) << "index " << i;
    delete[] pArray;
}

TEST(EXEBlockArrayTest, RecreateArrayDwordPreservesPrefixAndZeroFillsGrowth) {
    CEXEBlock exe;
    DWORD* pArray = exe.CreateArray(3);
    ASSERT_NE(pArray, nullptr);
    pArray[0] = 10;
    pArray[1] = 20;
    pArray[2] = 30;

    ASSERT_TRUE(exe.RecreateArray(&pArray, 3, 6));
    ASSERT_NE(pArray, nullptr);

    // Existing values preserved
    EXPECT_EQ(pArray[0], 10U);
    EXPECT_EQ(pArray[1], 20U);
    EXPECT_EQ(pArray[2], 30U);
    // Grown region zero-filled
    EXPECT_EQ(pArray[3], 0U);
    EXPECT_EQ(pArray[4], 0U);
    EXPECT_EQ(pArray[5], 0U);

    delete[] pArray;
}

TEST(EXEBlockArrayTest, RecreateArrayPtrPreservesPrefixAndZeroFillsGrowth) {
    CEXEBlock exe;
    uintptr_t* pArray = exe.CreatePtrArray(2);
    ASSERT_NE(pArray, nullptr);
    // Store plain sentinel integers (not real allocations) so we can inspect
    // preservation without invoking DeleteArrayContents.
    pArray[0] = 111;
    pArray[1] = 222;

    ASSERT_TRUE(exe.RecreateArray(&pArray, 2, 4));
    ASSERT_NE(pArray, nullptr);

    EXPECT_EQ(pArray[0], 111U);
    EXPECT_EQ(pArray[1], 222U);
    EXPECT_EQ(pArray[2], 0U);
    EXPECT_EQ(pArray[3], 0U);

    delete[] pArray;
}

TEST(EXEBlockArrayTest, RecreateArrayRejectsNullOwner) {
    CEXEBlock exe;
    // Soft-fail contract: a null owning pointer-to-pointer returns false.
    EXPECT_FALSE(exe.RecreateArray((DWORD**)nullptr, 0, 4));
    EXPECT_FALSE(exe.RecreateArray((uintptr_t**)nullptr, 0, 4));
}
