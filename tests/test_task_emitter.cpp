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

    // These carry no element index token at all (DIM/UNDIM/push).
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

TEST(TaskEmitterTest, ArrayHandleParamTaggedAsAddressEmitsTheWholeHandle) {
    CTaskEmitter emitter;
    CStr indexToken("@$_TEMPIDX_");

    // An 'H' command parameter asks for the array base ADDRESS, so the caster
    // retags the operand as type 7. The "(0)" in the source is mandatory syntax,
    // not an element selection: the runtime reads the 56-byte header at negative
    // offsets from the handle and copies size*itemSize bytes forward from it.
    EXPECT_EQ(emitter.DetermineParamMode("@&arr", 7u, 0u, &indexToken),
              static_cast<uint32_t>(ParamMode::Mem));
    EXPECT_EQ(emitter.DetermineParamMode("@:&arr", 7u, 0u, &indexToken),
              static_cast<uint32_t>(ParamMode::Rbp));

    // Genuine element types must keep dereferencing through the element data.
    EXPECT_EQ(emitter.DetermineParamMode("@&arr", 101u, 0u, &indexToken),
              static_cast<uint32_t>(ParamMode::MemArr));
    EXPECT_EQ(emitter.DetermineParamMode("@:&arr", 101u, 0u, &indexToken),
              static_cast<uint32_t>(ParamMode::RbpArr));
}

// DetermineASMCallForREL selects the width for the relative-address family used
// by the array-element paths: WriteASMARRtoRAX's default arm (TaskEmitter.cpp:207
// and :208) and WriteASMRAXtoARR's default arm (TaskEmitter.cpp:507). When the
// element value is itself a pointer, the 4-byte variant truncates it to a DWORD
// on x64 -- which is how a string member of a UDT array element reached
// EquateSS() as 0x00000000FFFFFFFF.
TEST(TaskEmitterTest, DetermineASMCallForRELWidensPointerValuedElements) {
    CTaskEmitter emitter;
    constexpr uint32_t pointerElementTypes[] = {
        static_cast<uint32_t>(DBPType::String),              // 3
        static_cast<uint32_t>(DBPType::StringArray),         // 103
        static_cast<uint32_t>(DBPType::UserDefinedPtr),      // 1001
        static_cast<uint32_t>(DBPType::UserDefinedArrayPtr), // 1101
    };

    for (const uint32_t type : pointerElementTypes) {
        EXPECT_EQ(emitter.DetermineASMCallForREL(static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF1), type),
                  static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF8)) << "type=" << type;
        EXPECT_EQ(emitter.DetermineASMCallForREL(static_cast<uint32_t>(ASMOp::MOVRAXRCX1), type),
                  static_cast<uint32_t>(ASMOp::MOVRAXRCX8)) << "type=" << type;
        EXPECT_EQ(emitter.DetermineASMCallForREL(static_cast<uint32_t>(ASMOp::MOVRAXOFFRCX1), type),
                  static_cast<uint32_t>(ASMOp::MOVRAXOFFRCX8)) << "type=" << type;
    }
}

// The widening above must not over-fire: a numeric array element holds a value,
// not a pointer, and its 1/2/4-byte width is correct.
TEST(TaskEmitterTest, DetermineASMCallForRELKeepsNarrowWidthForNumericArrayElements) {
    CTaskEmitter emitter;
    const uint32_t load = static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF1);

    EXPECT_EQ(emitter.DetermineASMCallForREL(load, static_cast<uint32_t>(DBPType::BooleanArray)),
              static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF1));
    EXPECT_EQ(emitter.DetermineASMCallForREL(load, static_cast<uint32_t>(DBPType::ByteArray)),
              static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF1));
    EXPECT_EQ(emitter.DetermineASMCallForREL(load, static_cast<uint32_t>(DBPType::WordArray)),
              static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF2));
    EXPECT_EQ(emitter.DetermineASMCallForREL(load, static_cast<uint32_t>(DBPType::IntegerArray)),
              static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF4));
    EXPECT_EQ(emitter.DetermineASMCallForREL(load, static_cast<uint32_t>(DBPType::FloatArray)),
              static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF4));
    EXPECT_EQ(emitter.DetermineASMCallForREL(load, static_cast<uint32_t>(DBPType::DwordArray)),
              static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF4));
    EXPECT_EQ(emitter.DetermineASMCallForREL(load, static_cast<uint32_t>(DBPType::DoubleFloatArray)),
              static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF4));
    EXPECT_EQ(emitter.DetermineASMCallForREL(load, static_cast<uint32_t>(DBPType::DoubleIntegerArray)),
              static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF4));
}
