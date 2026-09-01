#include <gtest/gtest.h>
#include <cstdint>
#include "../DBProCompiler/DBPCompiler/TaskEmitter.h"
#include "../DBProCompiler/DBPCompiler/ASMWriter.h"
#include "../DBProCompiler/DBPCompiler/Str.h"

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
    EXPECT_EQ(emitter.DetermineASMCall(baseOpcode, 8), baseOpcode + 2);
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
    EXPECT_EQ(emitter.DetermineParamMode(nullptr, 0, 0, nullptr), 0u); // ParamMode::None
}

TEST(TaskEmitterTest, ArrayTokensSplitByElementIndexPresence) {
    CTaskEmitter emitter;
    CStr indexToken("@$_TEMPIDX_");

    // Whole-handle operations (DIM/UNDIM/push) carry no element index.
    EXPECT_EQ(emitter.DetermineParamMode("@&arr", 107u, 0u, nullptr),
              static_cast<uint32_t>(ParamMode::Mem));
    EXPECT_EQ(emitter.DetermineParamMode("@:&arr", 107u, 0u, nullptr),
              static_cast<uint32_t>(ParamMode::Rbp));

    // Element accesses carry the linearized index in the additional-offset
    // token and must dereference through the direct-layout element data.
    EXPECT_EQ(emitter.DetermineParamMode("@&arr", 101u, 0u, &indexToken),
              static_cast<uint32_t>(ParamMode::MemArr));
    EXPECT_EQ(emitter.DetermineParamMode("@:&arr", 101u, 0u, &indexToken),
              static_cast<uint32_t>(ParamMode::RbpArr));
    // Field access on a UDT array element: type folds to 100+fieldtype.
    EXPECT_EQ(emitter.DetermineParamMode("@&udtarr", 103u, 8u, &indexToken),
              static_cast<uint32_t>(ParamMode::MemArr));
}
