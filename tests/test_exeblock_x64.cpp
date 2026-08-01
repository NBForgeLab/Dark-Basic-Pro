#include <gtest/gtest.h>
#include <limits>

#include "EXEBlock.h"

TEST(EXEBlockPointerStorageTest, MachineCodeBlockUsesByteAlignment) {
    CEXEBlock exeBlock;
    // Verify machine code block operates on byte array (uint8_t*)
    uint8_t* pMCB = exeBlock.GetMachineCodeBlockBytePointer();
    EXPECT_EQ(pMCB, nullptr);
}

TEST(EXEBlockPointerStorageTest, PointerArrayPreservesHostPointerValues) {
    CEXEBlock exeBlock;
    uintptr_t* pArray = exeBlock.CreatePtrArray(10);
    ASSERT_NE(pArray, nullptr);
    const uintptr_t dummyAddr = (std::numeric_limits<uintptr_t>::max)() - 0x1234u;
    pArray[0] = dummyAddr;
    EXPECT_EQ(pArray[0], dummyAddr);
    delete[] pArray;
}
