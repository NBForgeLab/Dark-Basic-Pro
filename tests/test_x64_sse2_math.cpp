// test_x64_sse2_math.cpp
//
// Wave 8 — the emitter's floating-point pipeline moves from x87 to SSE2:
//   * FLD/FSTP (ST0) load/store opcodes become MOVSD XMM0 (double) /
//     MOVSS XMM0 (float);
//   * float/double arithmetic (Add/Sub/Mul/Div) is emitted inline as
//     ADDSS/SUBSD/MULSD/DIVSD (XMM0 op XMM1) instead of runtime DLL calls;
//   * comparisons emit UCOMISD/UCOMISS + SETcc;
//   * int<->float casts emit CVTSI2SD/CVTSI2SS/CVTTSD2SI/CVTTSS2SI and the
//     double<->float CVTSD2SS/CVTSS2SD.
//
// All assertions fail against the wave-7 emitter (x87 bytes DD 05/DD 1D,
// ERR_SYNTAX+50 on hardcoded float math, DLL calls for float math and casts).
//
// Design: docs/superpowers/specs/2026-08-11-x64-sse2-float-pipeline-design.md

#include "gtest/gtest.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <windows.h>

#include "CompilerContext.h"
#include "ASMWriter.h"
#include "DBPCompiler.h"
#include "DBPLogger.h"
#include "EXEBlock.h"
#include "InstructionTable.h"
#include "ReferenceTracker.h"
#include "Statement.h"
#include "StatementList.h"
#include "Str.h"
#include "StructTable.h"

extern CStructTable* g_pStructTable;
extern CInstructionTable* g_pInstructionTable;
extern CStatementList* g_pStatementList;
extern ICodeGenerator* g_pASMWriter;
extern CDBPCompiler* g_pDBPCompiler;

namespace
{
std::vector<uint8_t> AsBytes(const char* raw, std::size_t length)
{
    const auto* bytes = reinterpret_cast<const uint8_t*>(raw);
    return std::vector<uint8_t>(bytes, bytes + length);
}

bool Contains(const std::vector<uint8_t>& bytes,
              const std::vector<uint8_t>& needle)
{
    return std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end())
           != bytes.end();
}
} // namespace

class X64Sse2MathTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_sse2_math.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();
        g_pStructTable->SetStructDefaults();
        ASSERT_TRUE(g_pInstructionTable->SetInternalInstructionDatabase());
        m_pWriter = static_cast<CASMWriter*>(g_pASMWriter);
        ASSERT_TRUE(g_pASMWriter->CreateASMHeader());
    }

    void TearDown() override {
        if (m_pContext) {
            m_pContext->Cleanup();
            delete m_pContext;
            m_pContext = nullptr;
        }
        spdlog::shutdown();
    }

    // Emits one instruction through the production path and returns exactly
    // the MCB bytes written for it (fixed opcode prefix; address slots stay
    // as placeholder bytes).
    std::vector<uint8_t> EmitLine(DWORD dwOp, const char* data) {
        const auto before = m_pWriter->GetCurrentMCPosition();
        if (!m_pWriter->WriteASMLine(dwOp, const_cast<char*>(data))) return {};
        const auto after = m_pWriter->GetCurrentMCPosition();
        const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
        return AsBytes(raw + before, after - before);
    }

    std::vector<uint8_t> BytesSince(std::size_t before) const {
        const auto after = m_pWriter->GetCurrentMCPosition();
        const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
        return AsBytes(raw + before, after - before);
    }

    // Runs the link-time patch pass over the writer's whole code buffer so
    // immediate slots become their real values.
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

    // Runs the write phase and returns the patched machine-code stream.
    std::vector<uint8_t> Compile() {
        char compilerPath[] = "DBPCompiler.exe";
        CDBPCompiler compiler(compilerPath);
        CDBPCompiler* const previousCompiler = g_pDBPCompiler;
        g_pDBPCompiler = &compiler;
        g_pStatementList->SetWriteStarted(true);
        EXPECT_TRUE(g_pStatementList->GetProgramStatements()->WriteDBM());
        EXPECT_TRUE(g_pStatementList->GetPreScanStatements()->WriteDBM());
        g_pDBPCompiler = previousCompiler;

        const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
        std::vector<uint8_t> stream(raw, raw + m_pWriter->GetCurrentMCPosition());
        PatchAll(stream);
        return stream;
    }

    // True when any pending reference label contains the given substring
    // (used to prove a runtime DLL call was *not* emitted).
    bool AnyReferenceContains(const char* needle) const {
        const auto& records = m_pWriter->GetReferenceTracker().GetRecords();
        return std::any_of(records.begin(), records.end(),
            [needle](const CReferenceTracker::Record& r) {
                return strstr(r.label.c_str(), needle) != nullptr;
            });
    }
};

// ---------------------------------------------------------------------------
// x87 -> SSE2 load/store (the eight legacy ST0 opcodes now move XMM0)
// ---------------------------------------------------------------------------

TEST_F(X64Sse2MathTest, FldMemEmitsMovsdXmm0ThroughRbx)
{
    // Old: FLD qword [MEM] = DD 03. New: MOVSD XMM0, qword [RBX] = F2 0F 10 03.
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVSDXMM0MEM), "@d");
    ASSERT_GE(bytes.size(), 14u);
    EXPECT_EQ(bytes[10], 0xF2);
    EXPECT_EQ(bytes[11], 0x0F);
    EXPECT_EQ(bytes[12], 0x10);
    EXPECT_EQ(bytes[13], 0x03);
}

TEST_F(X64Sse2MathTest, FstpMemEmitsMovsdStoreThroughRbx)
{
    // Old: FSTP qword [MEM] = DD 1B. New: MOVSD qword [RBX], XMM0 = F2 0F 11 03.
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVSDMEMXMM0), "@d");
    ASSERT_GE(bytes.size(), 14u);
    EXPECT_EQ(bytes[10], 0xF2);
    EXPECT_EQ(bytes[11], 0x0F);
    EXPECT_EQ(bytes[12], 0x11);
    EXPECT_EQ(bytes[13], 0x03);
}

TEST_F(X64Sse2MathTest, FldEbpEmitsMovsdXmm0RbpDisp)
{
    // Old: FLD qword [EBP+disp] = DD 85. New: MOVSD XMM0, [RBP+disp] = F2 0F 10 85.
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVSDXMM0EBP), "+8");
    ASSERT_GE(bytes.size(), 8u);
    EXPECT_EQ(bytes[0], 0xF2);
    EXPECT_EQ(bytes[1], 0x0F);
    EXPECT_EQ(bytes[2], 0x10);
    EXPECT_EQ(bytes[3], 0x85);
}

TEST_F(X64Sse2MathTest, FstpEbpEmitsMovsdRbpDispStore)
{
    // Old: FSTP qword [EBP+disp] = DD 9D. New: MOVSD [RBP+disp], XMM0 = F2 0F 11 85.
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVSDEBPXMM0), "+8");
    ASSERT_GE(bytes.size(), 8u);
    EXPECT_EQ(bytes[0], 0xF2);
    EXPECT_EQ(bytes[1], 0x0F);
    EXPECT_EQ(bytes[2], 0x11);
    EXPECT_EQ(bytes[3], 0x85);
}

TEST_F(X64Sse2MathTest, FldEaxEmitsMovsdXmm0RaxDisp)
{
    // Old: FLD qword [EAX+disp] = DD 80. New: MOVSD XMM0, [RAX+disp] = F2 0F 10 80.
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVSDXMM0EAX), "+8");
    ASSERT_GE(bytes.size(), 8u);
    EXPECT_EQ(bytes[0], 0xF2);
    EXPECT_EQ(bytes[1], 0x0F);
    EXPECT_EQ(bytes[2], 0x10);
    EXPECT_EQ(bytes[3], 0x80);
}

TEST_F(X64Sse2MathTest, FstpEaxEmitsMovsdRaxDispStore)
{
    // Old: FSTP qword [EAX+disp] = DD 98. New: MOVSD [RAX+disp], XMM0 = F2 0F 11 80.
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVSDEAXXMM0), "+8");
    ASSERT_GE(bytes.size(), 8u);
    EXPECT_EQ(bytes[0], 0xF2);
    EXPECT_EQ(bytes[1], 0x0F);
    EXPECT_EQ(bytes[2], 0x11);
    EXPECT_EQ(bytes[3], 0x80);
}

TEST_F(X64Sse2MathTest, FldEcxOffEmitsMovsdXmm0RcxDisp)
{
    // Old: FLD qword [ECX+disp] = DD 81. New: MOVSD XMM0, [RCX+disp] = F2 0F 10 81.
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVSDXMM0ECXOFF), "+8");
    ASSERT_GE(bytes.size(), 8u);
    EXPECT_EQ(bytes[0], 0xF2);
    EXPECT_EQ(bytes[1], 0x0F);
    EXPECT_EQ(bytes[2], 0x10);
    EXPECT_EQ(bytes[3], 0x81);
}

TEST_F(X64Sse2MathTest, FstpEcxOffEmitsMovsdRcxDispStore)
{
    // Old: FSTP qword [ECX+disp] = DD 99. New: MOVSD [RCX+disp], XMM0 = F2 0F 11 81.
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVSDECXOFFXMM0), "+8");
    ASSERT_GE(bytes.size(), 8u);
    EXPECT_EQ(bytes[0], 0xF2);
    EXPECT_EQ(bytes[1], 0x0F);
    EXPECT_EQ(bytes[2], 0x11);
    EXPECT_EQ(bytes[3], 0x81);
}

// ---------------------------------------------------------------------------
// Task-level: hardcoded SSE2 arithmetic and compares on doubles/floats
// ---------------------------------------------------------------------------

TEST_F(X64Sse2MathTest, DoubleAddEmitsAddsd)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Add),
        &a, nullptr, 8, 0, &b, nullptr, 8, 0, &r, nullptr, 8, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x58, 0xC1})) // ADDSD XMM0,XMM1
        << "double add must emit ADDSD (F2 0F 58 C1), not x87/error";
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x10, 0x03})); // MOVSD XMM0,[RBX]
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x11, 0x03})); // MOVSD [RBX],XMM0
}

TEST_F(X64Sse2MathTest, DoubleSubEmitsSubsd)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Sub),
        &a, nullptr, 8, 0, &b, nullptr, 8, 0, &r, nullptr, 8, 0));
    EXPECT_TRUE(Contains(BytesSince(before), {0xF2, 0x0F, 0x5C, 0xC1})); // SUBSD
}

TEST_F(X64Sse2MathTest, DoubleMulEmitsMulsd)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Mul),
        &a, nullptr, 8, 0, &b, nullptr, 8, 0, &r, nullptr, 8, 0));
    EXPECT_TRUE(Contains(BytesSince(before), {0xF2, 0x0F, 0x59, 0xC1})); // MULSD
}

TEST_F(X64Sse2MathTest, DoubleDivEmitsDivsd)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Div),
        &a, nullptr, 8, 0, &b, nullptr, 8, 0, &r, nullptr, 8, 0));
    EXPECT_TRUE(Contains(BytesSince(before), {0xF2, 0x0F, 0x5E, 0xC1})); // DIVSD
}

TEST_F(X64Sse2MathTest, FloatAddEmitsAddss)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Add),
        &a, nullptr, 2, 0, &b, nullptr, 2, 0, &r, nullptr, 2, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0xF3, 0x0F, 0x58, 0xC1})) // ADDSS XMM0,XMM1
        << "float add must emit ADDSS (F3 0F 58 C1)";
    EXPECT_TRUE(Contains(bytes, {0x66, 0x0F, 0x6E, 0xC0})); // MOVD XMM0,EAX
}

TEST_F(X64Sse2MathTest, DoubleCompareEmitsUcomisdSeta)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Greater),
        &a, nullptr, 8, 0, &b, nullptr, 8, 0, &r, nullptr, 1, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x66, 0x0F, 0x2E, 0xC1})) // UCOMISD XMM0,XMM1
        << "double compare must emit UCOMISD (66 0F 2E C1)";
    EXPECT_TRUE(Contains(bytes, {0x0F, 0x97})) // SETA AL
        << "double greater must emit SETA (0F 97)";
}

TEST_F(X64Sse2MathTest, FloatCompareEmitsUcomissSeta)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Greater),
        &a, nullptr, 2, 0, &b, nullptr, 2, 0, &r, nullptr, 1, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x0F, 0x2E, 0xC1})) // UCOMISS XMM0,XMM1
        << "float compare must emit UCOMISS (0F 2E C1)";
    EXPECT_TRUE(Contains(bytes, {0x0F, 0x97})); // SETA AL
}

// ---------------------------------------------------------------------------
// Compiler-level: DBP source drives the SSE2 pipeline end to end
// ---------------------------------------------------------------------------

TEST_F(X64Sse2MathTest, CompiledFloatAddEmitsAddssAndNoDllCall)
{
    char prog[] = "a#=b#+c#\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();

    EXPECT_TRUE(Contains(stream, {0xF3, 0x0F, 0x58, 0xC1})); // ADDSS XMM0,XMM1
    EXPECT_FALSE(AnyReferenceContains("AddFFF"))
        << "float + must be hardcoded SSE2, not a dbprocore AddFFF call";
}

TEST_F(X64Sse2MathTest, CompiledFloatSubEmitsSubss)
{
    char prog[] = "a#=b#-c#\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_TRUE(Contains(Compile(), {0xF3, 0x0F, 0x5C, 0xC1})); // SUBSS
}

TEST_F(X64Sse2MathTest, CompiledFloatMulEmitsMulss)
{
    char prog[] = "a#=b#*c#\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_TRUE(Contains(Compile(), {0xF3, 0x0F, 0x59, 0xC1})); // MULSS
}

TEST_F(X64Sse2MathTest, CompiledFloatDivEmitsDivss)
{
    char prog[] = "a#=b#/c#\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_TRUE(Contains(Compile(), {0xF3, 0x0F, 0x5E, 0xC1})); // DIVSS
}

TEST_F(X64Sse2MathTest, CompiledIntToFloatCastEmitsCvtsi2ss)
{
    char prog[] = "a#=5\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0xF3, 0x0F, 0x2A, 0xC0})); // CVTSI2SS XMM0,EAX
    EXPECT_FALSE(AnyReferenceContains("CastLtoF"))
        << "int->float cast must be CVTSI2SS, not a CastLtoF DLL call";
}

TEST_F(X64Sse2MathTest, CompiledFloatToIntCastEmitsCvttss2si)
{
    char prog[] = "a=b#\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_TRUE(Contains(Compile(), {0xF3, 0x0F, 0x2C, 0xC0})); // CVTTSS2SI EAX,XMM0
}

TEST_F(X64Sse2MathTest, IntToDoubleCastEmitsCvtsi2sd)
{
    // Scalar typed declarations fail in the legacy pre-scan (pre-existing), so
    // the double casts are driven through the emitter task directly.
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastIntToDouble),
        &a, nullptr, 7, 0, nullptr, nullptr, 0, 0, &r, nullptr, 8, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x2A, 0xC0})); // CVTSI2SD XMM0,EAX
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x11, 0x03})); // MOVSD [RBX],XMM0
}

TEST_F(X64Sse2MathTest, DoubleToIntCastEmitsCvttsd2si)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastDoubleToInt),
        &a, nullptr, 8, 0, nullptr, nullptr, 0, 0, &r, nullptr, 1, 0));
    EXPECT_TRUE(Contains(BytesSince(before), {0xF2, 0x0F, 0x2C, 0xC0})); // CVTTSD2SI
}

TEST_F(X64Sse2MathTest, DoubleToFloatCastEmitsCvtsd2ss)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastDoubleToFloat),
        &a, nullptr, 8, 0, nullptr, nullptr, 0, 0, &r, nullptr, 2, 0));
    EXPECT_TRUE(Contains(BytesSince(before), {0xF2, 0x0F, 0x5A, 0xC0})); // CVTSD2SS
}

TEST_F(X64Sse2MathTest, FloatToDoubleCastEmitsCvtss2sd)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastFloatToDouble),
        &a, nullptr, 2, 0, nullptr, nullptr, 0, 0, &r, nullptr, 8, 0));
    EXPECT_TRUE(Contains(BytesSince(before), {0xF3, 0x0F, 0x5A, 0xC0})); // CVTSS2SD
}

TEST_F(X64Sse2MathTest, CompiledFloatIncEmitsAddss)
{
    char prog[] = "a#=b#\r\ninc a#\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0xF3, 0x0F, 0x58, 0xC1})); // ADDSS (inc a#)
    EXPECT_FALSE(AnyReferenceContains("AddFFF"))
        << "inc a# must be hardcoded SSE2, not a dbprocore AddFFF call";
}

// Regression: CMathOp::IsLiteral must not dereference the global
// g_pDBPCompiler (only set by the standalone EXE entry point) while
// classifying a decimal literal. Before the fix, a#=1.5 crashed with an
// access violation (SEH 0xC0000005) whenever the parse pipeline ran
// without a live compiler object -- e.g. inside dbp_compiler_lib hosts.
TEST_F(X64Sse2MathTest, FloatLiteralAssignmentSurvivesWithoutCompilerObject)
{
    // Force the pre-fix crash condition: no CDBPCompiler instance alive.
    CDBPCompiler* const previousCompiler = g_pDBPCompiler;
    g_pDBPCompiler = nullptr;

    char prog[] = "a#=1.5\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    g_pDBPCompiler = previousCompiler;
    const auto stream = Compile();

    // The literal must become an inlined float store, not a runtime DLL call:
    //   B8 00 00 C0 3F  mov eax, 0x3FC00000   (1.5f as float)
    //   A3 <addr>       mov [varspace], eax
    EXPECT_TRUE(Contains(stream, {0xB8, 0x00, 0x00, 0xC0, 0x3F, 0xA3}))
        << "a#=1.5 must inline mov eax,1.5f + store without crashing";
    EXPECT_FALSE(AnyReferenceContains("AddFFF"))
        << "decimal literal assignment must not call AddFFF";
    EXPECT_FALSE(AnyReferenceContains("CastLtoF"))
        << "decimal literal assignment must not call CastLtoF";
}

// Same path with the compiler object alive (the production shape): the
// literal classification must still pick float and emit the same store.
TEST_F(X64Sse2MathTest, FloatLiteralAssignmentWithCompilerObject)
{
    char prog[] = "a#=1.5\r\nend\r\n";
    char compilerPath[] = "DBPCompiler.exe";
    CDBPCompiler compiler(compilerPath);
    CDBPCompiler* const previousCompiler = g_pDBPCompiler;
    g_pDBPCompiler = &compiler;

    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    g_pDBPCompiler = previousCompiler;
    const auto stream = Compile();

    EXPECT_TRUE(Contains(stream, {0xB8, 0x00, 0x00, 0xC0, 0x3F, 0xA3}))
        << "a#=1.5 must inline mov eax,1.5f + store";
    EXPECT_FALSE(AnyReferenceContains("AddFFF"));
    EXPECT_FALSE(AnyReferenceContains("CastLtoF"));
}

