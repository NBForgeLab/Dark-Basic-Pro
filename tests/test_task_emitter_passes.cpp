#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/TaskEmitter.h"

TEST(TaskEmitterPassesTest, CalculatesPassOffsetsCorrectly) {
    CTaskEmitter emitter;
    EXPECT_EQ(emitter.CalculateTaskPassOffset(1, 100), 100u);
    EXPECT_EQ(emitter.CalculateTaskPassOffset(2, 100), 200u);
    EXPECT_EQ(emitter.CalculateTaskPassOffset(3, 100), 300u);
    EXPECT_EQ(emitter.CalculateTaskPassOffset(0, 100), 0u);
}
