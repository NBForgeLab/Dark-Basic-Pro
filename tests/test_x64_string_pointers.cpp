// test_x64_string_pointers.cpp
//
// Wave 5 x64 string-pointer tests (TDD): a string variable's value is a heap
// pointer, so every load/store that moves a string value must be full-width
// (REX.W) on x64 — moffs (global vars), RBP-relative (function params),
// ECX-relative (UDT members), register-relative (derefs) — while dword/byte/
// int64/string-array paths keep their exact current byte streams. Also pins
// the varspace slot widths (string = 8 bytes), the x64 mangled names the
// compiler resolves for the runtime string manager, and full-pointer patching
// of string-literal slots.
//
// Design: docs/superpowers/specs/2026-08-11-x64-string-pointers-design.md

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <windows.h>

#include "DBPLogger.h"
#include "ASMWriter.h"
#include "EXEBlock.h"
#include "ReferenceTracker.h"
#include "Statement.h"
#include "StatementList.h"
#include "StructTable.h"
#include "VarTable.h"
#include "InstructionTable.h"
#include "Error.h"

#include "CompilerContext.h"
#include "DebuggerInterface.h"

// Declare compiler global pointers defined in dbp_compiler_lib
extern CStructTable*     g_pStructTable;
extern CStatementList*   g_pStatementList;
extern ICodeGenerator*   g_pASMWriter;
extern CError*           g_pErrorReport;
extern CInstructionTable* g_pInstructionTable;

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

class X64StringPointerTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_string_pointers.log");

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

    // Bytes written by the last operation (between the recorded positions).
    std::vector<uint8_t> BytesSince(std::size_t before) const {
        const auto after = m_pWriter->GetCurrentMCPosition();
        const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
        return AsBytes(raw + before, after - before);
    }

    // Loads a value into EAX/RAX through the production task path.
    std::vector<uint8_t> EmitXtoEAX(DWORD mode, const char* value,
                                    DWORD type, DWORD offset = 0) {
        const auto before = m_pWriter->GetCurrentMCPosition();
        CStr val(const_cast<char*>(value));
        m_pWriter->WriteASMXtoEAX(mode, &val, nullptr, type, offset);
        return BytesSince(before);
    }

    // Stores EAX/RAX through the production task path.
    std::vector<uint8_t> EmitEAXtoX(DWORD mode, const char* value,
                                    DWORD type, DWORD offset = 0) {
        const auto before = m_pWriter->GetCurrentMCPosition();
        CStr val(const_cast<char*>(value));
        m_pWriter->WriteASMEAXtoX(mode, &val, nullptr, type, offset);
        return BytesSince(before);
    }

    static bool StartsWith(const std::vector<uint8_t>& bytes,
                           const std::vector<uint8_t>& prefix) {
        if (bytes.size() < prefix.size()) return false;
        return std::equal(prefix.begin(), prefix.end(), bytes.begin());
    }
};

// ---------------------------------------------------------------------------
// String loads/stores must be full-width (REX.W) on x64
// ---------------------------------------------------------------------------

TEST_F(X64StringPointerTest, GlobalStringLoadIsMovRaxMoffs64)
{
    // MOV RAX, [moffs64]: 48 A1 <8-byte address slot>
    const auto bytes = EmitXtoEAX(static_cast<DWORD>(ParamMode::Mem), "@name",
                                  /*type*/ 3);
    ASSERT_EQ(bytes.size(), 10u);
    EXPECT_EQ(bytes[0], 0x48); // REX.W
    EXPECT_EQ(bytes[1], 0xA1); // mov rax, moffs64
    // 8-byte moffs slot reserved for the variable-space address patch.
    EXPECT_TRUE(std::all_of(bytes.begin() + 2, bytes.end(),
                            [](uint8_t b) { return b == 0; }));
}

TEST_F(X64StringPointerTest, GlobalStringStoreIsMovMoffs64Rax)
{
    // MOV [moffs64], RAX: 48 A3 <8-byte address slot>
    const auto bytes = EmitEAXtoX(static_cast<DWORD>(ParamMode::Mem), "@name",
                                  /*type*/ 3);
    ASSERT_EQ(bytes.size(), 10u);
    EXPECT_EQ(bytes[0], 0x48); // REX.W
    EXPECT_EQ(bytes[1], 0xA3); // mov [moffs64], rax
    EXPECT_TRUE(std::all_of(bytes.begin() + 2, bytes.end(),
                            [](uint8_t b) { return b == 0; }));
}

TEST_F(X64StringPointerTest, ParamStringLoadIsMovRaxRbpDisp)
{
    // MOV RAX, [RBP+disp32]: 48 8B 85 <disp32>
    const auto bytes = EmitXtoEAX(static_cast<DWORD>(ParamMode::Ebp), "@:4",
                                  /*type*/ 3);
    ASSERT_EQ(bytes.size(), 7u);
    EXPECT_EQ(bytes[0], 0x48); // REX.W
    EXPECT_EQ(bytes[1], 0x8B); // mov r64, r/m64
    EXPECT_EQ(bytes[2], 0x85); // [rbp+disp32]
    // disp32 slot: patched later (0xFFFFFFFF placeholder).
    EXPECT_TRUE(std::all_of(bytes.begin() + 3, bytes.end(),
                            [](uint8_t b) { return b == 0xFF; }));
}

TEST_F(X64StringPointerTest, ParamStringStoreIsMovRbpDispRax)
{
    // MOV [RBP+disp32], RAX: 48 89 85 <disp32>
    const auto bytes = EmitEAXtoX(static_cast<DWORD>(ParamMode::Ebp), "@:4",
                                  /*type*/ 3);
    ASSERT_EQ(bytes.size(), 7u);
    EXPECT_EQ(bytes[0], 0x48); // REX.W
    EXPECT_EQ(bytes[1], 0x89); // mov r/m64, r64
    EXPECT_EQ(bytes[2], 0x85); // [rbp+disp32]
    EXPECT_TRUE(std::all_of(bytes.begin() + 3, bytes.end(),
                            [](uint8_t b) { return b == 0xFF; }));
}

TEST_F(X64StringPointerTest, StringLiteralLoadIsMovRaxImm64)
{
    // String literals resolve by string-table index ($$N) to an address:
    // MOV RAX, imm64 = 48 B8 <8-byte reference slot>.
    const auto bytes = EmitXtoEAX(static_cast<DWORD>(ParamMode::Imm), "$$1",
                                  /*type*/ 3);
    ASSERT_EQ(bytes.size(), 10u);
    EXPECT_EQ(bytes[0], 0x48); // REX.W
    EXPECT_EQ(bytes[1], 0xB8); // mov rax, imm64
    EXPECT_TRUE(std::all_of(bytes.begin() + 2, bytes.end(),
                            [](uint8_t b) { return b == 0; }));
}

TEST_F(X64StringPointerTest, UdtMemberStringLoadIsMovRaxRcxDisp)
{
    // Base into RCX (MOVECXIMM4 → 48 B9 imm64), then MOV RAX, [RCX+disp32]
    // = 48 8B 81 <disp32>.
    const auto bytes = EmitXtoEAX(static_cast<DWORD>(ParamMode::MemOff), "@u",
                                  /*type*/ 3, /*offset*/ 8);
    ASSERT_EQ(bytes.size(), 7u + 10u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0xB9); // mov rcx, imm64
    EXPECT_EQ(bytes[10], 0x48); // REX.W
    EXPECT_EQ(bytes[11], 0x8B);
    EXPECT_EQ(bytes[12], 0x81); // [rcx+disp32]
    EXPECT_TRUE(std::all_of(bytes.begin() + 13, bytes.end(),
                            [](uint8_t b) { return b == 0xFF; }));
}

TEST_F(X64StringPointerTest, UdtMemberStringStoreIsMovRcxDispRax)
{
    // MOV [RCX+disp32], RAX = 48 89 81 <disp32>.
    const auto bytes = EmitEAXtoX(static_cast<DWORD>(ParamMode::MemOff), "@u",
                                  /*type*/ 3, /*offset*/ 8);
    ASSERT_EQ(bytes.size(), 7u + 10u);
    EXPECT_EQ(bytes[10], 0x48); // REX.W
    EXPECT_EQ(bytes[11], 0x89);
    EXPECT_EQ(bytes[12], 0x81); // [rcx+disp32]
    EXPECT_TRUE(std::all_of(bytes.begin() + 13, bytes.end(),
                            [](uint8_t b) { return b == 0xFF; }));
}

TEST_F(X64StringPointerTest, UdtRelStringLoadDerefsRax)
{
    // MOV RAX, [moffs64] (48 A1 + slot) then MOV RAX, [RAX] (48 8B 00).
    const auto bytes = EmitXtoEAX(static_cast<DWORD>(ParamMode::MemRel), "@u",
                                  /*type*/ 3);
    ASSERT_EQ(bytes.size(), 13u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0xA1);
    EXPECT_EQ(bytes[10], 0x48); // REX.W
    EXPECT_EQ(bytes[11], 0x8B);
    EXPECT_EQ(bytes[12], 0x00); // [rax]
}

TEST_F(X64StringPointerTest, UdtRelStringStoreGuardsRcxAndDerefs)
{
    // MOV RCX, RAX (48 8B C8), MOV RAX, [moffs64] (48 A1 + slot),
    // MOV [RCX], RAX (48 8B 08).
    const auto bytes = EmitEAXtoX(static_cast<DWORD>(ParamMode::MemRel), "@u",
                                  /*type*/ 3);
    ASSERT_EQ(bytes.size(), 16u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x8B);
    EXPECT_EQ(bytes[2], 0xC8); // mov rcx, rax (value guard, full width)
    EXPECT_EQ(bytes[3], 0x48);
    EXPECT_EQ(bytes[4], 0xA1);
    EXPECT_EQ(bytes[13], 0x48); // REX.W
    EXPECT_EQ(bytes[14], 0x8B);
    EXPECT_EQ(bytes[15], 0x08); // [rcx]
}

TEST_F(X64StringPointerTest, StringStackPushStillPushesFullRax)
{
    // PUSHEAX = 0x50 is push rax in 64-bit mode (full-width, 8-byte slot).
    const auto bytes = EmitEAXtoX(static_cast<DWORD>(ParamMode::Stack), nullptr,
                                  /*type*/ 3);
    ASSERT_EQ(bytes.size(), 1u);
    EXPECT_EQ(bytes[0], 0x50);
}

// ---------------------------------------------------------------------------
// Non-regression: other types keep their exact byte streams
// ---------------------------------------------------------------------------

TEST_F(X64StringPointerTest, DwordLoadsStay32Bit)
{
    // Global dword load: MOV EAX, [moffs64] — A1 with NO REX.W (32-bit move).
    const auto moffs = EmitXtoEAX(static_cast<DWORD>(ParamMode::Mem), "@x",
                                  /*type*/ 7);
    ASSERT_EQ(moffs.size(), 9u);
    EXPECT_EQ(moffs[0], 0xA1);

    // Param dword load: 8B 85 <disp32> — no REX.W.
    const auto ebp = EmitXtoEAX(static_cast<DWORD>(ParamMode::Ebp), "@:4",
                                /*type*/ 7);
    ASSERT_EQ(ebp.size(), 6u);
    EXPECT_EQ(ebp[0], 0x8B);
    EXPECT_EQ(ebp[1], 0x85);
}

TEST_F(X64StringPointerTest, ByteLoadStaysByteWidth)
{
    // MOV AL, [moffs64] — A0 with NO REX.W (8-bit move).
    const auto bytes = EmitXtoEAX(static_cast<DWORD>(ParamMode::Mem), "@b",
                                  /*type*/ 5);
    ASSERT_EQ(bytes.size(), 9u);
    EXPECT_EQ(bytes[0], 0xA0);
}

TEST_F(X64StringPointerTest, Int64LoadIsSingle8ByteMove)
{
    // Wave 8b: type 9 (int64) is a single 8-byte slot — MOV RAX,[moffs]
    // (48 A1 + moffs64), no two-dword-slot EDX:EAX split.
    const auto bytes = EmitXtoEAX(static_cast<DWORD>(ParamMode::Mem), "@x",
                                  /*type*/ 9);
    ASSERT_EQ(bytes.size(), 10u); // 48 A1 + moffs64
    EXPECT_EQ(bytes[0], 0x48); // REX.W
    EXPECT_EQ(bytes[1], 0xA1); // mov rax, [moffs64]
}

TEST_F(X64StringPointerTest, StringArrayElementAccessUsesX64RefTable)
{
    // Wave 6: the runtime ref table stores full 8-byte element addresses, so
    // the string-array chain is fully QWORD — slot load 48 A1, SIB scale ×8
    // (48 8B 04 D8), 8-byte element load (48 8B 88 / 48 8B C1).
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr arr(const_cast<char*>("@arr"));
    CStr idx(const_cast<char*>("@2"));
    m_pWriter->WriteASMXtoEAX(static_cast<DWORD>(ParamMode::MemArr), &arr,
                              &idx, /*type*/ 103, 0);
    const auto bytes = BytesSince(before);

    // Layout: MOVEAXMEM8 (48 A1+8) | MOVEBXMEM4 (48 BB+8 + 8B 1B, index
    // address via RBX) | SIB ×8 | QWORD element load | RAX=RCX.
    ASSERT_GE(bytes.size(), 36u);
    EXPECT_EQ(bytes[0], 0x48); // MOVEAXMEM8 (8-byte address slot)
    EXPECT_EQ(bytes[1], 0xA1);
    EXPECT_EQ(bytes[22], 0x48); // MOVEAXSIB8: mov rax, [rax+rbx*8]
    EXPECT_EQ(bytes[23], 0x8B);
    EXPECT_EQ(bytes[24], 0x04);
    EXPECT_EQ(bytes[25], 0xD8); // scale=3 (×8)
    EXPECT_EQ(bytes[26], 0x48); // MOVECXEAXOFF8: mov rcx, [rax+disp32]
    EXPECT_EQ(bytes[27], 0x8B);
    EXPECT_EQ(bytes[28], 0x88);
    EXPECT_EQ(bytes[33], 0x48); // MOVEAXECX8: mov rax, rcx
    EXPECT_EQ(bytes[34], 0x8B);
    EXPECT_EQ(bytes[35], 0xC1);
    EXPECT_FALSE(Contains(bytes, {0x8B, 0x04, 0x98})); // no 32-bit SIB ×4
}

// ---------------------------------------------------------------------------
// varspace slot widths
// ---------------------------------------------------------------------------

TEST_F(X64StringPointerTest, VarSpaceSlotsUseAddressWidthForStrings)
{
    // integer = 4, string = 8, string array = 8 on x64.
    CVarTable vInt, vStr, vStrArr;
    // The default ctor leaves the scope null; EstablishVarOffsets reads it,
    // so give each an empty (GLOBAL) scope.
    vInt.SetVarScope(new CStr(""));
    vStr.SetVarScope(new CStr(""));
    vStrArr.SetVarScope(new CStr(""));
    vInt.SetVarName(new CStr("i"));
    vInt.SetVarType(new CStr("integer"));
    vInt.SetVarTypeValue(1);
    vStr.SetVarName(new CStr("s$"));
    vStr.SetVarType(new CStr("string"));
    vStr.SetVarTypeValue(3);
    vStrArr.SetVarName(new CStr("a$"));
    vStrArr.SetVarType(new CStr("string array"));
    vStrArr.SetVarTypeValue(103);
    vStrArr.SetArrFlag(1);
    vInt.Add(&vStr);
    vStr.Add(&vStrArr);

    DWORD totalSize = 0;
    ASSERT_EQ(vInt.EstablishVarOffsets(&totalSize), 20u);
    EXPECT_EQ(vInt.GetOffsetValue(), 0u);
    EXPECT_EQ(vStr.GetOffsetValue(), 4u);      // after a 4-byte integer
    EXPECT_EQ(vStrArr.GetOffsetValue(), 12u);  // string slot is 8 bytes
    EXPECT_EQ(totalSize, 20u);
}

// ---------------------------------------------------------------------------
// Runtime string manager contract: x64 mangled names in the command table
// ---------------------------------------------------------------------------

TEST_F(X64StringPointerTest, CommandTableResolvesX64MangledNames)
{
    // The compiler emits the decorated name the runtime core DLL exports;
    // uintptr_t signatures mangle as _K on MSVC x64.
    auto* pAssign = g_pInstructionTable->GetRef(
        static_cast<DWORD>(InternalInstruction::AssignSS));
    ASSERT_NE(pAssign, nullptr);
    ASSERT_NE(pAssign->GetDecoratedName(), nullptr);
    EXPECT_STREQ(pAssign->GetDecoratedName()->GetStr(),
                 "?EquateSS@@YA_K_K_K@Z");

    auto* pFree = g_pInstructionTable->GetRef(
        static_cast<DWORD>(InternalInstruction::StrFree));
    ASSERT_NE(pFree, nullptr);
    ASSERT_NE(pFree->GetDecoratedName(), nullptr);
    EXPECT_STREQ(pFree->GetDecoratedName()->GetStr(),
                 "?FreeSS@@YA_K_K@Z");
}

// ---------------------------------------------------------------------------
// Full-pointer patching of string-literal slots
// ---------------------------------------------------------------------------

class X64StringPointerPatchTest : public ::testing::Test {
protected:
    std::vector<char> m_code;
    std::vector<uintptr_t> m_values;
    std::vector<DWORD> m_positions;
    std::vector<DWORD> m_types;

    void Patch() {
        CEXEBlock::PatchReferenceValues(
            m_values.data(), m_values.size(),
            m_positions.data(), m_types.data(),
            m_code.data());
    }
};

TEST_F(X64StringPointerPatchTest, StringLiteralSlotPatchesFullPointer)
{
    m_code.assign(16, 0);
    // A 64-bit heap address above 4 GiB must survive the patch untouched.
    m_values = { 0x1A2B3C4D5E6F7080ULL };
    m_positions = { 2u };
    m_types = { static_cast<DWORD>(ReferenceKind::StringLiteral) }; // 2
    Patch();
    uintptr_t written = 0;
    std::memcpy(&written, m_code.data() + 2, sizeof(written));
    EXPECT_EQ(written, 0x1A2B3C4D5E6F7080ULL);
}
