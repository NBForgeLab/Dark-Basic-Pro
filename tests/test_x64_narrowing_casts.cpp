// test_x64_narrowing_casts.cpp
//
// Wave 18 — the narrowing cast family (sources L/F/O/D → targets byte/word/
// dword) moves from dbprocore.dll calls to emitter-native truncating stores:
//   * integer-family source (L/D): 4-byte load, then the target-width store
//     does the truncation — MOV [dst],AL / AX / EAX — matching the C++ casts
//     (unsigned char)/(WORD)/(DWORD);
//   * float-family source (F/O): the truncating CVTTSS2SI/CVTTSD2SI
//     (wave-8 pattern), then the target-width store.
//
// All assertions fail against the current emitter (the 13 rows are still
// ?CastLtoB/?CastFtoB/?CastOtoB/?CastDtoB dbprocore calls, and the
// CastToNarrow/CastFloatToNarrow tasks do not exist).
//
// Design: docs/superpowers/specs/2026-08-11-x64-narrowing-casts-design.md

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

class X64NarrowingCastsTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_narrowing_casts.log");

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
// Task-level: WriteASMTaskCore cast tasks
// ---------------------------------------------------------------------------

TEST_F(X64NarrowingCastsTest, TaskIntToByteEmitsNoDllCast)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastToNarrow),
        &a, nullptr, 4, 0, nullptr, nullptr, 0, 0, &b, nullptr, 5, 0));
    EXPECT_FALSE(AnyReferenceContains("CastLtoB"))
        << "int->byte must be a truncating store, not a ?CastLtoB DLL call";
}

TEST_F(X64NarrowingCastsTest, TaskDwordToByteEmitsNoDllCast)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr d(const_cast<char*>("@d")), b(const_cast<char*>("@b"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastToNarrow),
        &d, nullptr, 7, 0, nullptr, nullptr, 0, 0, &b, nullptr, 5, 0));
    EXPECT_FALSE(AnyReferenceContains("CastDtoB"))
        << "dword->byte must be a truncating store, not a ?CastDtoB DLL call";
}

TEST_F(X64NarrowingCastsTest, TaskFloatToByteEmitsCvttss2si)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr f(const_cast<char*>("@f#")), b(const_cast<char*>("@b"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastFloatToNarrow),
        &f, nullptr, 2, 0, nullptr, nullptr, 0, 0, &b, nullptr, 5, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0xF3, 0x0F, 0x2C, 0xC0})) << "CVTTSS2SI EAX,XMM0 missing";
    EXPECT_FALSE(AnyReferenceContains("CastFtoB"))
        << "float->byte must be CVTTSS2SI + truncating store, not a ?CastFtoB DLL call";
}

TEST_F(X64NarrowingCastsTest, TaskDoubleToByteEmitsCvttsd2si)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr d(const_cast<char*>("@d")), b(const_cast<char*>("@b"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastFloatToNarrow),
        &d, nullptr, 8, 0, nullptr, nullptr, 0, 0, &b, nullptr, 5, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x0F, 0x2C, 0xC0})) << "CVTTSD2SI EAX,XMM0 missing";
    EXPECT_FALSE(AnyReferenceContains("CastOtoB"))
        << "double->byte must be CVTTSD2SI + truncating store, not a ?CastOtoB DLL call";
}

// ---------------------------------------------------------------------------
// Compiled end-to-end
// ---------------------------------------------------------------------------

TEST_F(X64NarrowingCastsTest, CompiledIntToByteEmitsNoDllCast)
{
    char prog[] = "dim b as byte\r\ndim a as integer\r\nb=a\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_FALSE(AnyReferenceContains("CastLtoB"))
        << "int->byte must be a truncating store, not a ?CastLtoB DLL call";
}

TEST_F(X64NarrowingCastsTest, CompiledIntToWordEmitsNoDllCast)
{
    char prog[] = "dim w as word\r\ndim a as integer\r\nw=a\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_FALSE(AnyReferenceContains("CastLtoW"))
        << "int->word must be a truncating store, not a ?CastLtoW DLL call";
}

TEST_F(X64NarrowingCastsTest, CompiledIntToDwordEmitsNoDllCast)
{
    char prog[] = "dim d as dword\r\ndim a as integer\r\nd=a\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_FALSE(AnyReferenceContains("CastLtoD"))
        << "int->dword must be a same-width move, not a ?CastLtoD DLL call";
}

TEST_F(X64NarrowingCastsTest, CompiledFloatToByteEmitsCvttss2siNoDllCast)
{
    char prog[] = "dim b as byte\r\nf#=5\r\nb=f#\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0xF3, 0x0F, 0x2C, 0xC0})) << "CVTTSS2SI EAX,XMM0 missing";
    EXPECT_FALSE(AnyReferenceContains("CastFtoB"))
        << "float->byte must be native, not a ?CastFtoB DLL call";
}

TEST_F(X64NarrowingCastsTest, CompiledDoubleToByteEmitsCvttsd2siNoDllCast)
{
    char prog[] = "dim b as byte\r\ndim dd as double float\r\nb=dd\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0xF2, 0x0F, 0x2C, 0xC0})) << "CVTTSD2SI EAX,XMM0 missing";
    EXPECT_FALSE(AnyReferenceContains("CastOtoB"))
        << "double->byte must be native, not a ?CastOtoB DLL call";
}

TEST_F(X64NarrowingCastsTest, CompiledDwordToByteEmitsNoDllCast)
{
    char prog[] = "dim b as byte\r\ndim dw as dword\r\nb=dw\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_FALSE(AnyReferenceContains("CastDtoB"))
        << "dword->byte must be a truncating store, not a ?CastDtoB DLL call";
}

// ---------------------------------------------------------------------------
// Closing the last arithmetic cast row: dword -> int (same-width move)
// ---------------------------------------------------------------------------

TEST_F(X64NarrowingCastsTest, TaskDwordToIntEmitsNoDllCast)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr d(const_cast<char*>("@d")), a(const_cast<char*>("@a"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastToNarrow),
        &d, nullptr, 7, 0, nullptr, nullptr, 0, 0, &a, nullptr, 4, 0));
    EXPECT_FALSE(AnyReferenceContains("CastDtoL"))
        << "dword->int must be a same-width move, not a ?CastDtoL DLL call";
}

TEST_F(X64NarrowingCastsTest, CompiledDwordToIntEmitsNoDllCast)
{
    // A plain `a=dw` assignment is a same-width move with no cast op; the
    // D->L cast is produced when a dword operand rides an int expression.
    char prog[] = "dim a as integer\r\ndim b as integer\r\ndim dw as dword\r\na=b+dw\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_FALSE(AnyReferenceContains("CastDtoL"))
        << "dword->int must be a same-width move, not a ?CastDtoL DLL call";
}
