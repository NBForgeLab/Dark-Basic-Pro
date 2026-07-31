#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/TaskEmitter.h"

TEST(TaskEmitterTest, InitialStateIsClean) {
    CTaskEmitter emitter;
    EXPECT_EQ(emitter.GetTaskCount(), 0u);
}

TEST(TaskEmitterTest, DetermineASMCallResolvesTypes) {
    CTaskEmitter emitter;
    DWORD sizeCodeByte = emitter.DetermineASMCall(1, 4); // BYTE
    DWORD sizeCodeDword = emitter.DetermineASMCall(1, 8); // DWORDx2
    EXPECT_EQ(sizeCodeByte, 0u);
    EXPECT_EQ(sizeCodeDword, 3u);
}

TEST(TaskEmitterTest, ResetClearsState) {
    CTaskEmitter emitter;
    emitter.IncrementTaskCount();
    EXPECT_EQ(emitter.GetTaskCount(), 1u);
    emitter.Reset();
    EXPECT_EQ(emitter.GetTaskCount(), 0u);
}
