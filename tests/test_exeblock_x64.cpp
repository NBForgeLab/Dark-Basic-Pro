#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/EXEBlock.h"

TEST(EXEBlockX64Test, MachineCodeBlockUsesByteAlignment) {
    CEXEBlock exeBlock;
    // Verify machine code block operates on byte array (uint8_t*)
    uint8_t* pMCB = exeBlock.GetMachineCodeBlockBytePointer();
    EXPECT_EQ(pMCB, nullptr);
}

TEST(EXEBlockX64Test, PointerArrayAllocationsSupport64BitPointers) {
    CEXEBlock exeBlock;
    uintptr_t* pArray = exeBlock.CreatePtrArray(10);
    ASSERT_NE(pArray, nullptr);
    uintptr_t dummyAddr = 0x7FFFFFFF12345678ULL;
    pArray[0] = dummyAddr;
    EXPECT_EQ(pArray[0], dummyAddr);
    delete[] pArray;
}
