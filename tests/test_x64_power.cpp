// test_x64_power.cpp
//
// Wave 17 — the `^` operator (PowerLLL for int, PowerFFF for float) moves
// from single opaque dbprocore.dll calls to an emitter-built exp/log
// sequence:  x^y = exp(y * log(x))  computed in double precision using the
// CRT exp/log primitives (msvcrt.dll) resolved through the command table.
//
//   float:  MOVD XMM0,EAX; CVTSS2SD XMM0,XMM0  (widen x)
//           ... same for y ...
//           aligned call log(x)   -> MULSD XMM0,XMM1 -> aligned call exp
//           CVTSD2SS XMM0,XMM0    (narrow result to float)
//   int:    CVTSI2SD XMM0,EAX (widen) -> same log/mul/exp -> CVTTSD2SI
//
// All assertions fail against the current emitter (PowerFFF/PowerLLL are
// still dbprocore.dll calls; ASMTask::Power/BuildTask::Power do not exist).
//
// Design: docs/superpowers/specs/2026-08-11-x64-power-design.md

#include "gtest/gtest.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <windows.h>

#include "CompilerContext.h"
#include "ASMWriter.h"
#include "DataTable.h"
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
extern CDataTable* g_pDLLTable;
extern CDataTable* g_pCommandTable;

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

bool TableContains(CDataTable* pTable, const char* needle)
{
    for (CDataTable* entry = pTable; entry != nullptr; entry = entry->GetNext())
    {
        CStr* pStr = entry->GetString();
        if (pStr != nullptr && strstr(pStr->GetStr(), needle) != nullptr)
            return true;
    }
    return false;
}
} // namespace

class X64PowerTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_power.log");

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

    std::vector<uint8_t> BytesSince(std::size_t before) const {
        const auto after = m_pWriter->GetCurrentMCPosition();
        const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
        return AsBytes(raw + before, after - before);
    }

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

    bool AnyReferenceContains(const char* needle) const {
        const auto& records = m_pWriter->GetReferenceTracker().GetRecords();
        return std::any_of(records.begin(), records.end(),
            [needle](const CReferenceTracker::Record& r) {
                return strstr(r.label.c_str(), needle) != nullptr;
            });
    }
};

// ---------------------------------------------------------------------------
// Task-level: WriteASMTaskCore(Power) — float operands
// ---------------------------------------------------------------------------

TEST_F(X64PowerTest, TaskFloatPowerEmitsLogMulExpSequence)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr b(const_cast<char*>("@b#")), c(const_cast<char*>("@c#")), a(const_cast<char*>("@a#"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Power),
        &b, nullptr, 2, 0, &c, nullptr, 2, 0, &a, nullptr, 2, 0));
    const auto bytes = BytesSince(before);

    // Widen both operands float -> double.
    EXPECT_TRUE(Contains(bytes, {0xF3, 0x0F, 0x5A, 0xC0})) << "CVTSS2SD XMM0,XMM0 missing";
    // Double multiply of log(x)*y.
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x59, 0xC1})) << "MULSD XMM0,XMM1 missing";
    // Narrow the result double -> float.
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x5A, 0xC0})) << "CVTSD2SS XMM0,XMM0 missing";
    // Two aligned call frames (log + exp): SUB RSP,imm and ADD RSP,imm.
    EXPECT_TRUE(Contains(bytes, {0x48, 0x83, 0xEC})) << "SUB RSP,imm frame missing";
    EXPECT_TRUE(Contains(bytes, {0x48, 0x83, 0xC4})) << "ADD RSP,imm frame missing";
    // Two indirect calls.
    EXPECT_GE(std::count(bytes.begin(), bytes.end(), 0xFF), 2);
    EXPECT_TRUE(Contains(bytes, {0xFF, 0xD3})) << "CALL EBX missing";

    // The CRT primitives are registered in the DLL/command tables.
    EXPECT_GT(g_pDLLTable->FindString("msvcrt.dll"), 0u)
        << "msvcrt.dll must be added to the DLL table";
    EXPECT_TRUE(TableContains(g_pCommandTable, ",log")) << "log command not registered";
    EXPECT_TRUE(TableContains(g_pCommandTable, ",exp")) << "exp command not registered";

    // No dbprocore Power call.
    EXPECT_FALSE(AnyReferenceContains("PowerFFF"))
        << "float ^ must be an exp/log sequence, not a ?PowerFFF DLL call";
}

// ---------------------------------------------------------------------------
// Task-level: WriteASMTaskCore(Power) — integer operands
// ---------------------------------------------------------------------------

TEST_F(X64PowerTest, TaskIntPowerEmitsLogMulExpAndTruncatingNarrow)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr b(const_cast<char*>("@b")), c(const_cast<char*>("@c")), a(const_cast<char*>("@a"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Power),
        &b, nullptr, 1, 0, &c, nullptr, 1, 0, &a, nullptr, 1, 0));
    const auto bytes = BytesSince(before);

    // Widen both operands int -> double.
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x2A, 0xC0})) << "CVTSI2SD XMM0,EAX missing";
    // The double multiply.
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x59, 0xC1})) << "MULSD XMM0,XMM1 missing";
    // Narrow the result double -> int (truncating, matches (int)pow semantics).
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x2C, 0xC0})) << "CVTTSD2SI EAX,XMM0 missing";

    EXPECT_FALSE(AnyReferenceContains("PowerLLL"))
        << "int ^ must be an exp/log sequence, not a ?PowerLLL DLL call";
}

// ---------------------------------------------------------------------------
// Compiled end-to-end
// ---------------------------------------------------------------------------

TEST_F(X64PowerTest, CompiledFloatPowerEmitsExpLogNoDllCall)
{
    char prog[] = "a#=b#^c#\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0xF2, 0x0F, 0x59, 0xC1})) << "MULSD XMM0,XMM1 missing";
    EXPECT_TRUE(TableContains(g_pCommandTable, ",log")) << "log command not registered";
    EXPECT_FALSE(AnyReferenceContains("PowerFFF"))
        << "float ^ must be an exp/log sequence, not a ?PowerFFF DLL call";
}

TEST_F(X64PowerTest, CompiledIntPowerEmitsExpLogNoDllCall)
{
    char prog[] = "a=b^c\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0xF2, 0x0F, 0x2A, 0xC0})) << "CVTSI2SD XMM0,EAX missing";
    EXPECT_TRUE(Contains(stream, {0xF2, 0x0F, 0x59, 0xC1})) << "MULSD XMM0,XMM1 missing";
    EXPECT_TRUE(Contains(stream, {0xF2, 0x0F, 0x2C, 0xC0})) << "CVTTSD2SI EAX,XMM0 missing";
    EXPECT_TRUE(TableContains(g_pCommandTable, ",exp")) << "exp command not registered";
    EXPECT_FALSE(AnyReferenceContains("PowerLLL"))
        << "int ^ must be an exp/log sequence, not a ?PowerLLL DLL call";
}

// ---------------------------------------------------------------------------
// Wave 20: the rest of the Power family (B/Y/W/D/O/R) is native too
// ---------------------------------------------------------------------------

TEST_F(X64PowerTest, TaskBytePowerEmitsMovzxLogMulExpNoDllCast)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr b(const_cast<char*>("@b")), c(const_cast<char*>("@c")), a(const_cast<char*>("@a"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Power),
        &b, nullptr, 5, 0, &c, nullptr, 5, 0, &a, nullptr, 5, 0));
    const auto bytes = BytesSince(before);
    // byte sources are unsigned (0-255): MOVZX then widen to double.
    EXPECT_TRUE(Contains(bytes, {0x0F, 0xB6, 0xC0})) << "MOVZX EAX,AL missing";
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x2A, 0xC0})) << "CVTSI2SD XMM0,EAX missing";
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x59, 0xC1})) << "MULSD XMM0,XMM1 missing";
    // Truncating narrow to the byte target.
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x2C, 0xC0})) << "CVTTSD2SI EAX,XMM0 missing";
    EXPECT_FALSE(AnyReferenceContains("PowerYYY"))
        << "byte ^ must be an exp/log sequence, not a ?PowerBBB/?PowerYYY DLL call";
    EXPECT_FALSE(AnyReferenceContains("PowerBBB"));
}

TEST_F(X64PowerTest, TaskWordPowerEmitsMovzxAxNoDllCast)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr w(const_cast<char*>("@w")), x(const_cast<char*>("@x")), a(const_cast<char*>("@a"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Power),
        &w, nullptr, 6, 0, &x, nullptr, 6, 0, &a, nullptr, 6, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x0F, 0xB7, 0xC0})) << "MOVZX EAX,AX missing";
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x59, 0xC1})) << "MULSD XMM0,XMM1 missing";
    EXPECT_FALSE(AnyReferenceContains("PowerWWW"))
        << "word ^ must be an exp/log sequence, not a ?PowerWWW DLL call";
}

TEST_F(X64PowerTest, TaskDwordPowerEmitsLogMulExpNoDllCast)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr d(const_cast<char*>("@d")), e(const_cast<char*>("@e")), a(const_cast<char*>("@a"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Power),
        &d, nullptr, 7, 0, &e, nullptr, 7, 0, &a, nullptr, 7, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x2A, 0xC0})) << "CVTSI2SD XMM0,EAX missing";
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x59, 0xC1})) << "MULSD XMM0,XMM1 missing";
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x2C, 0xC0})) << "CVTTSD2SI EAX,XMM0 missing";
    EXPECT_FALSE(AnyReferenceContains("PowerDDD"))
        << "dword ^ must be an exp/log sequence, not a ?PowerDDD DLL call";
}

TEST_F(X64PowerTest, TaskDoublePowerEmitsFloatRoundTripNoDllCast)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr d(const_cast<char*>("@d")), e(const_cast<char*>("@e")), a(const_cast<char*>("@a"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Power),
        &d, nullptr, 8, 0, &e, nullptr, 8, 0, &a, nullptr, 8, 0));
    const auto bytes = BytesSince(before);
    // double sources need no widening; the log/mul/exp core is the same.
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x59, 0xC1})) << "MULSD XMM0,XMM1 missing";
    // PowerOOO is (float)pow(a,b): round the result through float precision.
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x5A, 0xC0})) << "CVTSD2SS XMM0,XMM0 missing";
    EXPECT_TRUE(Contains(bytes, {0xF3, 0x0F, 0x5A, 0xC0})) << "CVTSS2SD XMM0,XMM0 missing";
    EXPECT_FALSE(AnyReferenceContains("PowerOOO"))
        << "double ^ must be an exp/log sequence, not a ?PowerOOO DLL call";
}

TEST_F(X64PowerTest, TaskInt64PowerEmitsRexWConversionsNoDllCast)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr r(const_cast<char*>("@r")), s(const_cast<char*>("@s")), a(const_cast<char*>("@a"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Power),
        &r, nullptr, 9, 0, &s, nullptr, 9, 0, &a, nullptr, 9, 0));
    const auto bytes = BytesSince(before);
    // int64 -> double needs the REX.W CVTSI2SD XMM0,RAX (F2 48 0F 2A C0).
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x48, 0x0F, 0x2A, 0xC0}))
        << "CVTSI2SD XMM0,RAX (REX.W) missing";
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x59, 0xC1})) << "MULSD XMM0,XMM1 missing";
    // double -> int64 truncation needs the REX.W CVTTSD2SI RAX,XMM0.
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x48, 0x0F, 0x2C, 0xC0}))
        << "CVTTSD2SI RAX,XMM0 (REX.W) missing";
    EXPECT_FALSE(AnyReferenceContains("PowerRRR"))
        << "int64 ^ must be an exp/log sequence, not a ?PowerRRR DLL call";
}

TEST_F(X64PowerTest, CompiledBytePowerEmitsExpLogNoDllCall)
{
    char prog[] = "dim b as byte\r\nb=5\r\nb=b^b\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x0F, 0xB6, 0xC0})) << "MOVZX EAX,AL missing";
    EXPECT_TRUE(Contains(stream, {0xF2, 0x0F, 0x59, 0xC1})) << "MULSD XMM0,XMM1 missing";
    EXPECT_FALSE(AnyReferenceContains("PowerYYY"))
        << "byte ^ must be an exp/log sequence, not a ?PowerBBB/?PowerYYY DLL call";
    EXPECT_FALSE(AnyReferenceContains("PowerBBB"));
}

TEST_F(X64PowerTest, CompiledBooleanPowerEmitsExpLogNoDllCall)
{
    char prog[] = "dim fl as boolean\r\nfl=1\r\nfl=fl^fl\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_FALSE(AnyReferenceContains("PowerBBB"))
        << "boolean ^ must be an exp/log sequence, not a ?PowerBBB DLL call";
}

TEST_F(X64PowerTest, CompiledWordPowerEmitsExpLogNoDllCall)
{
    char prog[] = "dim w as word\r\nw=100\r\nw=w^w\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x0F, 0xB7, 0xC0})) << "MOVZX EAX,AX missing";
    EXPECT_FALSE(AnyReferenceContains("PowerWWW"))
        << "word ^ must be an exp/log sequence, not a ?PowerWWW DLL call";
}

TEST_F(X64PowerTest, CompiledDwordPowerEmitsExpLogNoDllCall)
{
    char prog[] = "dim d as dword\r\nd=7\r\nd=d^d\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0xF2, 0x0F, 0x59, 0xC1})) << "MULSD XMM0,XMM1 missing";
    EXPECT_TRUE(Contains(stream, {0xF2, 0x0F, 0x2C, 0xC0})) << "CVTTSD2SI EAX,XMM0 missing";
    EXPECT_FALSE(AnyReferenceContains("PowerDDD"))
        << "dword ^ must be an exp/log sequence, not a ?PowerDDD DLL call";
}

TEST_F(X64PowerTest, CompiledDoublePowerEmitsFloatRoundTripNoDllCall)
{
    char prog[] = "dim dd as double float\r\ndd=1.5\r\ndd=dd^dd\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0xF2, 0x0F, 0x59, 0xC1})) << "MULSD XMM0,XMM1 missing";
    EXPECT_TRUE(Contains(stream, {0xF2, 0x0F, 0x5A, 0xC0})) << "CVTSD2SS XMM0,XMM0 missing";
    EXPECT_TRUE(Contains(stream, {0xF3, 0x0F, 0x5A, 0xC0})) << "CVTSS2SD XMM0,XMM0 missing";
    EXPECT_FALSE(AnyReferenceContains("PowerOOO"))
        << "double ^ must be an exp/log sequence, not a ?PowerOOO DLL call";
}

TEST_F(X64PowerTest, CompiledInt64PowerEmitsRexWNoDllCall)
{
    char prog[] = "dim r as double integer\r\nr=3\r\nr=r^r\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0xF2, 0x48, 0x0F, 0x2A, 0xC0}))
        << "CVTSI2SD XMM0,RAX (REX.W) missing";
    EXPECT_TRUE(Contains(stream, {0xF2, 0x48, 0x0F, 0x2C, 0xC0}))
        << "CVTTSD2SI RAX,XMM0 (REX.W) missing";
    EXPECT_FALSE(AnyReferenceContains("PowerRRR"))
        << "int64 ^ must be an exp/log sequence, not a ?PowerRRR DLL call";
}
