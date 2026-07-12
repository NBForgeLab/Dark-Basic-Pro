#include <gtest/gtest.h>
#include "DB3Task.h"
#include <thread>
#include <vector>
#include <atomic>

namespace db3 {

TEST(ConcurrencyTest, CLockMutualExclusion) {
    CLock lock;
    int sharedCounter = 0;
    const int numThreads = 10;
    const int incrementsPerThread = 1000;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&lock, &sharedCounter, incrementsPerThread]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                CAutolock autoLock(lock);
                sharedCounter++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(sharedCounter, numThreads * incrementsPerThread);
}

TEST(ConcurrencyTest, CEventWaitAndRaise) {
    CEvent event;
    std::atomic<bool> flag = false;

    std::thread t([&event, &flag]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        flag = true;
        event.Signal();
    });

    event.Sync();
    EXPECT_TRUE(flag);
    t.join();
}

static std::atomic<int> g_workCounter = 0;
static void TestWorkFunc(void* parm) {
    int val = (int)(uintptr_t)parm;
    g_workCounter += val;
}

TEST(ConcurrencyTest, CWorkQueueExecution) {
    g_workCounter = 0;
    CWorkQueue queue;
    ASSERT_TRUE(queue.Init(4));

    CSignal signal;
    signal.Reset(0);

    for (int i = 0; i < 20; ++i) {
        queue.Enqueue(TestWorkFunc, (void*)(uintptr_t)1, &signal);
    }

    // Wait for all tasks to complete
    signal.Sync();

    EXPECT_EQ(g_workCounter.load(), 20);
}

} // namespace db3
