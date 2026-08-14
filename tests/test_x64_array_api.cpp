// Wave 7 — Full-width runtime array-pointer API.
//
// The runtime array functions widen their array-pointer params/returns from
// DWORD to uintptr_t, so:
//   * the compiler resolves the new decorated names
//     (?DimDDD@@YA_K_KKKKKKKKKK@Z / ?UnDimDD@@YA_K_K@Z);
//   * array-pointer values use a dedicated full-width DBM type (1002) whose
//     memory loads/stores move 8 bytes (48 A1 / 48 A3 / 48 8B 85 / 48 89 85),
//     so the pointer returned by DimDDD lands in the varspace slot unclipped
//     and the old pointer is re-pushed unclipped on REDIM;
//   * a compiled `dim a(10)` stores full RAX after the call, and `undim a`
//     loads + clears the slot at full width.
//
// All assertions fail against the wave-6 compiler (32-bit decorated names,
// type 1002 falls into the 4-byte DWORD path, A3 stores clip the pointer).

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

std::size_t Count(const std::vector<uint8_t>& bytes,
                  const std::vector<uint8_t>& needle)
{
    std::size_t n = 0;
    for (std::size_t i = 0; i + needle.size() <= bytes.size(); ++i)
    {
        if (std::equal(needle.begin(), needle.end(), bytes.begin() + i)) ++n;
    }
    return n;
}
} // namespace

class X64ArrayApiTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_array_api.log");

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
};

// ---------------------------------------------------------------------------
// New decorated names for the widened runtime signatures
// ---------------------------------------------------------------------------

TEST_F(X64ArrayApiTest, InstructionTableResolvesX64ArrayDecoratedNames)
{
    auto* pAlloc = g_pInstructionTable->GetRef(
        static_cast<DWORD>(InternalInstruction::Alloc));
    ASSERT_NE(pAlloc, nullptr);
    ASSERT_NE(pAlloc->GetDecoratedName(), nullptr);
    EXPECT_STREQ(pAlloc->GetDecoratedName()->GetStr(),
                 "?DimDDD@@YA_K_KKKKKKKKKK@Z");

    auto* pFree = g_pInstructionTable->GetRef(
        static_cast<DWORD>(InternalInstruction::Free));
    ASSERT_NE(pFree, nullptr);
    ASSERT_NE(pFree->GetDecoratedName(), nullptr);
    EXPECT_STREQ(pFree->GetDecoratedName()->GetStr(),
                 "?UnDimDD@@YA_K_K@Z");
}

// ---------------------------------------------------------------------------
// Type 1002 (full-width pointer value) memory access is 8-byte
// ---------------------------------------------------------------------------

TEST_F(X64ArrayApiTest, FullWidthPointerMemAccessEmitsQword)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr p(const_cast<char*>("@p"));
    m_pWriter->WriteASMXtoEAX(static_cast<DWORD>(ParamMode::Mem), &p,
                              nullptr, /*type*/ 1002, 0);
    auto bytes = BytesSince(before);
    ASSERT_GE(bytes.size(), 10u);
    EXPECT_EQ(bytes[0], 0x48); // mov rax, [moffs64]
    EXPECT_EQ(bytes[1], 0xA1);

    const auto before2 = m_pWriter->GetCurrentMCPosition();
    m_pWriter->WriteASMEAXtoX(static_cast<DWORD>(ParamMode::Mem), &p,
                              nullptr, /*type*/ 1002, 0);
    bytes = BytesSince(before2);
    ASSERT_GE(bytes.size(), 10u);
    EXPECT_EQ(bytes[0], 0x48); // mov [moffs64], rax
    EXPECT_EQ(bytes[1], 0xA3);
}

TEST_F(X64ArrayApiTest, FullWidthPointerEbpAccessEmitsQword)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr p(const_cast<char*>("@:24"));
    m_pWriter->WriteASMXtoEAX(static_cast<DWORD>(ParamMode::Ebp), &p,
                              nullptr, /*type*/ 1002, 0);
    auto bytes = BytesSince(before);
    ASSERT_GE(bytes.size(), 7u);
    EXPECT_EQ(bytes[0], 0x48); // mov rax, [rbp+disp32]
    EXPECT_EQ(bytes[1], 0x8B);
    EXPECT_EQ(bytes[2], 0x85);

    const auto before2 = m_pWriter->GetCurrentMCPosition();
    m_pWriter->WriteASMEAXtoX(static_cast<DWORD>(ParamMode::Ebp), &p,
                              nullptr, /*type*/ 1002, 0);
    bytes = BytesSince(before2);
    ASSERT_GE(bytes.size(), 7u);
    EXPECT_EQ(bytes[0], 0x48); // mov [rbp+disp32], rax
    EXPECT_EQ(bytes[1], 0x89);
    EXPECT_EQ(bytes[2], 0x85);
}

TEST_F(X64ArrayApiTest, FullWidthPointerPushLoadsQwordThenPushes)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr p(const_cast<char*>("@p"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        1u, static_cast<DWORD>(ASMTask::Push), &p, /*type*/ 1002));
    const auto bytes = BytesSince(before);

    ASSERT_GE(bytes.size(), 11u);
    EXPECT_EQ(bytes[0], 0x48); // mov rax, [moffs64] — full-width slot load
    EXPECT_EQ(bytes[1], 0xA1);
    EXPECT_EQ(bytes[10], 0x50); // push rax (8-byte stack slot)
}

// ---------------------------------------------------------------------------
// Compiler-level: DIM / UNDIM keep the array pointer full-width end to end
// ---------------------------------------------------------------------------

TEST_F(X64ArrayApiTest, CompiledDimStoresFullRaxIntoArraySlot)
{
    char prog[] = "dim a(10)\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();

    // DimDDD (11 params): SUB RSP,0x58 ... CALL RBX ... ADD RSP,0x58, then
    // the return must land in the slot as a full QWORD (48 A3), never A3.
    EXPECT_TRUE(Contains(stream, {0xFF, 0xD3, 0x48, 0x83, 0xC4, 0x58, 0x48,
                                  0xA3}));
    EXPECT_FALSE(Contains(stream, {0xFF, 0xD3, 0x48, 0x83, 0xC4, 0x58, 0xA3}));
}

TEST_F(X64ArrayApiTest, CompiledUnDimLoadsAndClearsFullPointer)
{
    char prog[] = "dim a(10)\r\nundim a()\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();

    // The slot is read back and cleared at full width: every array-pointer
    // load is 48 A1 and every pointer store is 48 A3 (dim return + undim
    // clear = two QWORD stores; the old 32-bit A3 stores are gone).
    EXPECT_TRUE(Contains(stream, {0x48, 0xA1}));
    EXPECT_EQ(Count(stream, {0x48, 0xA3}), 2u);
}
