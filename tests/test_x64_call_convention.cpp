// test_x64_call_convention.cpp
//
// Wave 3 x64 calling-convention tests (TDD): the ASMTask::Call emission must
// follow the Microsoft x64 ABI — integer args in RCX/RDX/R8/R9, floats in
// XMM0-3, 32-byte shadow space, 16-byte RSP alignment at the CALL, stack
// args above the shadow space, caller cleanup consumed by the Call task and
// the matching cleanup pops suppressed.
//
// Design: docs/superpowers/specs/2026-08-11-x64-call-convention-design.md

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <windows.h>

#include "DBPLogger.h"
#include "ASMWriter.h"
#include "EXEBlock.h"
#include "ReferenceTracker.h"
#include "Statement.h"
#include "StatementList.h"
#include "StructTable.h"
#include "Error.h"

#include "CompilerContext.h"
#include "DebuggerInterface.h"

// Declare compiler global pointers defined in dbp_compiler_lib
extern CStructTable*     g_pStructTable;
extern CStatementList*   g_pStatementList;
extern ICodeGenerator*   g_pASMWriter;
extern CError*           g_pErrorReport;

namespace
{
std::vector<uint8_t> AsBytes(const char* raw, std::size_t length)
{
    const auto* bytes = reinterpret_cast<const uint8_t*>(raw);
    return std::vector<uint8_t>(bytes, bytes + length);
}
} // namespace

class X64CallConventionTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_call_convention.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();
        g_pStructTable->SetStructDefaults();

        char dummyProg[] = "";
        g_pStatementList->MakeStatements(dummyProg, 0);
        ASSERT_TRUE(g_pASMWriter->CreateASMHeader());
        m_pWriter = static_cast<CASMWriter*>(g_pASMWriter);

        // Real program prologue: 7 explicit pushes (PUSHAD expansion).
        // Entry RSP%16==8 (C call) + 56 bytes -> body starts at 0.
        ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
            0u, static_cast<DWORD>(ASMTask::PushRegisters), nullptr, 0u));
    }

    void TearDown() override {
        if (m_pContext) {
            m_pContext->Cleanup();
            delete m_pContext;
            m_pContext = nullptr;
        }
        spdlog::shutdown();
    }

    // Bytes written by the last operation (between the recorded positions).
    std::vector<uint8_t> BytesSince(std::size_t before) const {
        const auto after = m_pWriter->GetCurrentMCPosition();
        const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
        return AsBytes(raw + before, after - before);
    }

    // Pushes one value via the production ASMTask::Push path.
    void EmitPush(DWORD type, const char* value) {
        CStr val(const_cast<char*>(value));
        ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
            1u, static_cast<DWORD>(ASMTask::Push), &val, type));
    }

    // Emits the DLL call task and returns its bytes.
    std::vector<uint8_t> EmitCall() {
        const auto before = m_pWriter->GetCurrentMCPosition();
        CStr call("@mycore.dll,@myfunc");
        EXPECT_TRUE(m_pWriter->WriteASMTaskCoreP2(
            1u, static_cast<DWORD>(ASMTask::Call), &call, 0, nullptr, 0));
        return BytesSince(before);
    }

    // The command-address load + CALL RBX tail shared by every call.
    std::vector<uint8_t> CallTailBytes() const {
        // MOV RBX, imm64 (unpatched command-address reference: zero slot)
        // + CALL RBX
        std::vector<uint8_t> b{0x48, 0xBB, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0xFF, 0xD3};
        return b;
    }

    static bool EndsWith(const std::vector<uint8_t>& bytes,
                         const std::vector<uint8_t>& suffix) {
        if (bytes.size() < suffix.size()) return false;
        return std::equal(suffix.begin(), suffix.end(),
                          bytes.end() - suffix.size());
    }

    static bool Contains(const std::vector<uint8_t>& bytes,
                         const std::vector<uint8_t>& needle) {
        return std::search(bytes.begin(), bytes.end(),
                           needle.begin(), needle.end()) != bytes.end();
    }
};

// ---------------------------------------------------------------------------
// Call paths
// ---------------------------------------------------------------------------

// A call with no arguments still allocates the 32-byte shadow space and
// restores the stack afterwards (Microsoft x64 ABI requirement).
TEST_F(X64CallConventionTest, ZeroArgCallAllocatesShadowSpace) {
    const auto call = EmitCall();
    // SUB RSP, 32 ; MOV RBX,[idx] ; CALL RBX ; ADD RSP, 32
    ASSERT_EQ(call.size(), 4u + 12u + 4u);
    EXPECT_EQ(call[0], 0x48); EXPECT_EQ(call[1], 0x83); EXPECT_EQ(call[2], 0xEC);
    EXPECT_EQ(call[3], 0x20); // 32
    EXPECT_TRUE(Contains(call, CallTailBytes()));
    EXPECT_EQ(call[call.size() - 4], 0x48); // ADD RSP, 32 tail
}

// One integer argument goes to RCX from the shadow-space-relative slot.
// (One push puts RSP%16==8, so the frame is padded to 40 bytes.)
TEST_F(X64CallConventionTest, OneIntArgGoesToRcx) {
    EmitPush(1, "7");
    const auto call = EmitCall();
    // SUB RSP,40; MOV RCX,[RSP+40]; MOV RBX,[idx]; CALL RBX; ADD RSP,40
    ASSERT_GE(call.size(), 25u);
    EXPECT_EQ(call[1], 0x83); EXPECT_EQ(call[3], 0x28); // SUB RSP,40
    EXPECT_EQ(call[4], 0x48); EXPECT_EQ(call[5], 0x8B); EXPECT_EQ(call[6], 0x4C);
    EXPECT_EQ(call[7], 0x24); EXPECT_EQ(call[8], 0x28); // RCX,[RSP+40]
    EXPECT_TRUE(Contains(call, CallTailBytes()));
}

// Two integer arguments: arg1 (pushed last) -> RCX, arg2 -> RDX.
TEST_F(X64CallConventionTest, TwoIntArgsGoToRcxRdx) {
    EmitPush(1, "8"); // arg2 (pushed first)
    EmitPush(1, "7"); // arg1 (pushed last -> frame top)
    const auto call = EmitCall();
    // SUB RSP,32; MOV RCX,[RSP+32]; MOV RDX,[RSP+40]; tail; ADD RSP,32
    ASSERT_GE(call.size(), 30u);
    EXPECT_EQ(call[4], 0x48); EXPECT_EQ(call[5], 0x8B); EXPECT_EQ(call[6], 0x4C);
    EXPECT_EQ(call[7], 0x24); EXPECT_EQ(call[8], 0x20); // RCX,[RSP+32]
    EXPECT_EQ(call[9], 0x48); EXPECT_EQ(call[10], 0x8B); EXPECT_EQ(call[11], 0x54);
    EXPECT_EQ(call[12], 0x24); EXPECT_EQ(call[13], 0x28); // RDX,[RSP+40]
    EXPECT_TRUE(Contains(call, CallTailBytes()));
}

// Four arguments use the full integer register file.
TEST_F(X64CallConventionTest, FourIntArgsUseAllRegisters) {
    EmitPush(1, "11"); // arg4
    EmitPush(1, "10"); // arg3
    EmitPush(1, "9");  // arg2
    EmitPush(1, "7");  // arg1 (frame top)
    const auto call = EmitCall();
    // MOV RCX,[RSP+32]; MOV RDX,[RSP+40]; MOV R8,[RSP+48]; MOV R9,[RSP+56]
    ASSERT_GE(call.size(), 40u);
    EXPECT_EQ(call[4], 0x48); EXPECT_EQ(call[6], 0x4C); EXPECT_EQ(call[8], 0x20);
    EXPECT_EQ(call[9], 0x48); EXPECT_EQ(call[11], 0x54); EXPECT_EQ(call[13], 0x28);
    EXPECT_EQ(call[14], 0x49); EXPECT_EQ(call[15], 0x8B); EXPECT_EQ(call[16], 0x44);
    EXPECT_EQ(call[18], 0x30); // R8,[RSP+48]
    EXPECT_EQ(call[19], 0x49); EXPECT_EQ(call[20], 0x8B); EXPECT_EQ(call[21], 0x4C);
    EXPECT_EQ(call[23], 0x38); // R9,[RSP+56]
    EXPECT_TRUE(Contains(call, CallTailBytes()));
}

// The fifth argument is moved to [RSP+32] above the shadow space.
TEST_F(X64CallConventionTest, FifthArgGoesToStack) {
    for (int i = 0; i < 5; ++i) EmitPush(1, "5");
    const auto call = EmitCall();
    // SUB RSP,40; regs; MOV RAX,[RSP+72]; MOV [RSP+32],RAX; tail; ADD RSP,40
    EXPECT_EQ(call[2], 0xEC); EXPECT_EQ(call[3], 0x28); // SUB RSP,40
    // arg5 (pushed first, deepest, base 32) -> src [RSP+72], dest [RSP+32]
    const std::vector<uint8_t> move{0x48, 0x8B, 0x44, 0x24, 0x48,  // MOV RAX,[RSP+72]
                                    0x48, 0x89, 0x44, 0x24, 0x20}; // MOV [RSP+32],RAX
    EXPECT_TRUE(std::search(call.begin(), call.end(),
                            move.begin(), move.end()) != call.end());
    EXPECT_TRUE(Contains(call, CallTailBytes()));
}

// 16-byte alignment: an odd number of pushes puts RSP%16==8, so the call
// pads the frame by 8 (F = 40) to keep RSP aligned at the CALL.
TEST_F(X64CallConventionTest, OddPushCountPadsFrameToAlignRsp) {
    EmitPush(1, "7"); // one push: RSP%16 == 8 at the call site
    const auto call = EmitCall();
    EXPECT_EQ(call[1], 0x83); EXPECT_EQ(call[2], 0xEC); EXPECT_EQ(call[3], 0x28); // SUB RSP,40
    EXPECT_EQ(call[4], 0x48); EXPECT_EQ(call[5], 0x8B); EXPECT_EQ(call[6], 0x4C);
    EXPECT_EQ(call[8], 0x28); // RCX,[RSP+40]
    EXPECT_TRUE(Contains(call, CallTailBytes()));
    // ADD RSP, 40 restores (48 83 C4 28 at the end)
    EXPECT_TRUE(Contains(call, std::vector<uint8_t>{0x48, 0x83, 0xC4, 0x28}));
}

// Even push counts keep the frame at 32 with no padding.
TEST_F(X64CallConventionTest, EvenPushCountNoPadding) {
    EmitPush(1, "8");
    EmitPush(1, "7");
    const auto call = EmitCall();
    EXPECT_EQ(call[3], 0x20); // SUB RSP,32
    EXPECT_EQ(call[call.size() - 1], 0x20); // ADD RSP,32 (last byte)
}

// A float (type 2) argument is passed in XMM0 (MOVSS from the frame slot).
TEST_F(X64CallConventionTest, FloatArgGoesToXmm0) {
    EmitPush(2, "2.5");
    const auto call = EmitCall();
    // SUB RSP,40; MOVSS XMM0,[RSP+40]; tail; ADD RSP,40
    ASSERT_GE(call.size(), 26u);
    EXPECT_EQ(call[4], 0xF3); EXPECT_EQ(call[5], 0x0F); EXPECT_EQ(call[6], 0x10);
    EXPECT_EQ(call[7], 0x44); EXPECT_EQ(call[8], 0x24); EXPECT_EQ(call[9], 0x28);
    EXPECT_TRUE(Contains(call, CallTailBytes()));
}

// Mixed float/integer args use the positional ABI slots: arg1 float -> XMM0,
// arg2 integer -> RDX.
TEST_F(X64CallConventionTest, MixedFloatIntUsePositionalSlots) {
    EmitPush(1, "9");  // arg2 integer (pushed first)
    EmitPush(2, "1.5"); // arg1 float (frame top)
    const auto call = EmitCall();
    // MOVSS XMM0,[RSP+32]; MOV RDX,[RSP+40]
    ASSERT_GE(call.size(), 30u);
    EXPECT_EQ(call[4], 0xF3); EXPECT_EQ(call[6], 0x10); EXPECT_EQ(call[9], 0x20); // XMM0,[RSP+32]
    EXPECT_EQ(call[10], 0x48); EXPECT_EQ(call[11], 0x8B); EXPECT_EQ(call[12], 0x54);
    EXPECT_EQ(call[14], 0x28); // RDX,[RSP+40]
    EXPECT_TRUE(Contains(call, CallTailBytes()));
}

// A double (type 8) is passed in XMM0 with the two 4-byte halves reassembled
// (the frame holds low@slot, high@slot+8 with a zero gap on x64 pushes).
TEST_F(X64CallConventionTest, DoubleArgReassemblesIntoXmm0) {
    EmitPush(8, "2.5");
    const auto call = EmitCall();
    // SUB RSP,32
    EXPECT_EQ(call[3], 0x20);
    // MOV RAX,[RSP+32]; MOV RCX,[RSP+40]; SHL RCX,32; OR RAX,RCX; MOVQ XMM0,RAX
    const std::vector<uint8_t> reassemble{
        0x48, 0x8B, 0x44, 0x24, 0x20,           // MOV RAX,[RSP+32]
        0x48, 0x8B, 0x4C, 0x24, 0x28,           // MOV RCX,[RSP+40]
        0x48, 0xC1, 0xE1, 0x20,                 // SHL RCX,32
        0x48, 0x09, 0xC8,                       // OR RAX,RCX
        0x66, 0x48, 0x0F, 0x6E, 0xC0,           // MOVQ XMM0,RAX
    };
    EXPECT_TRUE(std::search(call.begin(), call.end(),
                            reassemble.begin(), reassemble.end()) != call.end());
    EXPECT_TRUE(Contains(call, CallTailBytes()));
}

// A double stack arg (arg 5) is reassembled and stored at [RSP+32].
TEST_F(X64CallConventionTest, FifthArgDoubleGoesToStackReassembled) {
    EmitPush(8, "2.5"); // arg5 double (pushed first, deepest)
    for (int i = 0; i < 4; ++i) EmitPush(1, "1"); // args 4..1
    const auto call = EmitCall();
    // 6 pushes -> RSP%16==0 -> F = 32+8+8 = 48 (pad 8 keeps 16-alignment).
    // arg5 base 32 -> low [RSP+80], high [RSP+88]; dest [RSP+32].
    const std::vector<uint8_t> reassemble{
        0x48, 0x8B, 0x44, 0x24, 0x50,           // MOV RAX,[RSP+80]
        0x48, 0x8B, 0x4C, 0x24, 0x58,           // MOV RCX,[RSP+88]
        0x48, 0xC1, 0xE1, 0x20,                 // SHL RCX,32
        0x48, 0x09, 0xC8,                       // OR RAX,RCX
        0x48, 0x89, 0x44, 0x24, 0x20,           // MOV [RSP+32],RAX
    };
    EXPECT_TRUE(std::search(call.begin(), call.end(),
                            reassemble.begin(), reassemble.end()) != call.end());
    EXPECT_TRUE(Contains(call, CallTailBytes()));
}

// An int64 (type 9) argument rides a single 8-byte slot and is moved
// directly into RCX (the MS x64 ABI: __int64 occupies one register).
// Wave 8b removed the legacy two-4-byte-half reassembly.
TEST_F(X64CallConventionTest, Int64ArgGoesToRcxSingleSlot) {
    EmitPush(9, "1234567890123");
    const auto call = EmitCall();
    // SUB RSP,40 (odd push count pads to align); MOV RCX,[RSP+40]
    ASSERT_GE(call.size(), 25u);
    EXPECT_EQ(call[1], 0x83); EXPECT_EQ(call[3], 0x28); // SUB RSP,40
    EXPECT_EQ(call[4], 0x48); EXPECT_EQ(call[5], 0x8B); EXPECT_EQ(call[6], 0x4C);
    EXPECT_EQ(call[7], 0x24); EXPECT_EQ(call[8], 0x28); // RCX,[RSP+40]
    EXPECT_TRUE(Contains(call, CallTailBytes()));
    // The old two-half reassembly must be gone.
    const std::vector<uint8_t> oldReassemble{
        0x48, 0xC1, 0xE1, 0x20, 0x48, 0x09, 0xC8, 0x48, 0x89, 0xC1};
    EXPECT_FALSE(std::search(call.begin(), call.end(),
                             oldReassemble.begin(), oldReassemble.end()) != call.end());
}

// ---------------------------------------------------------------------------
// Cleanup pops
// ---------------------------------------------------------------------------

// The Call task consumes the frame, so the caller's cleanup pops are
// suppressed (they must not pop past the restored stack).
TEST_F(X64CallConventionTest, CleanupPopsAfterCallAreSuppressed) {
    EmitPush(1, "8");
    EmitPush(1, "7");
    EmitCall();

    const auto before = m_pWriter->GetCurrentMCPosition();
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::PopEbx), nullptr, 0u));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::PopEax), nullptr, 0u));
    EXPECT_EQ(m_pWriter->GetCurrentMCPosition(), before); // nothing emitted
}

// A pop with no pending cleanup still emits (POP RBX).
TEST_F(X64CallConventionTest, UnmatchedPopStillEmits) {
    const auto before = m_pWriter->GetCurrentMCPosition();
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::PopEbx), nullptr, 0u));
    EXPECT_EQ(BytesSince(before), std::vector<uint8_t>({0x5B}));
}

// A double arg pushes 2 slots, so the caller emits 2 cleanup pops and both
// are suppressed.
TEST_F(X64CallConventionTest, DoubleArgSuppressesTwoCleanupPops) {
    EmitPush(8, "2.5");
    EmitCall();
    const auto before = m_pWriter->GetCurrentMCPosition();
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::PopEbx), nullptr, 0u));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::PopEax), nullptr, 0u));
    EXPECT_EQ(m_pWriter->GetCurrentMCPosition(), before);
}

// Nested calls: an inner call's frame is fully consumed by its own Call
// task; the result push feeds the outer call with only its own arg.
TEST_F(X64CallConventionTest, NestedCallsTrackFramesIndependently) {
    const auto before = m_pWriter->GetCurrentMCPosition();

    // Inner call: foo(8, 7)
    EmitPush(1, "8");
    EmitPush(1, "7");
    EmitCall();
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::PopEbx), nullptr, 0u));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::PopEbx), nullptr, 0u));

    // Outer call: print(innerResult)
    EmitPush(1, "5");
    EmitCall();
    const auto outer = BytesSince(before);

    // Two CALLs, and the outer frame has exactly one arg: the outer call is
    // SUB RSP,32 + MOV RCX,[RSP+32] + tail + ADD RSP,32 (25 bytes).
    int calls = 0;
    for (std::size_t i = 0; i + 1 < outer.size(); ++i) {
        if (outer[i] == 0xFF && outer[i + 1] == 0xD3) ++calls;
    }
    EXPECT_EQ(calls, 2);
    // Outer call after the inner frame: one push -> RSP%16==8 -> F=40.
    const std::vector<uint8_t> addRsp40{0x48, 0x83, 0xC4, 0x28};
    EXPECT_TRUE(EndsWith(outer, addRsp40));
    EXPECT_EQ(outer[outer.size() - 25 + 6], 0x4C); // MOV RCX,[RSP+40]
    EXPECT_EQ(outer[outer.size() - 25 + 8], 0x28);
}

// User-function calls (CALLMEM) keep the x86 stack convention; their pushes
// must not leak into the next DLL call's pending frame.
TEST_F(X64CallConventionTest, UserFunctionCallResetsPendingFrame) {
    EmitPush(1, "8");
    EmitPush(1, "7");
    // CALLMEM with a code-label reference (rel32 placeholder)
    const auto beforeCall = m_pWriter->GetCurrentMCPosition();
    CStr label("$myfunc");
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::JumpSubroutine), &label, 10));
    (void)BytesSince(beforeCall);
    // caller pops user-function args (not suppressed — not a DLL call)
    const auto beforePops = m_pWriter->GetCurrentMCPosition();
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::PopEbx), nullptr, 0u));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::PopEbx), nullptr, 0u));
    EXPECT_EQ(BytesSince(beforePops), std::vector<uint8_t>({0x5B, 0x5B}));

    // Now a DLL call with a single arg must see a single-arg frame.
    EmitPush(1, "5");
    const auto call = EmitCall();
    EXPECT_EQ(call.size(), 4u + 5u + 12u + 4u); // no stack-arg moves
    EXPECT_EQ(call[6], 0x4C); EXPECT_EQ(call[8], 0x28); // RCX,[RSP+40] (F=40)
}

// ---------------------------------------------------------------------------
// Alignment tracking
// ---------------------------------------------------------------------------

// SUBESP (compile-time immediate) participates in alignment tracking.
TEST_F(X64CallConventionTest, SubEspTaskTracksAlignment) {
    CStr amount("8");
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::SubEsp), &amount, 7));
    // RSP%16 == 8 -> the zero-arg call needs an 8-byte pad (F = 40).
    const auto call = EmitCall();
    EXPECT_EQ(call[3], 0x28); // SUB RSP,40
    EXPECT_TRUE(Contains(call, CallTailBytes()));
}

// PushRegisters (prologue) participates in alignment tracking: after the
// 7 pushes the body is aligned at 0.
TEST_F(X64CallConventionTest, PrologueLeavesBodyAligned) {
    EmitPush(1, "7");
    EmitPush(1, "7");
    const auto call = EmitCall();
    EXPECT_EQ(call[3], 0x20); // SUB RSP,32, no pad
}

// ---------------------------------------------------------------------------
// Fallback + frame ops
// ---------------------------------------------------------------------------

// A UDT-typed pending frame (PushUdt) falls back to the legacy call bytes:
// no register passing, no shadow allocation, no cleanup suppression.
TEST_F(X64CallConventionTest, UdtArgFallsBackToLegacyCall) {
    CStr udt("@myudt");
    CStr size("16");
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP2(
        1u, static_cast<DWORD>(ASMTask::PushUdt), &udt, 1001, &size, 7));
    const auto call = EmitCall();
    EXPECT_EQ(call.size(), 12u); // MOV RBX,[idx] + CALL RBX only
    EXPECT_EQ(call[0], 0x48); EXPECT_EQ(call[1], 0xBB);
    EXPECT_TRUE(Contains(call, CallTailBytes()));

    // cleanup pop is NOT suppressed
    const auto before = m_pWriter->GetCurrentMCPosition();
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::PopEbx), nullptr, 0u));
    EXPECT_EQ(BytesSince(before), std::vector<uint8_t>({0x5B}));
}

// The frame ops gain REX.W so EBP/RSP are 64-bit on x64.
TEST_F(X64CallConventionTest, FrameOpsUseRexW) {
    const auto before = m_pWriter->GetCurrentMCPosition();
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::MovBpEsp), nullptr, 0u));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::MovSpEbp), nullptr, 0u));
    EXPECT_EQ(BytesSince(before), std::vector<uint8_t>({0x48, 0x89, 0xE5,
                                                        0x48, 0x89, 0xEC}));
}

// A stack pointer load (RestoreEsp) poisons the alignment tracker: the next
// call degrades to the legacy bytes (no shadow/register passing) rather than
// emitting a possibly-misaligned call.
TEST_F(X64CallConventionTest, RestoreEspPoisonFallsBackToLegacy) {
    CStr esp("@$_ESP_");
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::RestoreEsp), &esp, 7));
    EmitPush(1, "7");
    const auto call = EmitCall();
    EXPECT_EQ(call.size(), 12u); // MOV RBX,[idx] + CALL RBX only
    EXPECT_TRUE(Contains(call, CallTailBytes()));
    // cleanup pop not suppressed
    const auto before = m_pWriter->GetCurrentMCPosition();
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::PopEbx), nullptr, 0u));
    EXPECT_EQ(BytesSince(before), std::vector<uint8_t>({0x5B}));
}
