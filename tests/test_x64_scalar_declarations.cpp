// Wave 10 — Scalar typed declarations via DIM.
//
// `dim d as float` (scalar, typed) failed in the pre-scan with ERR_SYNTAX+43
// while `dim d(10) as float` (array) and `global d as float` worked. The
// DoDeclaration DIM branch always required array brackets. These tests pin
// that a scalar name after DIM compiles end to end, both with and without an
// explicit type, and that the array form keeps working.

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

class X64ScalarDeclarationsTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_scalar_declarations.log");
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

    // Compiles a full program: MakeStatements (with a live compiler object,
    // the production shape) + the write phase, returning the patched stream.
    // Returns an empty stream when parsing/writing fails (caller asserts).
    std::vector<uint8_t> CompileProgram(const char* program) {
        char compilerPath[] = "DBPCompiler.exe";
        CDBPCompiler compiler(compilerPath);
        CDBPCompiler* const previousCompiler = g_pDBPCompiler;
        g_pDBPCompiler = &compiler;

        // MakeStatements mutates its input buffer; the literal program string
        // is copied into writable memory.
        const std::size_t len = strlen(program);
        std::vector<char> buf(program, program + len + 1);
        const bool parsed = g_pStatementList->MakeStatements(buf.data(), (DWORD)buf.size());
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

    bool AnyReferenceContains(const char* needle) const {
        const auto& records = m_pWriter->GetReferenceTracker().GetRecords();
        return std::any_of(records.begin(), records.end(),
            [needle](const CReferenceTracker::Record& r) {
                return strstr(r.label.c_str(), needle) != nullptr;
            });
    }
};

// The headline case: a scalar typed float declaration via DIM.
TEST_F(X64ScalarDeclarationsTest, ScalarTypedFloatDimCompiles)
{
    const auto stream = CompileProgram("dim d as float\r\nend\r\n");
    EXPECT_FALSE(stream.empty());
    EXPECT_FALSE(AnyReferenceContains("DimDDD"))
        << "scalar dim must not allocate a runtime array";
    EXPECT_FALSE(AnyReferenceContains("CreateArray"));
}

// Plain scalar dim (no explicit type) must also compile — it previously
// failed through the same bracket-requiring path.
TEST_F(X64ScalarDeclarationsTest, PlainScalarDimCompiles)
{
    const auto stream = CompileProgram("dim d\r\nend\r\n");
    EXPECT_FALSE(stream.empty());
}

// Typed scalar dim with an integer type.
TEST_F(X64ScalarDeclarationsTest, ScalarTypedIntegerDimCompiles)
{
    const auto stream = CompileProgram("dim d as integer\r\nend\r\n");
    EXPECT_FALSE(stream.empty());
}

// Typed scalar dim with a string type.
TEST_F(X64ScalarDeclarationsTest, ScalarTypedStringDimCompiles)
{
    const auto stream = CompileProgram("dim d as string\r\nend\r\n");
    EXPECT_FALSE(stream.empty());
}

// Scalar typed dim with an initializer.
TEST_F(X64ScalarDeclarationsTest, ScalarTypedFloatDimWithInitCompiles)
{
    const auto stream = CompileProgram("dim d as float=1.5\r\nend\r\n");
    EXPECT_FALSE(stream.empty());
}

// Regression: the array form keeps working.
TEST_F(X64ScalarDeclarationsTest, ArrayDimStillCompiles)
{
    const auto stream = CompileProgram("dim d(10) as float\r\nend\r\n");
    EXPECT_FALSE(stream.empty());
    // The array form must still allocate through the runtime DimDDD call
    // (CALL RBX = FF D3) rather than a scalar inline path.
    EXPECT_TRUE(Contains(stream, {0xFF, 0xD3}))
        << "array dim still allocates via the DimDDD runtime call";
}

// Multiple comma-separated scalar declarations compile.
TEST_F(X64ScalarDeclarationsTest, MultiScalarDimCompiles)
{
    const auto stream = CompileProgram("dim a as integer, b as float\r\nend\r\n");
    EXPECT_FALSE(stream.empty());
}

// End to end: assigning a decimal literal to a DIM-declared float stores it
// inline (SSE2/float store, no runtime DLL call) — mirroring wave 8/9.
TEST_F(X64ScalarDeclarationsTest, DimDeclaredFloatAssignmentInlinesStore)
{
    const auto stream = CompileProgram("dim d as float\r\nd=1.5\r\nend\r\n");
    // mov eax, 0x3FC00000 (1.5f) + store
    EXPECT_TRUE(Contains(stream, {0xB8, 0x00, 0x00, 0xC0, 0x3F, 0xA3}))
        << "d=1.5 must inline mov eax,1.5f + store";
    EXPECT_FALSE(AnyReferenceContains("AddFFF"));
    EXPECT_FALSE(AnyReferenceContains("CastLtoF"));
}
