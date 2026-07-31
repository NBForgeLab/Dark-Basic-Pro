#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/TaskEmitter.h"

TEST(TaskEmitterCoreTest, ReturnsFalseWhenLineNumberIsZero) {
    CTaskEmitter emitter;
    EXPECT_FALSE(emitter.EmitCoreTask(0, 100));
}

TEST(TaskEmitterCoreTest, AcceptsValidTaskParameters) {
    CTaskEmitter emitter;
    EXPECT_TRUE(emitter.EmitCoreTask(10, 100));
}

TEST(TaskEmitterCoreTest, AcceptsModeParameters) {
    CTaskEmitter emitter;
    EXPECT_FALSE(emitter.EmitCoreTask(0, 100, 1, 2, 3));
    EXPECT_TRUE(emitter.EmitCoreTask(10, 100, 1, 2, 3));
}
