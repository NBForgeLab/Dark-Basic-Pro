// Wave 12 — Decouple CMathOp::IsLiteral from the global compiler object.
//
// The literal preference (double vs float) used to be read from the global
// `g_pDBPCompiler->m_bDoubleLiterals` deep inside CMathOp::IsLiteral. That is
// a hidden coupling to a global that may be null in embedded/test hosts (hence
// the old null-guard). These tests pin that the preference now flows in as a
// parameter through the call chain: IsLiteral -> DoValue -> DoExpression ->
// MakeStatements(..., bDoubleLiterals).

#include "gtest/gtest.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

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

class X64LiteralPreferenceTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_literal_preference.log");
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

    void PatchAll(std::vector<uint8_t>& code) const {
        std::vector<uintptr_t> values;
        std::vector<DWORD> positions;
        std::vector<DWORD> types;
        for (const auto& record : m_pWriter->GetReferenceTracker().GetRecords())
        {
            const auto parsed = ParseReferenceLabel(record.label);
            ASSERT_TRUE(parsed.has_value())
                << "unparseable reference: " << record.label;
            values.push_back(parsed->index);
            positions.push_back(record.machineCodeOffset);
            types.push_back(static_cast<DWORD>(parsed->kind));
        }
        CEXEBlock::PatchReferenceValues(
            values.data(), values.size(),
            positions.data(), types.data(),
            reinterpret_cast<char*>(code.data()));
    }

    // Compiles a full program with an explicit literal preference, returning
    // the patched machine-code stream (empty when parsing/writing fails).
    std::vector<uint8_t> CompileProgram(const char* program,
                                        bool bDoubleLiterals) {
        char compilerPath[] = "DBPCompiler.exe";
        CDBPCompiler compiler(compilerPath);
        CDBPCompiler* const previousCompiler = g_pDBPCompiler;
        g_pDBPCompiler = &compiler;

        const std::size_t len = strlen(program);
        std::vector<char> buf(program, program + len + 1);
        const bool parsed = g_pStatementList->MakeStatements(
            buf.data(), (DWORD)buf.size(), bDoubleLiterals);
        if (!parsed) {
            g_pDBPCompiler = previousCompiler;
            return {};
        }

        g_pStatementList->SetWriteStarted(true);
        if (!g_pStatementList->GetProgramStatements()->WriteDBM()) {
            g_pDBPCompiler = previousCompiler;
            return {};
        }
        if (!g_pStatementList->GetPreScanStatements()->WriteDBM()) {
            g_pDBPCompiler = previousCompiler;
            return {};
        }
        g_pDBPCompiler = previousCompiler;

        const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
        std::vector<uint8_t> stream(raw, raw + m_pWriter->GetCurrentMCPosition());
        PatchAll(stream);
        return stream;
    }
};

// --- Unit level: IsLiteral takes the preference as a parameter. -------------

// A fractional literal resolves to DOUBLE (type 8) when doubles are requested.
TEST_F(X64LiteralPreferenceTest, IsLiteralPrefersDoubleWhenRequested)
{
    CMathOp op;
    CStr expr("1.5");
    DWORD type = 0;
    ASSERT_TRUE(op.IsLiteral(&expr, &type, true));
    EXPECT_EQ(type, 8u);
}

// The same literal resolves to FLOAT (type 2) by default.
TEST_F(X64LiteralPreferenceTest, IsLiteralPrefersFloatByDefault)
{
    CMathOp op;
    CStr expr("1.5");
    DWORD type = 0;
    ASSERT_TRUE(op.IsLiteral(&expr, &type, false));
    EXPECT_EQ(type, 2u);
}

// Integer literals are unaffected by the preference.
TEST_F(X64LiteralPreferenceTest, IsLiteralIntegerUnaffectedByPreference)
{
    CMathOp op;
    CStr expr("42");
    DWORD type = 0;
    ASSERT_TRUE(op.IsLiteral(&expr, &type, true));
    EXPECT_EQ(type, 1u);
    ASSERT_TRUE(op.IsLiteral(&expr, &type, false));
    EXPECT_EQ(type, 1u);
}

// --- Unit level: DoValue threads the preference through to IsLiteral. -------

// DoValue("1.5", true) must resolve the fractional literal as a DOUBLE.
TEST_F(X64LiteralPreferenceTest, DoValueThreadsDoublePreference)
{
    CMathOp op;
    CStr expr("1.5");
    ASSERT_TRUE(op.DoValue(&expr, true));
    EXPECT_EQ(op.GetResultType(), 8u);
}

// DoValue("1.5", false) must resolve it as a FLOAT.
TEST_F(X64LiteralPreferenceTest, DoValueThreadsFloatPreference)
{
    CMathOp op;
    CStr expr("1.5");
    ASSERT_TRUE(op.DoValue(&expr, false));
    EXPECT_EQ(op.GetResultType(), 2u);
}

// --- Independence from the global compiler object. --------------------------

// With the global compiler pointer forced to null, IsLiteral/DoValue must keep
// working (they take the preference as a parameter — no global read left).
TEST_F(X64LiteralPreferenceTest, IsLiteralIndependentOfGlobalCompiler)
{
    CDBPCompiler* const previousCompiler = g_pDBPCompiler;
    g_pDBPCompiler = nullptr;

    CMathOp op;
    CStr expr("1.5");
    DWORD type = 0;
    ASSERT_TRUE(op.IsLiteral(&expr, &type, true));
    EXPECT_EQ(type, 8u);
    ASSERT_TRUE(op.IsLiteral(&expr, &type, false));
    EXPECT_EQ(type, 2u);

    CStr expr2("2.5");
    ASSERT_TRUE(op.DoValue(&expr2, true));
    EXPECT_EQ(op.GetResultType(), 8u);

    g_pDBPCompiler = previousCompiler;
}

// --- Integration: MakeStatements(..., bDoubleLiterals) reaches the emitter. --

// With doubles requested, `d=1.5` must NOT emit the 32-bit float immediate
// (mov eax, 1.5f + store) that the float path produces; it goes through the
// SSE2 double path instead (MOVSD F2 0F 10).
TEST_F(X64LiteralPreferenceTest, MakeStatementsDoublePreferenceChangesEmission)
{
    const auto stream = CompileProgram(
        "dim d as float\r\nd=1.5\r\nend\r\n", true);
    ASSERT_FALSE(stream.empty());
    // The float path is mov eax, 0x3FC00000 (1.5f) + store (wave 10 shape).
    EXPECT_FALSE(Contains(stream, {0xB8, 0x00, 0x00, 0xC0, 0x3F}))
        << "double preference must not emit mov eax,1.5f";
    // The double path loads via MOVSD XMM0, m64 (F2 0F 10 /r).
    EXPECT_TRUE(Contains(stream, {0xF2, 0x0F, 0x10}))
        << "double preference must go through the SSE2 MOVSD path";
}

// With the default preference (false), the float immediate path is kept —
// regression against the wave-10 emission shape.
TEST_F(X64LiteralPreferenceTest, MakeStatementsDefaultKeepsFloatEmission)
{
    const auto stream = CompileProgram(
        "dim d as float\r\nd=1.5\r\nend\r\n", false);
    ASSERT_FALSE(stream.empty());
    EXPECT_TRUE(Contains(stream, {0xB8, 0x00, 0x00, 0xC0, 0x3F}))
        << "default preference keeps mov eax,1.5f + store";
}
