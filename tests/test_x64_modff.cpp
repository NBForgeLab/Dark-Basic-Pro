// test_x64_modff.cpp
//
// Wave 21 — the float `mod` operator (ModFFF) moves from the last scalar
// dbprocore.dll call (?ModFFF@@YAKMM@Z) to an emitter-built CRT fmod call:
//   if (B == ±0.0f) result = 0.0f                  (sign-stripped bit test)
//   else result = (float)fmod((double)A, (double)B)
// The fmod primitive (undecorated msvcrt.dll export) is resolved through
// the command table exactly like the wave-17 exp/log sequence.
//
// All assertions fail against the current emitter (ModFFF is still a
// ?ModFFF dbprocore call, and the float-mod branch does not exist).
//
// Design: docs/superpowers/specs/2026-08-11-x64-modff-design.md

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

class X64ModFfTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_modff.log");

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
// Task-level: WriteASMTaskCore(Mod) with float operands
// ---------------------------------------------------------------------------

TEST_F(X64ModFfTest, TaskFloatModEmitsFmodSequenceNoDllCast)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr b(const_cast<char*>("@b#")), c(const_cast<char*>("@c#")), a(const_cast<char*>("@a#"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Mod),
        &b, nullptr, 2, 0, &c, nullptr, 2, 0, &a, nullptr, 2, 0));
    const auto bytes = BytesSince(before);

    // Zero-divisor guard: AND EAX,0x7FFFFFFF (25 FF FF FF 7F little-endian).
    EXPECT_TRUE(Contains(bytes, {0x25, 0xFF, 0xFF, 0xFF, 0x7F}))
        << "AND EAX,0x7FFFFFFF guard missing";
    // Widen both operands float -> double.
    EXPECT_GE(std::count(bytes.begin(), bytes.end(), 0xF3), 2);
    EXPECT_TRUE(Contains(bytes, {0xF3, 0x0F, 0x5A, 0xC0})) << "CVTSS2SD XMM0,XMM0 missing";
    // Double second argument load: MOVSD XMM1,XMM0.
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x10, 0xC8})) << "MOVSD XMM1,XMM0 missing";
    // Two aligned call frames (fmod has one, like log/exp).
    EXPECT_TRUE(Contains(bytes, {0x48, 0x83, 0xEC})) << "SUB RSP,imm frame missing";
    EXPECT_TRUE(Contains(bytes, {0x48, 0x83, 0xC4})) << "ADD RSP,imm frame missing";
    // The indirect call itself.
    EXPECT_TRUE(Contains(bytes, {0xFF, 0xD3})) << "CALL EBX missing";
    // Narrow the double result back to float.
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x5A, 0xC0})) << "CVTSD2SS XMM0,XMM0 missing";

    // The CRT primitive is registered in the DLL/command tables.
    EXPECT_GT(g_pDLLTable->FindString("msvcrt.dll"), 0u)
        << "msvcrt.dll must be added to the DLL table";
    EXPECT_TRUE(TableContains(g_pCommandTable, ",fmod")) << "fmod command not registered";

    // No dbprocore Mod call.
    EXPECT_FALSE(AnyReferenceContains("ModFFF"))
        << "float mod must be an fmod sequence, not a ?ModFFF DLL call";
}

// ---------------------------------------------------------------------------
// Compiled end-to-end
// ---------------------------------------------------------------------------

TEST_F(X64ModFfTest, CompiledFloatModEmitsFmodNoDllCall)
{
    char prog[] = "a#=b# mod c#\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x25, 0xFF, 0xFF, 0xFF, 0x7F}))
        << "AND EAX,0x7FFFFFFF guard missing";
    EXPECT_TRUE(Contains(stream, {0xF3, 0x0F, 0x5A, 0xC0})) << "CVTSS2SD XMM0,XMM0 missing";
    EXPECT_TRUE(Contains(stream, {0xF2, 0x0F, 0x5A, 0xC0})) << "CVTSD2SS XMM0,XMM0 missing";
    EXPECT_TRUE(Contains(stream, {0xFF, 0xD3})) << "CALL EBX missing";
    EXPECT_TRUE(TableContains(g_pCommandTable, ",fmod")) << "fmod command not registered";
    EXPECT_FALSE(AnyReferenceContains("ModFFF"))
        << "float mod must be an fmod sequence, not a ?ModFFF DLL call";
}

TEST_F(X64ModFfTest, CompiledFloatModZeroDivisorEmitsGuardNoDllCall)
{
    // The zero-divisor guard must be present even in a compiled program
    // whose divisor is known to be a variable.
    char prog[] = "dim a as float\r\ndim b as float\r\ndim c as float\r\nb=0\r\na=c mod b\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x25, 0xFF, 0xFF, 0xFF, 0x7F}))
        << "AND EAX,0x7FFFFFFF guard missing";
    EXPECT_FALSE(AnyReferenceContains("ModFFF"))
        << "float mod must be an fmod sequence, not a ?ModFFF DLL call";
}
