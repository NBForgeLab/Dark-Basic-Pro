#include <gtest/gtest.h>
#include <cstdint>
#include "../DBProCompiler/DBPCompiler/TaskEmitter.h"

TEST(TaskEmitterTest, InitialStateIsClean) {
    CTaskEmitter emitter;
    EXPECT_EQ(emitter.GetTaskCount(), 0u);
}

TEST(TaskEmitterTest, DetermineASMCallResolvesTypes) {
    CTaskEmitter emitter;
    constexpr uint32_t baseOpcode = 100;

    EXPECT_EQ(emitter.DetermineASMCall(baseOpcode, 4), baseOpcode);
    EXPECT_EQ(emitter.DetermineASMCall(baseOpcode, 6), baseOpcode + 1);
    EXPECT_EQ(emitter.DetermineASMCall(baseOpcode, 1), baseOpcode + 2);
    EXPECT_EQ(emitter.DetermineASMCall(baseOpcode, 8), baseOpcode + 3);
}

TEST(TaskEmitterTest, ResetClearsState) {
    CTaskEmitter emitter;
    emitter.IncrementTaskCount();
    EXPECT_EQ(emitter.GetTaskCount(), 1u);
    emitter.Reset();
    EXPECT_EQ(emitter.GetTaskCount(), 0u);
}

TEST(TaskEmitterTest, DetermineASMCallForRELResolvesTypes) {
    CTaskEmitter emitter;
    EXPECT_EQ(emitter.DetermineASMCallForREL(10, 104), 10u);
    EXPECT_EQ(emitter.DetermineASMCallForREL(10, 106), 11u);
    EXPECT_EQ(emitter.DetermineASMCallForREL(10, 107), 12u);
}

TEST(TaskEmitterTest, DetermineParamModeResolvesModes) {
    CTaskEmitter emitter;
    EXPECT_EQ(emitter.DetermineParamMode(nullptr, 0, 0), 0u); // ParamMode::None
}
