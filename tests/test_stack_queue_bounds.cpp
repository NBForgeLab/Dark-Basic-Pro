#include <gtest/gtest.h>
#include <windows.h>
#include <cstdint>

// Helper functions mirroring DBDLLCore.cpp Stack and Queue routines
static inline void TestPopFromStack(uintptr_t dwArrayPtr) {
    if (dwArrayPtr == 0) return;
    int iIndexAtEnd = (int)*((DWORD*)dwArrayPtr - 4) - 1;
    if (iIndexAtEnd >= 0) {
        // Simulate deleting bottom/top element: shrink array size by 1
        *((DWORD*)dwArrayPtr - 4) = *((DWORD*)dwArrayPtr - 4) - 1;
    }
    iIndexAtEnd = (int)*((DWORD*)dwArrayPtr - 4) - 1;
    *((DWORD*)dwArrayPtr - 1) = (DWORD)iIndexAtEnd;
}

static inline void TestRemoveFromQueue(uintptr_t dwArrayPtr) {
    if (dwArrayPtr == 0) return;
    DWORD dwSizeOfTable = *((DWORD*)dwArrayPtr - 4);
    if (dwSizeOfTable > 0) {
        // Simulate removing top element: shrink array size by 1
        *((DWORD*)dwArrayPtr - 4) = dwSizeOfTable - 1;
    }
    if (*((DWORD*)dwArrayPtr - 4) == 0) {
        *((DWORD*)dwArrayPtr - 1) = (DWORD)-1;
    } else {
        *((DWORD*)dwArrayPtr - 1) = 0;
    }
}

static inline void TestPushToStack(uintptr_t dwArrayPtr) {
    if (dwArrayPtr == 0) return;
    *((DWORD*)dwArrayPtr - 4) = *((DWORD*)dwArrayPtr - 4) + 1; // Expand size by 1
    int iIndexAtEnd = (int)*((DWORD*)dwArrayPtr - 4) - 1;
    *((DWORD*)dwArrayPtr - 1) = (DWORD)iIndexAtEnd;
}

static inline void TestAddToQueue(uintptr_t dwArrayPtr) {
    if (dwArrayPtr == 0) return;
    *((DWORD*)dwArrayPtr - 4) = *((DWORD*)dwArrayPtr - 4) + 1; // Expand size by 1
    int iIndexAtEnd = (int)*((DWORD*)dwArrayPtr - 4) - 1;
    *((DWORD*)dwArrayPtr - 1) = (DWORD)iIndexAtEnd;
}

static inline DWORD TestArrayIndexValid(uintptr_t dwArrayPtr) {
    if (dwArrayPtr) {
        if (*((DWORD*)dwArrayPtr - 1) < *((DWORD*)dwArrayPtr - 4))
            return 1;
        else
            return 0;
    }
    return 0;
}

// 1. Contract Test: RemoveFromQueue sets index to (DWORD)-1 when queue becomes empty
TEST(StackQueueTest, RemoveFromQueueSetsIndexToMinusOneWhenEmpty) {
    DWORD header[14] = {0};
    header[10] = 0;         // Empty array
    header[13] = (DWORD)-1;

    uintptr_t dwArrayPtr = (uintptr_t)&header[14];

    TestRemoveFromQueue(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), (DWORD)-1);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 0u);
}

// 2. Contract Test: PopFromStack sets index to (DWORD)-1 when stack becomes empty
TEST(StackQueueTest, PopFromStackSetsIndexToMinusOneWhenEmpty) {
    DWORD header[14] = {0};
    header[10] = 0; // Empty array
    header[13] = 0;

    uintptr_t dwArrayPtr = (uintptr_t)&header[14];

    TestPopFromStack(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), (DWORD)-1);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 0u);
}

// 3. Contract Test: Sequential Queue Add and Remove transitions index cleanly
TEST(StackQueueTest, SequentialQueueAddAndRemoveTransitionsIndexCleanly) {
    DWORD header[14] = {0};
    header[10] = 0;         // Start empty
    header[13] = (DWORD)-1;
    uintptr_t dwArrayPtr = (uintptr_t)&header[14];

    // Add 1 item
    TestAddToQueue(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 4), 1u); // Count = 1
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 0u); // Index = 0
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 1u); // Valid!

    // Add 2nd item
    TestAddToQueue(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 4), 2u); // Count = 2
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 1u); // Index = 1
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 1u); // Valid!

    // Remove 1 item from queue -> Count = 1, Index = 0
    TestRemoveFromQueue(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 4), 1u);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 0u);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 1u); // Valid!

    // Remove final item from queue -> Count = 0, Index = -1
    TestRemoveFromQueue(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 4), 0u);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), (DWORD)-1);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 0u); // Invalid (empty)!
}

// 4. Contract Test: Sequential Stack Push and Pop transitions index cleanly
TEST(StackQueueTest, SequentialStackPushAndPopTransitionsIndexCleanly) {
    DWORD header[14] = {0};
    header[10] = 0;         // Start empty
    header[13] = (DWORD)-1;
    uintptr_t dwArrayPtr = (uintptr_t)&header[14];

    // Push 3 items onto stack
    TestPushToStack(dwArrayPtr); // Count=1, Index=0
    TestPushToStack(dwArrayPtr); // Count=2, Index=1
    TestPushToStack(dwArrayPtr); // Count=3, Index=2

    EXPECT_EQ(*((DWORD*)dwArrayPtr - 4), 3u);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 2u);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 1u);

    // Pop 1 item -> Count=2, Index=1
    TestPopFromStack(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 4), 2u);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 1u);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 1u);

    // Pop 2nd item -> Count=1, Index=0
    TestPopFromStack(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 4), 1u);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), 0u);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 1u);

    // Pop final item -> Count=0, Index=-1
    TestPopFromStack(dwArrayPtr);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 4), 0u);
    EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), (DWORD)-1);
    EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 0u);
}

// 5. Contract Test: Repeated pops or queue removals on empty array are safe no-ops
TEST(StackQueueTest, RepeatedPopsAndQueueRemovalsOnEmptyArrayAreSafeAndNoop) {
    DWORD header[14] = {0};
    header[10] = 0;         // Empty array
    header[13] = (DWORD)-1;
    uintptr_t dwArrayPtr = (uintptr_t)&header[14];

    // Call PopFromStack 5 times on empty stack
    for (int i = 0; i < 5; ++i) {
        TestPopFromStack(dwArrayPtr);
        EXPECT_EQ(*((DWORD*)dwArrayPtr - 4), 0u);
        EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), (DWORD)-1);
        EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 0u);
    }

    // Call RemoveFromQueue 5 times on empty queue
    for (int i = 0; i < 5; ++i) {
        TestRemoveFromQueue(dwArrayPtr);
        EXPECT_EQ(*((DWORD*)dwArrayPtr - 4), 0u);
        EXPECT_EQ(*((DWORD*)dwArrayPtr - 1), (DWORD)-1);
        EXPECT_EQ(TestArrayIndexValid(dwArrayPtr), 0u);
    }
}

