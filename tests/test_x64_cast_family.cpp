// test_x64_cast_family.cpp
//
// Wave 16 — the int64 ("double integer", type 9) <-> float/double cast
// family moves from runtime DLL calls to native SSE2/REG64 instructions:
//   * float  -> int64 : CVTTSS2SI RAX,XMM0   (F3 48 0F 2C C0)
//   * double -> int64 : CVTTSD2SI RAX,XMM0   (F2 48 0F 2C C0)
//   * int64  -> float : CVTSI2SS XMM0,RAX    (F3 48 0F 2A C0)
//   * int64  -> double: CVTSI2SD XMM0,RAX    (F2 48 0F 2A C0)
//   * int64  -> int/dword/byte/word: truncating store at target width
//   * byte/word -> int64: zero-extension (load width does the work)
//
// The 64-bit CVT encodings need the legacy F2/F3 prefix *before* REX.W,
// which the pre-wave emitter could not produce (it wrote REX.W first).
//
// All assertions fail against the current emitter (DLL calls
// ?CastFtoR/?CastOtoR/?CastRtoL/?CastRtoF/?CastRtoO/?CastBtoR, and the
// missing REX.W opcodes).
//
// Design: docs/superpowers/specs/2026-08-11-x64-cast-family-design.md

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

class X64CastFamilyTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_cast_family.log");

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
// New 64-bit CVT opcodes (unit bytes, legacy prefix BEFORE REX.W)
// ---------------------------------------------------------------------------

TEST_F(X64CastFamilyTest, Cvttss2siRaxEmitsPrefixThenRexW)
{
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::CVTTSS2SIRAXXMM0), "");
    ASSERT_GE(bytes.size(), 5u);
    EXPECT_EQ(bytes[0], 0xF3); // legacy prefix first
    EXPECT_EQ(bytes[1], 0x48); // then REX.W
    EXPECT_EQ(bytes[2], 0x0F);
    EXPECT_EQ(bytes[3], 0x2C);
    EXPECT_EQ(bytes[4], 0xC0); // CVTTSS2SI RAX,XMM0
}

TEST_F(X64CastFamilyTest, Cvttsd2siRaxEmitsPrefixThenRexW)
{
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::CVTTSD2SIRAXXMM0), "");
    ASSERT_GE(bytes.size(), 5u);
    EXPECT_EQ(bytes[0], 0xF2);
    EXPECT_EQ(bytes[1], 0x48);
    EXPECT_EQ(bytes[2], 0x0F);
    EXPECT_EQ(bytes[3], 0x2C);
    EXPECT_EQ(bytes[4], 0xC0); // CVTTSD2SI RAX,XMM0
}

TEST_F(X64CastFamilyTest, Cvtsi2ssXmm0RaxEmitsPrefixThenRexW)
{
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::CVTSI2SSXMM0RAX), "");
    ASSERT_GE(bytes.size(), 5u);
    EXPECT_EQ(bytes[0], 0xF3);
    EXPECT_EQ(bytes[1], 0x48);
    EXPECT_EQ(bytes[2], 0x0F);
    EXPECT_EQ(bytes[3], 0x2A);
    EXPECT_EQ(bytes[4], 0xC0); // CVTSI2SS XMM0,RAX
}

TEST_F(X64CastFamilyTest, Cvtsi2sdXmm0RaxEmitsPrefixThenRexW)
{
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::CVTSI2SDXMM0RAX), "");
    ASSERT_GE(bytes.size(), 5u);
    EXPECT_EQ(bytes[0], 0xF2);
    EXPECT_EQ(bytes[1], 0x48);
    EXPECT_EQ(bytes[2], 0x0F);
    EXPECT_EQ(bytes[3], 0x2A);
    EXPECT_EQ(bytes[4], 0xC0); // CVTSI2SD XMM0,RAX
}

// ---------------------------------------------------------------------------
// Task-level: WriteASMTaskCore cast tasks
// ---------------------------------------------------------------------------

TEST_F(X64CastFamilyTest, TaskFloatToInt64EmitsCvttss2siRax)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr f(const_cast<char*>("@f")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastFloatToInt64),
        &f, nullptr, 3, 0, nullptr, nullptr, 0, 0, &r, nullptr, 9, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0xF3, 0x48, 0x0F, 0x2C, 0xC0}))
        << "CVTTSS2SI RAX,XMM0 missing";
    EXPECT_FALSE(AnyReferenceContains("CastFtoR"));
}

TEST_F(X64CastFamilyTest, TaskDoubleToInt64EmitsCvttsd2siRax)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr d(const_cast<char*>("@d")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastDoubleToInt64),
        &d, nullptr, 8, 0, nullptr, nullptr, 0, 0, &r, nullptr, 9, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x48, 0x0F, 0x2C, 0xC0}))
        << "CVTTSD2SI RAX,XMM0 missing";
    EXPECT_FALSE(AnyReferenceContains("CastOtoR"));
}

TEST_F(X64CastFamilyTest, TaskInt64ToLowerEmitsTruncatingStore)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr r(const_cast<char*>("@r")), a(const_cast<char*>("@a"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastInt64ToLower),
        &r, nullptr, 9, 0, nullptr, nullptr, 0, 0, &a, nullptr, 5, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x48, 0xA1})) << "MOV RAX,[mem8] load missing";
    EXPECT_FALSE(AnyReferenceContains("CastRtoL"));
}

TEST_F(X64CastFamilyTest, TaskInt64ToFloatEmitsCvtsi2ssRax)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr r(const_cast<char*>("@r")), f(const_cast<char*>("@f"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastInt64ToFloat),
        &r, nullptr, 9, 0, nullptr, nullptr, 0, 0, &f, nullptr, 3, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0xF3, 0x48, 0x0F, 0x2A, 0xC0}))
        << "CVTSI2SS XMM0,RAX missing";
    EXPECT_FALSE(AnyReferenceContains("CastRtoF"));
}

TEST_F(X64CastFamilyTest, TaskInt64ToDoubleEmitsCvtsi2sdRax)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr r(const_cast<char*>("@r")), d(const_cast<char*>("@d"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastInt64ToDouble),
        &r, nullptr, 9, 0, nullptr, nullptr, 0, 0, &d, nullptr, 8, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0xF2, 0x48, 0x0F, 0x2A, 0xC0}))
        << "CVTSI2SD XMM0,RAX missing";
    EXPECT_FALSE(AnyReferenceContains("CastRtoO"));
}

// ---------------------------------------------------------------------------
// Compiled end-to-end: int64 <-> float/double assignments
// ---------------------------------------------------------------------------

TEST_F(X64CastFamilyTest, CompiledFloatToInt64AssignEmitsCvttss2siRax)
{
    char prog[] = "dim r as double integer\r\ndim f as float\r\nr=f\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0xF3, 0x48, 0x0F, 0x2C, 0xC0}))
        << "CVTTSS2SI RAX,XMM0 missing";
    EXPECT_FALSE(AnyReferenceContains("CastFtoR"))
        << "float->int64 must be native CVTTSS2SI, not a ?CastFtoR DLL call";
}

TEST_F(X64CastFamilyTest, CompiledDoubleToInt64AssignEmitsCvttsd2siRax)
{
    char prog[] = "dim r as double integer\r\ndim d as double float\r\nr=d\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0xF2, 0x48, 0x0F, 0x2C, 0xC0}))
        << "CVTTSD2SI RAX,XMM0 missing";
    EXPECT_FALSE(AnyReferenceContains("CastOtoR"))
        << "double->int64 must be native CVTTSD2SI, not a ?CastOtoR DLL call";
}

TEST_F(X64CastFamilyTest, CompiledInt64ToFloatAssignEmitsCvtsi2ssRax)
{
    char prog[] = "dim r as double integer\r\nf#=r\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0xF3, 0x48, 0x0F, 0x2A, 0xC0}))
        << "CVTSI2SS XMM0,RAX missing";
    EXPECT_FALSE(AnyReferenceContains("CastRtoF"))
        << "int64->float must be native CVTSI2SS, not a ?CastRtoF DLL call";
}

TEST_F(X64CastFamilyTest, CompiledInt64ToDoubleAssignEmitsCvtsi2sdRax)
{
    char prog[] = "dim r as double integer\r\ndim d as double float\r\nd=r\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0xF2, 0x48, 0x0F, 0x2A, 0xC0}))
        << "CVTSI2SD XMM0,RAX missing";
    EXPECT_FALSE(AnyReferenceContains("CastRtoO"))
        << "int64->double must be native CVTSI2SD, not a ?CastRtoO DLL call";
}

TEST_F(X64CastFamilyTest, CompiledInt64ToIntAssignEmitsNoDllCast)
{
    char prog[] = "dim a as integer\r\ndim r as double integer\r\na=r\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x48, 0xA1})) << "MOV RAX,[mem8] load missing";
    EXPECT_FALSE(AnyReferenceContains("CastRtoL"))
        << "int64->int must be a truncating store, not a ?CastRtoL DLL call";
}

TEST_F(X64CastFamilyTest, CompiledByteToInt64AssignEmitsNoDllCast)
{
    char prog[] = "dim r as double integer\r\ndim b as byte\r\nr=b\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_FALSE(AnyReferenceContains("CastBtoR"))
        << "byte->int64 must be zero-extension, not a ?CastBtoR DLL call";
}
