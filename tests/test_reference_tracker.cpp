#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/ReferenceTracker.h"

TEST(ReferenceTrackerTest, InitialStateIsEmpty) {
    CReferenceTracker tracker;
    EXPECT_EQ(tracker.GetRefPointer(), 0u);
    EXPECT_GE(tracker.GetRefBufferSize(), 1024u);
}

TEST(ReferenceTrackerTest, AddReferenceIncrementsPointer) {
    CReferenceTracker tracker;
    tracker.AddReference(0x1000, 0x2000);
    EXPECT_EQ(tracker.GetRefPointer(), 1u);
    EXPECT_EQ(tracker.GetRef(0), 0x1000u);
    EXPECT_EQ(tracker.GetRefLabel(0), 0x2000u);
}

TEST(ReferenceTrackerTest, BufferExpandsWhenThresholdReached) {
    CReferenceTracker tracker;
    DWORD initialSize = tracker.GetRefBufferSize();
    
    // Fill until threshold (within 100 bytes of capacity)
    for (size_t i = 0; i < initialSize - 50; ++i) {
        tracker.AddReference(static_cast<DWORD>(i), static_cast<DWORD>(i * 2));
    }
    
    EXPECT_GT(tracker.GetRefBufferSize(), initialSize);
    EXPECT_EQ(tracker.GetRefPointer(), initialSize - 50);
}

TEST(ReferenceTrackerTest, ResetClearsPointer) {
    CReferenceTracker tracker;
    tracker.AddReference(10, 20);
    tracker.AddReference(30, 40);
    EXPECT_EQ(tracker.GetRefPointer(), 2u);

    tracker.Reset();
    EXPECT_EQ(tracker.GetRefPointer(), 0u);
}

TEST(ReferenceTrackerTest, BoundsCheckingReturnsZeroOnOutOfBounds) {
    CReferenceTracker tracker;
    tracker.AddReference(100, 200);
    EXPECT_EQ(tracker.GetRef(0), 100u);
    EXPECT_EQ(tracker.GetRef(9999), 0u);
    EXPECT_EQ(tracker.GetRefLabel(9999), 0u);
}
