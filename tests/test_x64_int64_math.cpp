// test_x64_int64_math.cpp
//
// Wave 8b — int64 ("double integer", type 9) arithmetic moves from runtime
// DLL calls to full-width REG64 in the emitter:
//   * type-9 loads/stores become single 8-byte moves (MOVEAXMEM8/MOVMEMEAX8);
//   * Add/Sub/Mul/Div/Mod are emitted inline as ADD/SUB/IMUL/IDIV RAX,RBX
//     (CQO for division, MOV RAX,RDX for the remainder) instead of
//     ?AddRRR/?SubRRR/?MulRRR/?DivRRR dbprocore.dll calls;
//   * comparisons emit CMP RDX,RBX + SETcc instead of ?EqualLRR/... calls.
//
// All assertions fail against the current emitter (ERR_SYNTAX+50 on hardcoded
// type-9 math, DLL calls for int64 arithmetic, two 4-byte half moves).
//
// Design: docs/superpowers/specs/2026-08-11-x64-int64-reg64-design.md

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

class X64Int64MathTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_int64_math.log");

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
// New 8-byte opcodes (unit bytes)
// ---------------------------------------------------------------------------

TEST_F(X64Int64MathTest, AddRaxRbxEmitsRexWAdd)
{
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::ADDEAXEBX8), "");
    ASSERT_GE(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0x48); // REX.W
    EXPECT_EQ(bytes[1], 0x01);
    EXPECT_EQ(bytes[2], 0xD8); // ADD RAX,RBX
}

TEST_F(X64Int64MathTest, SubRaxRbxEmitsRexWSub)
{
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::SUBEAXEBX8), "");
    ASSERT_GE(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x29);
    EXPECT_EQ(bytes[2], 0xD8); // SUB RAX,RBX
}

TEST_F(X64Int64MathTest, MulRaxRbxEmitsRexWImul)
{
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MULEAXEBX8), "");
    ASSERT_GE(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x0F);
    EXPECT_EQ(bytes[2], 0xAF);
    EXPECT_EQ(bytes[3], 0xD8); // IMUL RAX,RBX
}

TEST_F(X64Int64MathTest, CqoEmitsRexWSignExtend)
{
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::CQO), "");
    ASSERT_GE(bytes.size(), 2u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x99); // CQO
}

TEST_F(X64Int64MathTest, DivRaxRbxEmitsRexWIdiv)
{
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::DIVEAXEBX8), "");
    ASSERT_GE(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0xF7);
    EXPECT_EQ(bytes[2], 0xFB); // IDIV RBX
}

TEST_F(X64Int64MathTest, MovRaxRdxEmitsRexW)
{
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVEAXEDX8), "");
    ASSERT_GE(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x8B);
    EXPECT_EQ(bytes[2], 0xC2); // MOV RAX,RDX
}

TEST_F(X64Int64MathTest, MovRbxRaxEmitsRexW)
{
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVEBXRAX8), "");
    ASSERT_GE(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x8B);
    EXPECT_EQ(bytes[2], 0xD8); // MOV RBX,RAX
}

TEST_F(X64Int64MathTest, CmpRdxRbxEmitsRexW)
{
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::CMPEDXEBX8), "");
    ASSERT_GE(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x3B);
    EXPECT_EQ(bytes[2], 0xDA); // CMP RDX,RBX
}

// ---------------------------------------------------------------------------
// Task-level: WriteASMTaskCore on type 9 operands
// ---------------------------------------------------------------------------

TEST_F(X64Int64MathTest, TaskInt64AddEmitsAddRaxRbx)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Add),
        &a, nullptr, 9, 0, &b, nullptr, 9, 0, &r, nullptr, 9, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x48, 0x01, 0xD8})) << "ADD RAX,RBX missing";
    EXPECT_FALSE(AnyReferenceContains("AddRRR"));
}

TEST_F(X64Int64MathTest, TaskInt64SubEmitsSubRaxRbx)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Sub),
        &a, nullptr, 9, 0, &b, nullptr, 9, 0, &r, nullptr, 9, 0));
    EXPECT_TRUE(Contains(BytesSince(before), {0x48, 0x29, 0xD8}));
}

TEST_F(X64Int64MathTest, TaskInt64MulEmitsImulRaxRbx)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Mul),
        &a, nullptr, 9, 0, &b, nullptr, 9, 0, &r, nullptr, 9, 0));
    EXPECT_TRUE(Contains(BytesSince(before), {0x48, 0x0F, 0xAF, 0xD8}));
}

TEST_F(X64Int64MathTest, TaskInt64DivEmitsCqoIdiv)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Div),
        &a, nullptr, 9, 0, &b, nullptr, 9, 0, &r, nullptr, 9, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x48, 0x99})) << "CQO missing";
    EXPECT_TRUE(Contains(bytes, {0x48, 0xF7, 0xFB})) << "IDIV RBX missing";
}

TEST_F(X64Int64MathTest, TaskInt64ModEmitsMovRaxRdx)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Mod),
        &a, nullptr, 9, 0, &b, nullptr, 9, 0, &r, nullptr, 9, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x48, 0x99})) << "CQO missing";
    EXPECT_TRUE(Contains(bytes, {0x48, 0xF7, 0xFB})) << "IDIV RBX missing";
    EXPECT_TRUE(Contains(bytes, {0x48, 0x8B, 0xC2})) << "MOV RAX,RDX (mod) missing";
}

TEST_F(X64Int64MathTest, TaskInt64EqualEmitsCmpRdxRbxSete)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Equal),
        &a, nullptr, 9, 0, &b, nullptr, 9, 0, &r, nullptr, 1, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x48, 0x3B, 0xDA})) << "CMP RDX,RBX missing";
    EXPECT_FALSE(AnyReferenceContains("EqualLRR"));
}

TEST_F(X64Int64MathTest, TaskInt64GreaterEmitsSeteSetg)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Greater),
        &a, nullptr, 9, 0, &b, nullptr, 9, 0, &r, nullptr, 1, 0));
    const auto bytes = BytesSince(before);
    EXPECT_TRUE(Contains(bytes, {0x48, 0x3B, 0xDA}));
    EXPECT_FALSE(AnyReferenceContains("GreaterLRR"));
}

TEST_F(X64Int64MathTest, TaskInt64LessEmitsCmpRdxRbxSetl)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), b(const_cast<char*>("@b")), r(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Less),
        &a, nullptr, 9, 0, &b, nullptr, 9, 0, &r, nullptr, 1, 0));
    EXPECT_TRUE(Contains(BytesSince(before), {0x48, 0x3B, 0xDA}));
}

// ---------------------------------------------------------------------------
// Compiled end-to-end: "double integer" programs
// ---------------------------------------------------------------------------

TEST_F(X64Int64MathTest, CompiledInt64AddEmitsAddRaxRbxAndNoDllCall)
{
    char prog[] = "dim a as double integer\r\ndim b as double integer\r\ndim c as double integer\r\na=b+c\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();

    EXPECT_TRUE(Contains(stream, {0x48, 0x01, 0xD8})) << "ADD RAX,RBX missing";
    EXPECT_FALSE(AnyReferenceContains("AddRRR"))
        << "int64 + must be hardcoded REG64, not a dbprocore AddRRR call";
}

TEST_F(X64Int64MathTest, CompiledInt64SubEmitsSubRaxRbx)
{
    char prog[] = "dim a as double integer\r\ndim b as double integer\r\ndim c as double integer\r\na=b-c\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_TRUE(Contains(Compile(), {0x48, 0x29, 0xD8})); // SUB RAX,RBX
}

TEST_F(X64Int64MathTest, CompiledInt64MulEmitsImulRaxRbx)
{
    char prog[] = "dim a as double integer\r\ndim b as double integer\r\ndim c as double integer\r\na=b*c\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_TRUE(Contains(Compile(), {0x48, 0x0F, 0xAF, 0xD8})); // IMUL RAX,RBX
}

TEST_F(X64Int64MathTest, CompiledInt64DivEmitsCqoIdivAndNoDllCall)
{
    char prog[] = "dim a as double integer\r\ndim b as double integer\r\ndim c as double integer\r\na=b/c\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x48, 0x99})) << "CQO missing";
    EXPECT_TRUE(Contains(stream, {0x48, 0xF7, 0xFB})) << "IDIV RBX missing";
    EXPECT_FALSE(AnyReferenceContains("DivRRR"))
        << "int64 / must be hardcoded REG64, not a dbprocore DivRRR call";
}

TEST_F(X64Int64MathTest, CompiledInt64ModEmitsIdivAndMovRaxRdx)
{
    char prog[] = "dim a as double integer\r\ndim b as double integer\r\ndim c as double integer\r\na=b mod c\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x48, 0x99})) << "CQO missing";
    EXPECT_TRUE(Contains(stream, {0x48, 0xF7, 0xFB})) << "IDIV RBX missing";
    EXPECT_TRUE(Contains(stream, {0x48, 0x8B, 0xC2})) << "MOV RAX,RDX missing";
    EXPECT_FALSE(AnyReferenceContains("DivRRR"));
}

TEST_F(X64Int64MathTest, CompiledInt64CompareEmitsCmpRdxRbx)
{
    char prog[] = "dim a as double integer\r\ndim b as double integer\r\ndim r as integer\r\nr=a>b\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x48, 0x3B, 0xDA})) << "CMP RDX,RBX missing";
    EXPECT_FALSE(AnyReferenceContains("GreaterLRR"))
        << "int64 compare must be hardcoded CMP+SETcc, not a DLL call";
}

TEST_F(X64Int64MathTest, CompiledInt64AssignUsesSingle8ByteMove)
{
    char prog[] = "dim a as double integer\r\ndim b as double integer\r\na=b\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    // Single 8-byte load (48 A1 moffs64) and single 8-byte store (48 A3 moffs64).
    EXPECT_TRUE(Contains(stream, {0x48, 0xA1})) << "MOV RAX,[mem8] missing";
    EXPECT_TRUE(Contains(stream, {0x48, 0xA3})) << "MOV [mem8],RAX missing";
}
