// test_x64_user_function_frames.cpp
//
// Wave 4 x64 custom user-function frame tests (TDD): the frame prologue and
// epilogue must use the 64-bit stack pointer (REX.W SUB/ADD RSP), every stack
// slot is 8 bytes (EBP-relative displacements scaled x2: first parameter at
// RBP+16, return value at RBP-8, locals below), ClearStack must zero the local
// region with a 64-bit-safe loop, and the caller's cleanup after a
// user-function call scales by 8 bytes per slot.
//
// Note: Imm32 slots are emitted as 0xFFFFFFFF and patched at link time via the
// reference tracker (CEXEBlock::PatchReferenceValues), so tests that assert
// concrete immediate values run the patch pass first.
//
// Design: docs/superpowers/specs/2026-08-11-x64-user-function-frames-design.md

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <windows.h>

#include "DBPLogger.h"
#include "ASMWriter.h"
#include "DBPCompiler.h"
#include "EXEBlock.h"
#include "InstructionTable.h"
#include "ReferenceTracker.h"
#include "Statement.h"
#include "StatementList.h"
#include "StructTable.h"
#include "Error.h"

#include "CompilerContext.h"

// Declare compiler global pointers defined in dbp_compiler_lib
extern CStructTable*       g_pStructTable;
extern CStatementList*     g_pStatementList;
extern CInstructionTable*  g_pInstructionTable;
extern ICodeGenerator*     g_pASMWriter;
extern CError*             g_pErrorReport;
extern CDBPCompiler*       g_pDBPCompiler;

namespace
{
std::vector<uint8_t> AsBytes(const char* raw, std::size_t length)
{
    const auto* bytes = reinterpret_cast<const uint8_t*>(raw);
    return std::vector<uint8_t>(bytes, bytes + length);
}

bool EndsWith(const std::vector<uint8_t>& bytes,
              const std::vector<uint8_t>& suffix)
{
    if (bytes.size() < suffix.size()) return false;
    return std::equal(suffix.begin(), suffix.end(),
                      bytes.end() - suffix.size());
}

bool Contains(const std::vector<uint8_t>& bytes,
              const std::vector<uint8_t>& needle)
{
    return std::search(bytes.begin(), bytes.end(),
                       needle.begin(), needle.end()) != bytes.end();
}
} // namespace

class X64UserFunctionFramesTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_user_function_frames.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();
        g_pStructTable->SetStructDefaults();

        char dummyProg[] = "";
        g_pStatementList->MakeStatements(dummyProg, 0);
        ASSERT_TRUE(g_pASMWriter->CreateASMHeader());
        m_pWriter = static_cast<CASMWriter*>(g_pASMWriter);
    }

    void TearDown() override {
        if (m_pContext) {
            m_pContext->Cleanup();
            delete m_pContext;
            m_pContext = nullptr;
        }
        spdlog::shutdown();
    }

    std::vector<uint8_t> BytesSince(std::size_t before) const {
        const auto after = m_pWriter->GetCurrentMCPosition();
        const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
        return AsBytes(raw + before, after - before);
    }

    std::size_t Position() const {
        return m_pWriter->GetCurrentMCPosition();
    }

    // Runs the link-time patch pass over the writer's whole code buffer so
    // immediate slots (0xFFFFFFFF) become their real values.
    void PatchAll(std::vector<uint8_t>& code) const {
        std::vector<uintptr_t> values;
        std::vector<DWORD> positions;
        std::vector<DWORD> types;
        for (const auto& record : m_pWriter->GetReferenceTracker().GetRecords())
        {
            const auto parsed = ParseReferenceLabel(record.label);
            ASSERT_TRUE(parsed.has_value()) << "unparseable reference: " << record.label;
            values.push_back(parsed->index);
            positions.push_back(record.machineCodeOffset);
            types.push_back(static_cast<DWORD>(parsed->kind));
        }
        CEXEBlock::PatchReferenceValues(
            values.data(), values.size(),
            positions.data(), types.data(),
            reinterpret_cast<char*>(code.data()));
    }

    void EmitTask(DWORD task, const char* value) {
        CStr val(const_cast<char*>(value));
        ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
            1u, task, &val, 7u));
    }

    void EmitTaskNull(DWORD task) {
        ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
            1u, task, nullptr, 0u));
    }

    void EmitPush(DWORD type, const char* value) {
        CStr val(const_cast<char*>(value));
        ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
            1u, static_cast<DWORD>(ASMTask::Push), &val, type));
    }
};

// ---------------------------------------------------------------------------
// Frame prologue / epilogue byte contract
// ---------------------------------------------------------------------------

TEST_F(X64UserFunctionFramesTest, PrologueEmitsX64FrameBytes) {
    // PUSH RBP (55) / MOV RBP,RSP (48 89 E5) / SUB RSP,32 (48 81 EC imm32,
    // patched) / ClearStack(16) via REP STOSB with the exact byte count.
    const auto before = Position();
    EmitTaskNull(static_cast<DWORD>(ASMTask::PushEbp));
    EmitTaskNull(static_cast<DWORD>(ASMTask::MovBpEsp));
    EmitTask(static_cast<DWORD>(ASMTask::SubEsp), "32");
    EmitTask(static_cast<DWORD>(ASMTask::ClearStack), "16");

    std::vector<uint8_t> bytes = BytesSince(before);
    PatchAll(bytes);

    const std::vector<uint8_t> expected = {
        0x55,                                              // PUSH RBP
        0x48, 0x89, 0xE5,                                  // MOV RBP,RSP
        0x48, 0x81, 0xEC, 0x20, 0x00, 0x00, 0x00,          // SUB RSP,32
        0x48, 0x89, 0xE0,                                  // MOV RAX,RSP
        0x33, 0xC0,                                        // XOR EAX,EAX
        0x48, 0x8B, 0xFC,                                  // MOV RDI,RSP
        0xB9, 0x10, 0x00, 0x00, 0x00,                      // MOV ECX,16
        0xFC,                                              // CLD
        0xF3, 0xAA                                         // REP STOSB
    };
    EXPECT_EQ(bytes, expected);
}

TEST_F(X64UserFunctionFramesTest, EpilogueEmitsX64FrameBytes) {
    // MOV RSP,RBP (48 89 EC) / POP RBP (5D) / RET (C3).
    const auto before = Position();
    EmitTaskNull(static_cast<DWORD>(ASMTask::MovSpEbp));
    EmitTaskNull(static_cast<DWORD>(ASMTask::PopEbp));
    EmitTaskNull(static_cast<DWORD>(ASMTask::PureReturn));

    const std::vector<uint8_t> expected = {
        0x48, 0x89, 0xEC, 0x5D, 0xC3
    };
    EXPECT_EQ(BytesSince(before), expected);
}

TEST_F(X64UserFunctionFramesTest, SubEspAndAddEspUseRexW) {
    // SUB RSP,imm32 / ADD RSP,imm32 must carry the 48 REX.W prefix so the
    // 64-bit stack pointer is adjusted, never a truncated 32-bit value.
    const auto before = Position();
    EmitTask(static_cast<DWORD>(ASMTask::SubEsp), "32");
    EmitTask(static_cast<DWORD>(ASMTask::AddEsp), "16");

    std::vector<uint8_t> bytes = BytesSince(before);
    PatchAll(bytes);

    const std::vector<uint8_t> expected = {
        0x48, 0x81, 0xEC, 0x20, 0x00, 0x00, 0x00,   // SUB RSP,32
        0x48, 0x81, 0xC4, 0x10, 0x00, 0x00, 0x00    // ADD RSP,16
    };
    EXPECT_EQ(bytes, expected);
}

TEST_F(X64UserFunctionFramesTest, ClearStackZeroesExactByteCountWithStosb) {
    // The legacy SIB[EAX:ECX*4]+LOOP structure couples the loop counter to
    // the addressing index and cannot clear a region in bounds; the x64
    // prologue must zero [RSP, RSP+n) exactly with an independent counter.
    const auto before = Position();
    EmitTask(static_cast<DWORD>(ASMTask::ClearStack), "12");

    const std::vector<uint8_t> expected = {
        0x48, 0x89, 0xE0,                                  // MOV RAX,RSP
        0x33, 0xC0,                                        // XOR EAX,EAX
        0x48, 0x8B, 0xFC,                                  // MOV RDI,RSP
        0xB9, 0x0C, 0x00, 0x00, 0x00,                      // MOV ECX,12
        0xFC,                                              // CLD
        0xF3, 0xAA                                         // REP STOSB
    };
    EXPECT_EQ(BytesSince(before), expected);
}

// ---------------------------------------------------------------------------
// EBP-relative displacement contract (8-byte slots)
// ---------------------------------------------------------------------------

TEST_F(X64UserFunctionFramesTest, ParamAccessUsesEightByteSlotDisplacements) {
    // First parameter at RBP+16, second at RBP+24 (x86 legacy: +8/+12).
    const auto before = Position();
    ASSERT_TRUE(m_pWriter->WriteASMLine(
        static_cast<DWORD>(ASMOp::MOVEAXEBP4), const_cast<char*>("16")));
    ASSERT_TRUE(m_pWriter->WriteASMLine(
        static_cast<DWORD>(ASMOp::MOVEAXEBP4), const_cast<char*>("24")));

    std::vector<uint8_t> bytes = BytesSince(before);
    PatchAll(bytes);

    const std::vector<uint8_t> expected = {
        0x8B, 0x85, 0x10, 0x00, 0x00, 0x00,   // MOV EAX,[RBP+16]
        0x8B, 0x85, 0x18, 0x00, 0x00, 0x00    // MOV EAX,[RBP+24]
    };
    EXPECT_EQ(bytes, expected);
}

TEST_F(X64UserFunctionFramesTest, LocalAccessUsesNegativeRbpDisplacements) {
    // Return value at RBP-8, first local at RBP-16 (x86 legacy: -4/-8).
    const auto before = Position();
    ASSERT_TRUE(m_pWriter->WriteASMLine(
        static_cast<DWORD>(ASMOp::MOVEAXEBP4), const_cast<char*>("-8")));
    ASSERT_TRUE(m_pWriter->WriteASMLine(
        static_cast<DWORD>(ASMOp::MOVEAXEBP4), const_cast<char*>("-16")));

    std::vector<uint8_t> bytes = BytesSince(before);
    PatchAll(bytes);

    const std::vector<uint8_t> expected = {
        0x8B, 0x85, 0xF8, 0xFF, 0xFF, 0xFF,   // MOV EAX,[RBP-8]
        0x8B, 0x85, 0xF0, 0xFF, 0xFF, 0xFF    // MOV EAX,[RBP-16]
    };
    EXPECT_EQ(bytes, expected);
}

// ---------------------------------------------------------------------------
// Caller side of a user-function call
// ---------------------------------------------------------------------------

TEST_F(X64UserFunctionFramesTest, CallerCleanupAfterUserFunctionCallScalesByEight) {
    // Params pushed as 8-byte slots, CALL to the function label, then the
    // caller pops slots*8 bytes via ADD RSP (REX.W).
    const auto before = Position();
    EmitPush(7, "1");
    EmitPush(7, "2");
    EmitTask(static_cast<DWORD>(ASMTask::JumpSubroutine), "$labeladd");
    EmitTask(static_cast<DWORD>(ASMTask::AddEsp), "16");

    std::vector<uint8_t> bytes = BytesSince(before);
    PatchAll(bytes);

    // Direct CALL rel32 to the function label.
    EXPECT_TRUE(Contains(bytes, {0xE8}));
    // ADD RSP,16 — two 8-byte slots popped by the caller.
    EXPECT_TRUE(EndsWith(bytes, {0x48, 0x81, 0xC4, 0x10, 0x00, 0x00, 0x00}));
}

TEST_F(X64UserFunctionFramesTest, DllCallInsideFrameUsesTrackedAlignment) {
    // Frame: PUSH RBP (+8) + MOV RBP,RSP + SUB RSP,32 -> RSP%16 == 0 at the
    // body. A 2-arg DLL call therefore needs exactly 32 bytes of shadow space
    // (no padding): 48 83 EC 20, then RCX/RDX register loads, the
    // command-address load + CALL RBX, then the frame teardown ADD RSP,32.
    const auto before = Position();
    EmitTaskNull(static_cast<DWORD>(ASMTask::PushEbp));
    EmitTaskNull(static_cast<DWORD>(ASMTask::MovBpEsp));
    EmitTask(static_cast<DWORD>(ASMTask::SubEsp), "32");
    EmitPush(7, "1");
    EmitPush(7, "2");

    CStr call("@mycore.dll,@myfunc");
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP2(
        1u, static_cast<DWORD>(ASMTask::Call), &call, 0, nullptr, 0));

    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x48, 0x83, 0xEC, 0x20}));      // SUB RSP,32
    EXPECT_TRUE(Contains(bytes, {0x48, 0x8B, 0x4C, 0x24, 0x20})); // MOV RCX,[RSP+32]
    EXPECT_TRUE(Contains(bytes, {0x48, 0x8B, 0x54, 0x24, 0x28})); // MOV RDX,[RSP+40]
    EXPECT_TRUE(Contains(bytes, {0xFF, 0xD3}));                  // CALL RBX
    EXPECT_TRUE(EndsWith(bytes, {0x48, 0x83, 0xC4, 0x20}));      // ADD RSP,32
}

// ---------------------------------------------------------------------------
// Compiler-level: real function source through the parse + emit pipeline
// ---------------------------------------------------------------------------

TEST_F(X64UserFunctionFramesTest, CompiledFunctionEmitsScaledRbpDisplacements) {
    // A function with two integer parameters must be compiled with an 8-byte
    // slot frame: param0 at [RBP+16], param1 at [RBP+24], prologue
    // SUB RSP,2*(12+4)=32 with REX.W, and the epilogue MOV RSP,RBP/POP RBP/RET.
    m_pContext->Cleanup();
    delete m_pContext;
    m_pContext = new CompilerContext();
    m_pContext->Initialize();
    g_pStructTable->SetStructDefaults();
    g_pInstructionTable->SetInternalInstructionDatabase();
    m_pWriter = static_cast<CASMWriter*>(g_pASMWriter);
    ASSERT_TRUE(g_pASMWriter->CreateASMHeader());

    char prog[] =
        "function add(a as integer, b as integer)\r\n"
        "b = 5\r\n"
        "endfunction a\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(
        prog, (DWORD)strlen(prog) + 1));

    char compilerPath[] = "DBPCompiler.exe";
    CDBPCompiler compiler(compilerPath);
    CDBPCompiler* const previousCompiler = g_pDBPCompiler;
    g_pDBPCompiler = &compiler;
    g_pStatementList->SetWriteStarted(true);

    // CStatement::WriteDBM walks the chain itself; the write phase drives the
    // program statements (calls) then the pre-scan statements (function
    // definitions and labels), matching CDBMWriter::WriteProgramAsEXEOrDEBUG.
    ASSERT_TRUE(g_pStatementList->GetProgramStatements()->WriteDBM());
    ASSERT_TRUE(g_pStatementList->GetPreScanStatements()->WriteDBM());

    g_pDBPCompiler = previousCompiler;

    const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
    std::vector<uint8_t> stream(
        raw, raw + m_pWriter->GetCurrentMCPosition());
    PatchAll(stream);

    // Prologue: PUSH RBP / MOV RBP,RSP / SUB RSP (REX.W imm32) / the x64
    // ClearStack (MOV RAX,RSP; XOR EAX,EAX; MOV RDI,RSP; MOV ECX,imm;
    // CLD; REP STOSB).
    EXPECT_TRUE(Contains(stream, {0x55, 0x48, 0x89, 0xE5, 0x48, 0x81, 0xEC}));
    EXPECT_TRUE(Contains(stream, {0x48, 0x89, 0xE0, 0x33, 0xC0, 0x48, 0x8B,
                                  0xFC, 0xB9, 0x10, 0x00, 0x00, 0x00,
                                  0xFC, 0xF3, 0xAA}));
    // Epilogue: MOV RSP,RBP / POP RBP / RET.
    EXPECT_TRUE(Contains(stream, {0x48, 0x89, 0xEC, 0x5D, 0xC3}));
    // Param accesses at the x64 slot displacements (x86 legacy: +8/+12).
    EXPECT_TRUE(Contains(stream, {0x8B, 0x85, 0x10, 0x00, 0x00, 0x00}));
    EXPECT_TRUE(Contains(stream, {0x89, 0x85, 0x18, 0x00, 0x00, 0x00}));
}
