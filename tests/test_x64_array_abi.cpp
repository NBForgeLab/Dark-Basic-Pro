// Wave 6 — Runtime array ABI to 64-bit.
//
// Pins the x64 array contract end to end:
//   * the varspace array-pointer slot is an 8-byte address (REX.W moffs/EBP
//     loads) — a truncated pointer would dereference garbage;
//   * the runtime ref table is indexed with SIB scale ×8 (48 8B 04 D8), since
//     CreateArray stores full 8-byte element addresses;
//   * string-array element values are 8-byte pointers (QWORD loads/stores),
//     while integer-array element values stay 4-byte DWORDs;
//   * the REL-to-string-element path (203) dereferences an 8-byte pointer.
//
// All these assertions fail against the wave-5 emitter (4-byte ref table,
// SIB ×4, DWORD element access).

#include "gtest/gtest.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "CompilerContext.h"
#include "ASMWriter.h" // defines enum class ASMTask
#include "DBPLogger.h"
#include "ParserResultData.h"
#include "StatementList.h"
#include "Str.h"
#include "StructTable.h"
#include "InstructionTable.h"
#include "TargetABI.h"

extern CStructTable* g_pStructTable;
extern CInstructionTable* g_pInstructionTable;
extern CStatementList* g_pStatementList;
extern ICodeGenerator* g_pASMWriter;

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

class X64ArrayAbiTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_array_abi.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();
        g_pStructTable->SetStructDefaults();
        ASSERT_TRUE(g_pInstructionTable->SetInternalInstructionDatabase());

        char dummyProg[] = "";
        g_pStatementList->MakeStatements(dummyProg, 0);
        ASSERT_TRUE(g_pASMWriter->CreateASMHeader());
        m_pWriter = static_cast<CASMWriter*>(g_pASMWriter);
        m_pWriter->SetArrayCheckFlag(false);
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
};

// ---------------------------------------------------------------------------
// String-array element access: the full 8-byte chain
// ---------------------------------------------------------------------------

TEST_F(X64ArrayAbiTest, StringArrayElementReadUsesQwordRefChain)
{
    // arr$[2] read: slot load (48 A1+moffs) | index (48 BB+moffs + 8B 1B) |
    // SIB ×8 (48 8B 04 D8) | element load (48 8B 88+disp) | reg copy (48 8B C1).
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr arr(const_cast<char*>("@arr"));
    CStr idx(const_cast<char*>("@2"));
    m_pWriter->WriteASMXtoEAX(static_cast<DWORD>(ParamMode::MemArr), &arr,
                              &idx, /*type*/ 103, 0);
    const auto bytes = BytesSince(before);

    ASSERT_GE(bytes.size(), 36u);
    // Slot load is a full 8-byte pointer: REX.W + moffs.
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0xA1);
    // Ref table indexed with scale ×8.
    EXPECT_EQ(bytes[22], 0x48);
    EXPECT_EQ(bytes[23], 0x8B);
    EXPECT_EQ(bytes[24], 0x04);
    EXPECT_EQ(bytes[25], 0xD8); // SIB: scale=3 (×8), index=RBX, base=RAX
    // Element value: 8-byte pointer load [RAX+disp32] then RAX=RCX.
    EXPECT_EQ(bytes[26], 0x48);
    EXPECT_EQ(bytes[27], 0x8B);
    EXPECT_EQ(bytes[28], 0x88);
    EXPECT_EQ(bytes[33], 0x48);
    EXPECT_EQ(bytes[34], 0x8B);
    EXPECT_EQ(bytes[35], 0xC1);
    // The old 32-bit ref-table chain is gone (scale ×4 SIB).
    EXPECT_FALSE(Contains(bytes, {0x8B, 0x04, 0x98}));
}

TEST_F(X64ArrayAbiTest, StringArrayElementWriteUsesQwordRefChain)
{
    // arr$[2] = value: value guard (48 8B C8) | slot (48 A1+moffs) |
    // index (48 BB+moffs + 8B 1B) | SIB ×8 | element store (48 89 88+disp).
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr arr(const_cast<char*>("@arr"));
    CStr idx(const_cast<char*>("@2"));
    m_pWriter->WriteASMEAXtoX(static_cast<DWORD>(ParamMode::MemArr), &arr,
                              &idx, /*type*/ 103, 0);
    const auto bytes = BytesSince(before);

    ASSERT_GE(bytes.size(), 36u);
    // The value (a full 64-bit string pointer) must not truncate in RCX.
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x8B);
    EXPECT_EQ(bytes[2], 0xC8); // mov rcx, rax
    // Slot load: full 8-byte pointer.
    EXPECT_EQ(bytes[3], 0x48);
    EXPECT_EQ(bytes[4], 0xA1);
    // Ref table indexed with scale ×8.
    EXPECT_EQ(bytes[25], 0x48);
    EXPECT_EQ(bytes[26], 0x8B);
    EXPECT_EQ(bytes[27], 0x04);
    EXPECT_EQ(bytes[28], 0xD8);
    // Element store: 8-byte pointer through [RAX+disp32].
    EXPECT_EQ(bytes[29], 0x48);
    EXPECT_EQ(bytes[30], 0x89);
    EXPECT_EQ(bytes[31], 0x88);
}

// ---------------------------------------------------------------------------
// Integer arrays: SIB ×8 for the ref table, DWORD element values
// ---------------------------------------------------------------------------

TEST_F(X64ArrayAbiTest, IntegerArrayElementReadKeepsDwordValue)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr arr(const_cast<char*>("@arr"));
    CStr idx(const_cast<char*>("@2"));
    m_pWriter->WriteASMXtoEAX(static_cast<DWORD>(ParamMode::MemArr), &arr,
                              &idx, /*type*/ 101, 0);
    const auto bytes = BytesSince(before);

    ASSERT_GE(bytes.size(), 34u);
    // Ref table still scale ×8 (uniform runtime ABI)...
    EXPECT_EQ(bytes[22], 0x48);
    EXPECT_EQ(bytes[23], 0x8B);
    EXPECT_EQ(bytes[24], 0x04);
    EXPECT_EQ(bytes[25], 0xD8);
    // ...but the element value stays a 32-bit DWORD: no REX.W on the load.
    EXPECT_EQ(bytes[26], 0x8B);
    EXPECT_EQ(bytes[27], 0x88);
    EXPECT_EQ(bytes[32], 0x8B); // mov eax, ecx
    EXPECT_EQ(bytes[33], 0xC1);
}

TEST_F(X64ArrayAbiTest, IntegerArrayElementWriteKeepsDwordValue)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr arr(const_cast<char*>("@arr"));
    CStr idx(const_cast<char*>("@2"));
    m_pWriter->WriteASMEAXtoX(static_cast<DWORD>(ParamMode::MemArr), &arr,
                              &idx, /*type*/ 101, 0);
    const auto bytes = BytesSince(before);

    ASSERT_GE(bytes.size(), 34u);
    EXPECT_EQ(bytes[0], 0x8B); // mov ecx, eax — 32-bit value guard
    EXPECT_EQ(bytes[1], 0xC8);
    EXPECT_EQ(bytes[2], 0x48); // slot: 8-byte pointer
    EXPECT_EQ(bytes[3], 0xA1);
    EXPECT_EQ(bytes[24], 0x48); // SIB ×8
    EXPECT_EQ(bytes[25], 0x8B);
    EXPECT_EQ(bytes[26], 0x04);
    EXPECT_EQ(bytes[27], 0xD8);
    EXPECT_EQ(bytes[28], 0x89); // 32-bit element store
    EXPECT_EQ(bytes[29], 0x88);
}

// ---------------------------------------------------------------------------
// Array pointer slots through EBP (local arrays)
// ---------------------------------------------------------------------------

TEST_F(X64ArrayAbiTest, ArraySlotViaEbpLoadsQwordPointer)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr arr(const_cast<char*>("@:24"));
    CStr idx(const_cast<char*>("@2"));
    m_pWriter->WriteASMXtoEAX(static_cast<DWORD>(ParamMode::EbpArr), &arr,
                              &idx, /*type*/ 103, 0);
    const auto bytes = BytesSince(before);

    ASSERT_GE(bytes.size(), 33u);
    // MOV RAX, [RBP+disp32] — full-width pointer slot.
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x8B);
    EXPECT_EQ(bytes[2], 0x85);
    // SIB ×8 after the index load.
    EXPECT_EQ(bytes[19], 0x48);
    EXPECT_EQ(bytes[20], 0x8B);
    EXPECT_EQ(bytes[21], 0x04);
    EXPECT_EQ(bytes[22], 0xD8);
    // 8-byte element load.
    EXPECT_EQ(bytes[23], 0x48);
    EXPECT_EQ(bytes[24], 0x8B);
    EXPECT_EQ(bytes[25], 0x88);
}

TEST_F(X64ArrayAbiTest, ArrayElementWriteViaEbpUsesQwordGuard)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr arr(const_cast<char*>("@:24"));
    CStr idx(const_cast<char*>("@2"));
    m_pWriter->WriteASMEAXtoX(static_cast<DWORD>(ParamMode::EbpArr), &arr,
                              &idx, /*type*/ 103, 0);
    const auto bytes = BytesSince(before);

    ASSERT_GE(bytes.size(), 33u);
    EXPECT_EQ(bytes[0], 0x48); // mov rcx, rax — full pointer value
    EXPECT_EQ(bytes[1], 0x8B);
    EXPECT_EQ(bytes[2], 0xC8);
    EXPECT_EQ(bytes[3], 0x48); // MOV RAX, [RBP+disp32]
    EXPECT_EQ(bytes[4], 0x8B);
    EXPECT_EQ(bytes[5], 0x85);
    EXPECT_EQ(bytes[22], 0x48); // SIB ×8
    EXPECT_EQ(bytes[23], 0x8B);
    EXPECT_EQ(bytes[24], 0x04);
    EXPECT_EQ(bytes[25], 0xD8);
    EXPECT_EQ(bytes[26], 0x48); // 8-byte element store
    EXPECT_EQ(bytes[27], 0x89);
    EXPECT_EQ(bytes[28], 0x88);
}

// ---------------------------------------------------------------------------
// CalcArrayOffset task: array-pointer slot must load 8 bytes
// ---------------------------------------------------------------------------

TEST_F(X64ArrayAbiTest, CalcArrayOffsetLoadsQwordArrayPointer)
{
    // Dim-1 offset calculation: slot load | POP EBX | MOV EAX,EBX | store.
    CResultData p1; // destination: type-7 array-offset result
    p1.m_dwType = 7;
    p1.m_dwDataOffset = 1; // one subscript
    p1.m_pStringToken = std::make_unique<CStr>(const_cast<char*>("@dest"));
    CResultData p2; // the array: MemArr
    p2.m_dwType = 103;
    p2.m_pStringToken = std::make_unique<CStr>(const_cast<char*>("@arr"));

    const auto before = m_pWriter->GetCurrentMCPosition();
    ASSERT_TRUE(m_pWriter->WriteASMTaskP2(
        1u, static_cast<DWORD>(ASMTask::CalcArrayOffset), &p1, &p2));
    const auto bytes = BytesSince(before);

    ASSERT_GE(bytes.size(), 24u);
    // The array pointer comes from an 8-byte slot: REX.W + moffs.
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0xA1);
    // POP EBX (first subscript) then MOV EAX,EBX — unchanged DWORD index math.
    EXPECT_EQ(bytes[10], 0x5B);
    EXPECT_EQ(bytes[11], 0x8B);
    EXPECT_EQ(bytes[12], 0xC3);
    // The linear index is stored to the temp destination via the ImmOrAddr
    // path: a full 8-byte address load (48 B9 + imm64) then a DWORD store.
    EXPECT_EQ(bytes[13], 0x48); // mov rcx, imm64 — no address truncation
    EXPECT_EQ(bytes[14], 0xB9);
    EXPECT_EQ(bytes[23], 0x89); // mov [ecx+disp], eax
    EXPECT_EQ(bytes[24], 0x81);
}

// ---------------------------------------------------------------------------
// REL to a string element (203): full 8-byte pointer dereference
// ---------------------------------------------------------------------------

TEST_F(X64ArrayAbiTest, StringRelElementUsesQwordPointerDeref)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr rel(const_cast<char*>("@rel"));
    m_pWriter->WriteASMXtoEAX(static_cast<DWORD>(ParamMode::MemRel), &rel,
                              nullptr, /*type*/ 203, 0);
    const auto bytes = BytesSince(before);

    ASSERT_GE(bytes.size(), 13u);
    // REL slot is an 8-byte pointer.
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0xA1);
    // [RAX] dereference is an 8-byte pointer load.
    EXPECT_EQ(bytes[10], 0x48);
    EXPECT_EQ(bytes[11], 0x8B);
    EXPECT_EQ(bytes[12], 0x00);
}
