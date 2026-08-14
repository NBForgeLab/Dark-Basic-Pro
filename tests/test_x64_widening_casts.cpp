// test_x64_widening_casts.cpp
//
// Wave 19 — the widening cast family (sources boolean/byte/word → targets
// long/word/dword/float/double) moves from dbprocore.dll calls to
// emitter-native zero-extension:
//   * boolean/byte (B/Y) and word (W) are unsigned sources (the Y rows share
//     the B DLL entries `?CastBtoL@@YAKE@Z`, W uses `?CastWtoL@@YAKG@Z`);
//   * the generic emitter load is `MOV AL/AX` which preserves the upper
//     bits of EAX, so a width extension needs an explicit MOVZX:
//       0F B6 C0  movzx eax, al   (byte/boolean source)
//       0F B7 C0  movzx eax, ax   (word source)
//   * integer-family targets (L/W/D) store the zero-extended value at the
//     target width; float/double targets convert via the wave-8
//     CVTSI2SS/CVTSI2SD pattern.
//   * W→B/Y stay truncations and reuse the wave-18 CastToNarrow task.
//
// All assertions fail against the current emitter (the 16 rows are still
// ?CastBto* / ?CastYto* / ?CastWto* dbprocore calls, and the
// CastWiden/CastWidenToFloat tasks do not exist).
//
// Design: docs/superpowers/specs/2026-08-11-x64-widening-casts-design.md

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

class X64WideningCastsTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_widening_casts.log");

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
// Task-level: WriteASMTaskCore widening tasks
// ---------------------------------------------------------------------------

TEST_F(X64WideningCastsTest, TaskByteToIntEmitsMovzxNoDllCast)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr b(const_cast<char*>("@b")), a(const_cast<char*>("@a"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastWiden),
        &b, nullptr, 5, 0, nullptr, nullptr, 0, 0, &a, nullptr, 1, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x0F, 0xB6, 0xC0})) << "MOVZX EAX,AL missing";
    EXPECT_FALSE(AnyReferenceContains("CastBtoL"))
        << "byte->int must be MOVZX, not a ?CastBtoL DLL call";
}

TEST_F(X64WideningCastsTest, TaskWordToDwordEmitsMovzxNoDllCast)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr w(const_cast<char*>("@w")), d(const_cast<char*>("@d"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastWiden),
        &w, nullptr, 6, 0, nullptr, nullptr, 0, 0, &d, nullptr, 7, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x0F, 0xB7, 0xC0})) << "MOVZX EAX,AX missing";
    EXPECT_FALSE(AnyReferenceContains("CastWtoD"))
        << "word->dword must be MOVZX, not a ?CastWtoD DLL call";
}

TEST_F(X64WideningCastsTest, TaskByteToFloatEmitsMovzxCvtsi2ssNoDllCast)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr b(const_cast<char*>("@b")), f(const_cast<char*>("@f#"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastWidenToFloat),
        &b, nullptr, 5, 0, nullptr, nullptr, 0, 0, &f, nullptr, 2, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x0F, 0xB6, 0xC0})) << "MOVZX EAX,AL missing";
    EXPECT_TRUE(Contains(bytes, {0xF3, 0x0F, 0x2A, 0xC0}))
        << "CVTSI2SS XMM0,EAX missing";
    EXPECT_FALSE(AnyReferenceContains("CastBtoF"))
        << "byte->float must be MOVZX + CVTSI2SS, not a ?CastBtoF DLL call";
}

TEST_F(X64WideningCastsTest, TaskWordToDoubleEmitsMovzxCvtsi2sdNoDllCast)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr w(const_cast<char*>("@w")), d(const_cast<char*>("@d"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastWidenToFloat),
        &w, nullptr, 6, 0, nullptr, nullptr, 0, 0, &d, nullptr, 8, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x0F, 0xB7, 0xC0})) << "MOVZX EAX,AX missing";
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x2A, 0xC0}))
        << "CVTSI2SD XMM0,EAX missing";
    EXPECT_FALSE(AnyReferenceContains("CastWtoO"))
        << "word->double must be MOVZX + CVTSI2SD, not a ?CastWtoO DLL call";
}

// ---------------------------------------------------------------------------
// Compiled end-to-end
// ---------------------------------------------------------------------------

TEST_F(X64WideningCastsTest, CompiledByteToIntEmitsMovzxNoDllCast)
{
    char prog[] = "dim a as integer\r\ndim b as byte\r\na=b\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x0F, 0xB6, 0xC0})) << "MOVZX EAX,AL missing";
    EXPECT_FALSE(AnyReferenceContains("CastBtoL"))
        << "byte->int must be MOVZX, not a ?CastBtoL DLL call";
}

TEST_F(X64WideningCastsTest, CompiledWordToDwordEmitsMovzxNoDllCast)
{
    char prog[] = "dim d as dword\r\ndim w as word\r\nd=w\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x0F, 0xB7, 0xC0})) << "MOVZX EAX,AX missing";
    EXPECT_FALSE(AnyReferenceContains("CastWtoD"))
        << "word->dword must be MOVZX, not a ?CastWtoD DLL call";
}

TEST_F(X64WideningCastsTest, CompiledByteToFloatEmitsMovzxCvtsi2ssNoDllCast)
{
    char prog[] = "dim b as byte\r\nb=200\r\nf#=b\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x0F, 0xB6, 0xC0})) << "MOVZX EAX,AL missing";
    EXPECT_TRUE(Contains(stream, {0xF3, 0x0F, 0x2A, 0xC0}))
        << "CVTSI2SS XMM0,EAX missing";
    EXPECT_FALSE(AnyReferenceContains("CastBtoF"))
        << "byte->float must be native, not a ?CastBtoF DLL call";
}

TEST_F(X64WideningCastsTest, CompiledWordToDoubleEmitsMovzxCvtsi2sdNoDllCast)
{
    char prog[] = "dim dd as double float\r\ndim w as word\r\nw=40000\r\ndd=w\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x0F, 0xB7, 0xC0})) << "MOVZX EAX,AX missing";
    EXPECT_TRUE(Contains(stream, {0xF2, 0x0F, 0x2A, 0xC0}))
        << "CVTSI2SD XMM0,EAX missing";
    EXPECT_FALSE(AnyReferenceContains("CastWtoO"))
        << "word->double must be native, not a ?CastWtoO DLL call";
}

TEST_F(X64WideningCastsTest, CompiledByteToWordEmitsMovzxNoDllCast)
{
    char prog[] = "dim w as word\r\ndim b as byte\r\nw=b\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x0F, 0xB6, 0xC0})) << "MOVZX EAX,AL missing";
    EXPECT_FALSE(AnyReferenceContains("CastBtoW"))
        << "byte->word must be MOVZX, not a ?CastBtoW DLL call";
}

TEST_F(X64WideningCastsTest, CompiledWordToByteReusesTruncationNoDllCast)
{
    // W->B/Y are pure truncations: the wave-18 CastToNarrow store-width
    // truncation covers them (MOV AX load, AL store).
    char prog[] = "dim b as byte\r\ndim w as word\r\nb=w\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_FALSE(AnyReferenceContains("CastWtoB"))
        << "word->byte must be a truncating store, not a ?CastWtoB DLL call";
}

TEST_F(X64WideningCastsTest, CompiledBooleanToIntEmitsNoDllCast)
{
    char prog[] = "dim a as integer\r\ndim fl as boolean\r\nfl=1\r\na=fl\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x0F, 0xB6, 0xC0})) << "MOVZX EAX,AL missing";
    EXPECT_FALSE(AnyReferenceContains("CastBtoL"))
        << "boolean->int must be MOVZX, not a ?CastBtoL DLL call";
}

TEST_F(X64WideningCastsTest, CompiledByteToInt64EmitsNoDllCast)
{
    // Wave 16 already made byte->int64 native; ensure it stays native with
    // no ?CastBtoR reference after this wave's changes.
    char prog[] = "dim r as double integer\r\ndim b as byte\r\nr=b\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_FALSE(AnyReferenceContains("CastBtoR"))
        << "byte->int64 must stay native, not a ?CastBtoR DLL call";
}
