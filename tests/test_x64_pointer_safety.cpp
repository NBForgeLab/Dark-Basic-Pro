#include <gtest/gtest.h>
#include "EXEBlock.h"
#include <cstdint>

TEST(PointerSafetyTest, UintptrPreservesFull64BitAddress) {
    // Simulated high-address pointer (> 4GB limit, 64-bit address)
    uint64_t highAddress64 = 0x7FFF000088889999ULL;
    void* pDummy = reinterpret_cast<void*>(static_cast<uintptr_t>(highAddress64));

    uintptr_t stored = reinterpret_cast<uintptr_t>(pDummy);
    
    #if defined(_WIN64) || defined(__x86_64__)
    EXPECT_EQ(stored, highAddress64);
    #else
    EXPECT_NE(stored, 0U);
    #endif
}

TEST(EXEBlockPointerSafetyTest, StringArrayAllocatesUintptrMemory) {
    CEXEBlock exeBlock;
    uintptr_t* pArray = nullptr;

    // Simulate allocating string array
    pArray = reinterpret_cast<uintptr_t*>(exeBlock.CreateArray(5));
    ASSERT_NE(pArray, nullptr);

    uint64_t mockPtr = 0x123456789ABCDEF0ULL;
    pArray[0] = static_cast<uintptr_t>(mockPtr);

    #if defined(_WIN64)
    EXPECT_EQ(pArray[0], mockPtr);
    #else
    EXPECT_NE(pArray[0], 0U);
    #endif

    delete[] pArray;
}

TEST(EXEBlockPointerSafetyTest, RecreateAndCleanArrayContents) {
    CEXEBlock exeBlock;
    uintptr_t* pArray = exeBlock.CreatePtrArray(3);
    ASSERT_NE(pArray, nullptr);

    // Allocate dynamic strings
    pArray[0] = reinterpret_cast<uintptr_t>(new char[16]{"test1"});
    pArray[1] = reinterpret_cast<uintptr_t>(new char[16]{"test2"});
    pArray[2] = 0;

    // Recreate array to size 5
    bool recreated = exeBlock.RecreateArray(&pArray, 3, 5);
    EXPECT_TRUE(recreated);
    ASSERT_NE(pArray, nullptr);

    // Clean contents safely
    exeBlock.DeleteArrayContents(pArray, 5);
    EXPECT_EQ(pArray[0], 0U);
    EXPECT_EQ(pArray[1], 0U);

    delete[] pArray;
}

TEST(PointerSafetyTest, HighAddressPointerRoundTrip) {
    uint64_t highAddress = 0x7FFF0000FFFF0000ULL;
    uintptr_t converted = static_cast<uintptr_t>(highAddress);
    void* ptr = reinterpret_cast<void*>(converted);
    uintptr_t back = reinterpret_cast<uintptr_t>(ptr);
    #if defined(_WIN64) || defined(__x86_64__)
    EXPECT_EQ(back, highAddress);
    #else
    EXPECT_NE(back, 0U);
    #endif
}


