#include <gtest/gtest.h>
#include <windows.h>
#include <cstdint>

// Helper functions mirroring DBDLLCore.cpp array index navigation
static inline void TestArrayIndexToTop(uintptr_t dwArrayPtr) {
    if (dwArrayPtr) *((DWORD*)dwArrayPtr - 1) = 0;
}

static inline void TestArrayIndexToBottom(uintptr_t dwArrayPtr) {
    if (dwArrayPtr) *((DWORD*)dwArrayPtr - 1) = *((DWORD*)dwArrayPtr - 4) - 1;
}

static inline void TestNextArrayIndex(uintptr_t dwArrayPtr) {
    if (dwArrayPtr) {
        *((DWORD*)dwArrayPtr - 1) = *((DWORD*)dwArrayPtr - 1) + 1;
        if (*((DWORD*)dwArrayPtr - 1) > *((DWORD*)dwArrayPtr - 4)) {
            *((DWORD*)dwArrayPtr - 1) = *((DWORD*)dwArrayPtr - 4);
        }
    }
}

static inline void TestPreviousArrayIndex(uintptr_t dwArrayPtr) {
    if (dwArrayPtr) {
        if ((int)*((DWORD*)dwArrayPtr - 1) > 0) {
            *((DWORD*)dwArrayPtr - 1) = (*((DWORD*)dwArrayPtr - 1)) - 1;
        } else {
            *((DWORD*)dwArrayPtr - 1) = (DWORD)-1;
        }
    }
}

static inline DWORD TestArrayIndexValid(uintptr_t dwArrayPtr) {
    if (dwArrayPtr) {
        if (*((DWORD*)dwArrayPtr - 1) < *((DWORD*)dwArrayPtr - 4))
            return 1;
        else
            return 0;
    } else {
        return 0;
    }
}

static inline DWORD TestArrayCount(uintptr_t dwArrayPtr) {
    if (dwArrayPtr)
        return (*((DWORD*)dwArrayPtr - 4)) - 1;
    else
        return (DWORD)-1;
}

// 1. Contract Test: PreviousArrayIndex does not underflow when index is already -1 (0xFFFFFFFF)
TEST(ArrayIndexTest, PreviousArrayIndexDoesNotUnderflowWhenAlreadyAtMinusOne) {
    DWORD header[14] = {0};
    header[10] = 10;         // Array size = 10
    header[13] = (DWORD)-1;  // Current index = -1

    uintptr_t dwArrayPtr = (uintptr_t)&header[14];

    TestPreviousArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), (DWORD)-1);

    // Call 5 times consecutively to verify stability
    for (int i = 0; i < 5; ++i) {
        TestPreviousArrayIndex(dwArrayPtr);
        EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), (DWORD)-1);
    }
}

// 2. Contract Test: PreviousArrayIndex decrements sequentially down to 0, then transitions to -1
TEST(ArrayIndexTest, PreviousArrayIndexDecrementsSequentiallyAndTransitionsToMinusOne) {
    DWORD header[14] = {0};
    header[10] = 5; // Array size = 5 (indices 0..4)
    header[13] = 3; // Start at index 3

    uintptr_t dwArrayPtr = (uintptr_t)&header[14];

    TestPreviousArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 2u);

    TestPreviousArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 1u);

    TestPreviousArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 0u);

    // Transition from 0 to -1
    TestPreviousArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), (DWORD)-1);

    // Further calls stay at -1
    TestPreviousArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), (DWORD)-1);
}

// 3. Contract Test: NextArrayIndex increments up to max count (dwSizeOfArray), then clamps
TEST(ArrayIndexTest, NextArrayIndexIncrementsAndClampsAtMaxCount) {
    DWORD header[14] = {0};
    header[10] = 4; // Array size = 4 (valid indices 0..3)
    header[13] = 2; // Start at index 2

    uintptr_t dwArrayPtr = (uintptr_t)&header[14];

    TestNextArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 3u);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 1u);

    TestNextArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 4u); // Clamped at dwSizeOfArray = 4 (out of bounds)
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 0u); // Invalid at index 4

    // Consecutively call NextArrayIndex; must stay clamped at 4
    for (int i = 0; i < 3; ++i) {
        TestNextArrayIndex(dwArrayPtr);
        EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 4u);
        EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 0u);
    }
}

// 4. Contract Test: ArrayIndexToTop and ArrayIndexToBottom positioning
TEST(ArrayIndexTest, ArrayIndexToTopAndBottomSetsCorrectIndices) {
    DWORD header[14] = {0};
    header[10] = 10; // Array size = 10 (valid indices 0..9)
    header[13] = 5;

    uintptr_t dwArrayPtr = (uintptr_t)&header[14];

    TestArrayIndexToTop(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 0u);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 1u);

    TestArrayIndexToBottom(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 9u);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 1u);

    EXPECT_EQ(TestArrayCount(dwArrayPtr), 9u);
}

// 5. Contract Test: Comprehensive navigation cycle (Top -> Next -> Bottom -> Previous -> Underflow -> Top)
TEST(ArrayIndexTest, FullNavigationCycleIsConsistentAndSafe) {
    DWORD header[14] = {0};
    header[10] = 3; // Array size = 3 (indices 0..2)
    uintptr_t dwArrayPtr = (uintptr_t)&header[14];

    // 1. Go to top (index 0)
    TestArrayIndexToTop(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 0u);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 1u);

    // 2. Next -> 1
    TestNextArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 1u);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 1u);

    // 3. Next -> 2
    TestNextArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 2u);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 1u);

    // 4. Next -> 3 (out of bounds)
    TestNextArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 3u);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 0u);

    // 5. Go to bottom -> 2
    TestArrayIndexToBottom(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 2u);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 1u);

    // 6. Previous -> 1 -> 0 -> -1
    TestPreviousArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 1u);
    TestPreviousArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 0u);
    TestPreviousArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), (DWORD)-1);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 0u);

    // 7. Previous again -> remains -1 (no underflow!)
    TestPreviousArrayIndex(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), (DWORD)-1);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 0u);
}

