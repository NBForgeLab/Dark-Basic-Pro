// Wave 11 — list instructions (ArrayInsert/ArrayDelete/Queue/Stack) as
// internal build instructions with the widened uintptr_t runtime API.
//
// Red phase (before the fix): the commands were registered under the internal
// placeholder name "+list", so FindInstruction("ARRAY INSERT AT TOP ...")
// returned false and every scalar list statement failed in DoBlock with
// ERR_SYNTAX+43. Green phase: they register under their user-visible names
// with x64 decorated names (?...@@YA_K_K@Z / ?...@@YAX_K@Z), the internal DB
// loads before the DLL .rc surface so the x64 entries win the friend chain,
// and the H param (array-as-input) flows the array pointer full-width (1002).
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

bool Contains(const std::vector<uint8_t>& haystack, const std::vector<uint8_t>& needle)
{
    return std::search(haystack.begin(), haystack.end(),
                       needle.begin(), needle.end()) != haystack.end();
}

std::size_t Count(const std::vector<uint8_t>& haystack, const std::vector<uint8_t>& needle)
{
    std::size_t count = 0;
    for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i)
    {
        if (std::equal(needle.begin(), needle.end(), haystack.begin() + i))
            ++count;
    }
    return count;
}
} // namespace

class X64ListInstructionsTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_list_instructions.log");
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
};

// ---------------------------------------------------------------------------
// Table-level: the internal DB registers the user-visible names with the
// widened x64 decorated names (uintptr_t = _K in MSVC x64 mangling).
// ---------------------------------------------------------------------------

TEST_F(X64ListInstructionsTest, InsertTopRegisteredWithX64Name)
{
    // FindInstruction resolves to the HEAD of the friend chain — the first
    // registered form (H, 1 param) — carrying the 1-param x64 mangling.
    CInstructionTableEntry* pRef = nullptr;
    DWORD dwTokenData = 0, dwParamMax = 0, dwLength = 0;
    ASSERT_TRUE(g_pInstructionTable->FindInstruction(
        true, const_cast<char*>("ARRAY INSERT AT TOP a(0)"), 0,
        &dwTokenData, &dwParamMax, &dwLength, &pRef));
    ASSERT_NE(pRef, nullptr);
    EXPECT_STREQ(pRef->GetName()->GetStr(), "ARRAY INSERT AT TOP");
    EXPECT_STREQ(pRef->GetParamTypes()->GetStr(), "H");
    EXPECT_STREQ(pRef->GetDecoratedName()->GetStr(),
                 "?ArrayInsertAtTop@@YA_K_K@Z");
}

TEST_F(X64ListInstructionsTest, InsertTopTwoParamRegisteredWithX64Name)
{
    // GetRef holds the LAST registration for the internal value — the HL
    // (uintptr_t,int) overload — carrying the 2-param x64 mangling and the
    // star semantics of the .rc %H*L% form (place=1, array-as-input).
    CInstructionTableEntry* pRef = g_pInstructionTable->GetRef(
        static_cast<DWORD>(InternalInstruction::ArrayInsertTop));
    ASSERT_NE(pRef, nullptr);
    EXPECT_STREQ(pRef->GetName()->GetStr(), "ARRAY INSERT AT TOP");
    EXPECT_STREQ(pRef->GetParamTypes()->GetStr(), "HL");
    EXPECT_STREQ(pRef->GetDecoratedName()->GetStr(),
                 "?ArrayInsertAtTop@@YA_K_KH@Z");
    EXPECT_EQ(pRef->GetReturnParam(), 7u);
    EXPECT_EQ(pRef->GetReturnParamPlace(), 1u);
    EXPECT_TRUE(pRef->GetSpecialArrayParam());
}

TEST_F(X64ListInstructionsTest, DeleteElementRegisteredWithX64VoidName)
{
    CInstructionTableEntry* pRef = nullptr;
    DWORD dwTokenData = 0, dwParamMax = 0, dwLength = 0;
    ASSERT_TRUE(g_pInstructionTable->FindInstruction(
        true, const_cast<char*>("ARRAY DELETE ELEMENT a(0)"), 0,
        &dwTokenData, &dwParamMax, &dwLength, &pRef));
    ASSERT_NE(pRef, nullptr);
    EXPECT_STREQ(pRef->GetName()->GetStr(), "ARRAY DELETE ELEMENT");
    EXPECT_STREQ(pRef->GetDecoratedName()->GetStr(),
                 "?ArrayDeleteElement@@YAX_K@Z");
    EXPECT_EQ(pRef->GetReturnParam(), 0u);
}

TEST_F(X64ListInstructionsTest, QueueAndStackRegisteredWithX64Names)
{
    const char* kExpect[][2] = {
        {"ADD TO QUEUE",       "?AddToQueue@@YA_K_K@Z"},
        {"REMOVE FROM QUEUE",  "?RemoveFromQueue@@YAX_K@Z"},
        {"ADD TO STACK",       "?PushToStack@@YA_K_K@Z"},
        {"REMOVE FROM STACK",  "?PopFromStack@@YAX_K@Z"},
    };
    const DWORD kValues[] = {
        static_cast<DWORD>(InternalInstruction::AddToQueue),
        static_cast<DWORD>(InternalInstruction::RemoveFromQueue),
        static_cast<DWORD>(InternalInstruction::PushStack),
        static_cast<DWORD>(InternalInstruction::PopStack),
    };
    for (std::size_t i = 0; i < 4; ++i)
    {
        CInstructionTableEntry* pRef = g_pInstructionTable->GetRef(kValues[i]);
        ASSERT_NE(pRef, nullptr) << "missing entry " << kExpect[i][0];
        EXPECT_STREQ(pRef->GetName()->GetStr(), kExpect[i][0]);
        EXPECT_STREQ(pRef->GetDecoratedName()->GetStr(), kExpect[i][1]);
    }
}

TEST_F(X64ListInstructionsTest, FindInstructionMatchesUserVisibleName)
{
    CInstructionTableEntry* pRef = nullptr;
    DWORD dwTokenData = 0, dwParamMax = 0, dwLength = 0;
    const bool ok = g_pInstructionTable->FindInstruction(
        true, const_cast<char*>("ARRAY INSERT AT TOP a(0), 5"), 0,
        &dwTokenData, &dwParamMax, &dwLength, &pRef);
    ASSERT_TRUE(ok);
    ASSERT_NE(pRef, nullptr);
    EXPECT_EQ(dwLength, 19u); // strlen("ARRAY INSERT AT TOP")
    EXPECT_STREQ(pRef->GetDecoratedName()->GetStr(),
                 "?ArrayInsertAtTop@@YA_K_K@Z");
}

// ---------------------------------------------------------------------------
// Compiler-level: full-width array pointer end to end.
// ---------------------------------------------------------------------------

TEST_F(X64ListInstructionsTest, InsertTopPushesFullPointerAndStoresReturn)
{
    char prog[] = "dim a(10)\r\narray insert at top a(0), 5\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();

    // Full-width slot load (48 A1) + push rax (50): the array pointer is
    // pushed as a QWORD, never a 32-bit A1/DWORD push.
    EXPECT_TRUE(Contains(stream, {0x48, 0xA1, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x50}));

    // x64 call convention: mov rbx,<import> + call rbx.
    EXPECT_TRUE(Contains(stream, {0x48, 0xBB}));
    EXPECT_TRUE(Contains(stream, {0xFF, 0xD3}));

    // The star command writes the returned (possibly reallocated) pointer
    // back into the array slot at full width: 48 A3 (mov [moffs64], rax).
    // dim stores once (DimDDD) + insert stores once = exactly two QWORD
    // pointer stores; the 32-bit A3 form never appears.
    EXPECT_EQ(Count(stream, {0x48, 0xA3}), 2u);
    EXPECT_FALSE(Contains(stream, {0xFF, 0xD3, 0x48, 0x83, 0xC4, 0xA3}));
}

TEST_F(X64ListInstructionsTest, DeleteElementPushesFullPointerNoReturnStore)
{
    char prog[] = "dim a(10)\r\narray delete element a(0)\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();

    // Full-width slot load + push rax for the array pointer.
    EXPECT_TRUE(Contains(stream, {0x48, 0xA1, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x50}));
    EXPECT_TRUE(Contains(stream, {0xFF, 0xD3}));

    // Void command (resultp=0): NO returned-pointer store. Only dim's own
    // DimDDD return lands in the slot, so exactly one 48 A3 in the stream.
    EXPECT_EQ(Count(stream, {0x48, 0xA3}), 1u);
}

TEST_F(X64ListInstructionsTest, AddToStackPushesFullPointer)
{
    char prog[] = "dim a(10)\r\nadd to stack a(0)\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();

    EXPECT_TRUE(Contains(stream, {0x48, 0xA1, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x50}));
    EXPECT_TRUE(Contains(stream, {0xFF, 0xD3}));
    EXPECT_EQ(Count(stream, {0x48, 0xA3}), 2u); // dim + add to stack
}

TEST_F(X64ListInstructionsTest, RemoveFromQueuePushesFullPointerNoReturn)
{
    char prog[] = "dim a(10)\r\nremove from queue a(0)\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();

    EXPECT_TRUE(Contains(stream, {0x48, 0xA1, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x50}));
    EXPECT_TRUE(Contains(stream, {0xFF, 0xD3}));
    EXPECT_EQ(Count(stream, {0x48, 0xA3}), 1u); // dim only
}
