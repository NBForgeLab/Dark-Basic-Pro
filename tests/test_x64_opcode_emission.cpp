// test_x64_opcode_emission.cpp
//
// Wave 2 x64 emission-layer tests (TDD): the opcode table, the data slot
// widths, the [RBX] indirect expansion, PUSHAD/POPAD expansion, and the
// runtime reference patching must all produce x64-native instruction streams.
//
// Design: docs/superpowers/specs/2026-08-11-x64-opcode-emission-design.md

#include <gtest/gtest.h>
#include <algorithm>
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
#include "Error.h"

#include "CompilerContext.h"
#include "DebuggerInterface.h"

// Declare compiler global pointers defined in dbp_compiler_lib
extern CStructTable*     g_pStructTable;
extern CStatementList*   g_pStatementList;
extern ICodeGenerator*   g_pASMWriter;
extern CError*           g_pErrorReport;

namespace
{
std::vector<uint8_t> AsBytes(const char* raw, std::size_t length)
{
    const auto* bytes = reinterpret_cast<const uint8_t*>(raw);
    return std::vector<uint8_t>(bytes, bytes + length);
}
} // namespace

// ---------------------------------------------------------------------------
// Emission-layer tests
// ---------------------------------------------------------------------------

class X64OpcodeEmissionTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_opcode_emission.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();
        g_pStructTable->SetStructDefaults();

        char dummyProg[] = "";
        g_pStatementList->MakeStatements(dummyProg, 0);
        ASSERT_TRUE(g_pASMWriter->CreateASMHeader());
        m_pWriter = static_cast<CASMWriter*>(g_pASMWriter);
    }

    void TearDown() override {
        if (m_pContext) {
            m_pContext->Cleanup();
            delete m_pContext;
            m_pContext = nullptr;
        }
        spdlog::shutdown();
    }

    // Emits one instruction through the production path and returns exactly
    // the MCB bytes written for it.
    std::vector<uint8_t> EmitLine(DWORD dwOp, const char* data) {
        const auto before = m_pWriter->GetCurrentMCPosition();
        if (!m_pWriter->WriteASMLine(dwOp, const_cast<char*>(data))) return {};
        const auto after = m_pWriter->GetCurrentMCPosition();
        const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
        return AsBytes(raw + before, after - before);
    }

    std::vector<uint8_t> EmitLine2(DWORD dwOp, const char* data1, const char* data2) {
        const auto before = m_pWriter->GetCurrentMCPosition();
        if (!m_pWriter->WriteASMLine2(dwOp, const_cast<char*>(data1), const_cast<char*>(data2))) return {};
        const auto after = m_pWriter->GetCurrentMCPosition();
        const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
        return AsBytes(raw + before, after - before);
    }

    std::vector<uint8_t> EmitLine2IMM(DWORD dwOp, const char* data1, const char* data2, DWORD size) {
        const auto before = m_pWriter->GetCurrentMCPosition();
        if (!m_pWriter->WriteASMLine2IMM(dwOp, const_cast<char*>(data1), const_cast<char*>(data2), size)) return {};
        const auto after = m_pWriter->GetCurrentMCPosition();
        const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
        return AsBytes(raw + before, after - before);
    }

    std::size_t RefCount() const {
        return m_pWriter->GetReferenceTracker().GetRecords().size();
    }

    const CReferenceTracker::Record& LastRef() const {
        const auto& records = m_pWriter->GetReferenceTracker().GetRecords();
        EXPECT_FALSE(records.empty());
        return records.back();
    }
};

// --- Descriptor table ------------------------------------------------------

TEST_F(X64OpcodeEmissionTest, DescriptorTableClassifiesAllDefinedOpcodes) {
    for (DWORD code = 1; code < CASMWriter::GetASMOpcodeCount(); ++code) {
        const auto& def = m_pWriter->GetASMOpcodeDef(code);
        if (def.name == nullptr || def.name[0] == '\0') {
            continue; // gap in the enum numbering; not a defined opcode
        }
        if (strcmp(def.name, "???") == 0) {
            continue; // UNKNOWN sentinel has no encoding by design
        }
        ASSERT_NE(def.op1, -1) << "opcode " << code << " (" << def.name << ") must have an opcode byte";
        ASSERT_GE(static_cast<int>(def.data1), 0) << "opcode " << code << " must declare data1";
        ASSERT_GE(static_cast<int>(def.data2), 0) << "opcode " << code << " must declare data2";
    }
}

TEST_F(X64OpcodeEmissionTest, DescriptorPreservesLegacyBytesForUnchangedForms) {
    struct Expectation { DWORD code; int preOp; int op1; int op2; };
    const Expectation expectations[] = {
        { static_cast<DWORD>(ASMOp::CMPEAX4),     -1, 0x3D, -1 },
        { static_cast<DWORD>(ASMOp::ADDESP),      -1, 0x81, 0xC4 },
        { static_cast<DWORD>(ASMOp::SUBESP),      -1, 0x81, 0xEC },
        { static_cast<DWORD>(ASMOp::MOVEAXECXOFF4), -1, 0x8B, 0x81 },
        { static_cast<DWORD>(ASMOp::MOVEAXEBP4),  -1, 0x8B, 0x85 },
        { static_cast<DWORD>(ASMOp::SETE),        0x0F, 0x94, 0xC0 },
        { static_cast<DWORD>(ASMOp::CDQ),         -1, 0x99, -1 },
        { static_cast<DWORD>(ASMOp::CALLEBX),     -1, 0xFF, 0xD3 },
        { static_cast<DWORD>(ASMOp::MOVEAXSIB4),  0x8B, 0x04, 0x98 },
        { static_cast<DWORD>(ASMOp::RET),         -1, 0xC3, -1 },
    };
    for (const auto& e : expectations) {
        const auto& def = m_pWriter->GetASMOpcodeDef(e.code);
        EXPECT_EQ(def.preOp, e.preOp) << def.name;
        EXPECT_EQ(def.op1, e.op1) << def.name;
        EXPECT_EQ(def.op2, e.op2) << def.name;
    }
}

// --- moffs absolute forms (A0-A3): 8-byte address slot ----------------------

TEST_F(X64OpcodeEmissionTest, MoffsLoadEmits8ByteAddressSlot) {
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "@score");
    // A1 <imm64>: MOV EAX, [abs64]
    EXPECT_EQ(bytes.size(), 9u);
    EXPECT_EQ(bytes[0], 0xA1);
    EXPECT_TRUE(std::all_of(bytes.begin() + 1, bytes.end(), [](uint8_t b) { return b == 0; }))
        << "8-byte placeholder expected";
    EXPECT_EQ(LastRef().machineCodeOffset, m_pWriter->GetCurrentMCPosition() - 8u);
    EXPECT_EQ(LastRef().label, "@score");
}

TEST_F(X64OpcodeEmissionTest, MoffsStoreEmits8ByteAddressSlot) {
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVMEMEAX4), "@score");
    // A3 <imm64>: MOV [abs64], EAX
    EXPECT_EQ(bytes.size(), 9u);
    EXPECT_EQ(bytes[0], 0xA3);
    EXPECT_TRUE(std::all_of(bytes.begin() + 1, bytes.end(), [](uint8_t b) { return b == 0; }));
}

// --- MOV r, imm: 4-byte values, 48 B8+rd imm64 for addresses -----------------

TEST_F(X64OpcodeEmissionTest, MovRegImmValueKeepsImm32Slot) {
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "42");
    // B8 <imm32 placeholder>: MOV EAX, imm32 (zero-extending, value)
    EXPECT_EQ(bytes.size(), 5u);
    EXPECT_EQ(bytes[0], 0xB8);
    EXPECT_TRUE(std::all_of(bytes.begin() + 1, bytes.end(), [](uint8_t b) { return b == 0xFF; }));
}

TEST_F(X64OpcodeEmissionTest, MovRegImmAddressEmitsRexWImm64) {
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVEBXIMM4), "@func");
    // 48 BB <imm64>: MOV RBX, imm64 (address must not truncate)
    EXPECT_EQ(bytes.size(), 10u);
    EXPECT_EQ(bytes[0], 0x48); // REX.W
    EXPECT_EQ(bytes[1], 0xBB); // MOV RBX, imm64
    EXPECT_TRUE(std::all_of(bytes.begin() + 2, bytes.end(), [](uint8_t b) { return b == 0; }));
    EXPECT_EQ(LastRef().machineCodeOffset, m_pWriter->GetCurrentMCPosition() - 8u);

    const auto eax = EmitLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "@var");
    EXPECT_EQ(eax.size(), 10u);
    EXPECT_EQ(eax[0], 0x48);
    EXPECT_EQ(eax[1], 0xB8); // MOV RAX, imm64

    const auto ecx = EmitLine(static_cast<DWORD>(ASMOp::MOVECXIMM4), "@x");
    EXPECT_EQ(ecx[1], 0xB9); // MOV RCX, imm64

    const auto edx = EmitLine(static_cast<DWORD>(ASMOp::MOVEDXIMM4), "@y");
    EXPECT_EQ(edx[1], 0xBA); // MOV RDX, imm64
}

// --- [disp32] absolute forms: 48 BB imm64 + [RBX] expansion ------------------

TEST_F(X64OpcodeEmissionTest, PtrIndirectLoadExpandsThroughRbx) {
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVEBXMEM4), "@arr");
    // 48 BB <imm64> 8B 1B: MOV RBX, addr; MOV EBX, [RBX]
    EXPECT_EQ(bytes.size(), 12u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0xBB);
    EXPECT_TRUE(std::all_of(bytes.begin() + 2, bytes.begin() + 10, [](uint8_t b) { return b == 0; }));
    EXPECT_EQ(bytes[10], 0x8B);
    EXPECT_EQ(bytes[11], 0x1B);
    EXPECT_EQ(LastRef().machineCodeOffset, m_pWriter->GetCurrentMCPosition() - 10u);
}

TEST_F(X64OpcodeEmissionTest, PtrIndirectStoreExpandsThroughRbx) {
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::MOVMEMEBX4), "@arr");
    // 48 BB <imm64> 89 1B: MOV RBX, addr; MOV [RBX], EBX
    EXPECT_EQ(bytes.size(), 12u);
    EXPECT_EQ(bytes[10], 0x89);
    EXPECT_EQ(bytes[11], 0x1B);
}

TEST_F(X64OpcodeEmissionTest, PtrIndirectImmediateStoreExpandsThroughRbx) {
    // 48 BB <imm64> (10 bytes) + C7 03 <imm32> (6 bytes) = 16 bytes total.
    // WriteASMLine2: both operands are reference labels; data2 is a value slot.
    const auto viaRef = EmitLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ERR_", "118");
    EXPECT_EQ(viaRef.size(), 16u);
    EXPECT_EQ(viaRef[10], 0xC7);
    EXPECT_EQ(viaRef[11], 0x03);
    EXPECT_TRUE(std::all_of(viaRef.begin() + 12, viaRef.end(), [](uint8_t b) { return b == 0xFF; }));

    // WriteASMLine2IMM: data2 is written as an immediate value.
    const auto viaImm = EmitLine2IMM(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ERR_", "118", 2);
    EXPECT_EQ(viaImm.size(), 16u);
    EXPECT_EQ(viaImm[10], 0xC7);
    EXPECT_EQ(viaImm[11], 0x03);
    EXPECT_EQ(viaImm[12], 118);
    EXPECT_EQ(viaImm[13], 0);
    EXPECT_EQ(viaImm[14], 0);
    EXPECT_EQ(viaImm[15], 0);
}

TEST_F(X64OpcodeEmissionTest, PtrIndirectEspFormsExpandThroughRbx) {
    const auto store = EmitLine(static_cast<DWORD>(ASMOp::MOVMEMESP4), "@sp");
    EXPECT_EQ(store[10], 0x89);
    EXPECT_EQ(store[11], 0x23); // MOV [RBX], ESP

    const auto load = EmitLine(static_cast<DWORD>(ASMOp::MOVESPMEM4), "@sp");
    EXPECT_EQ(load[10], 0x8B);
    EXPECT_EQ(load[11], 0x23); // MOV ESP, [RBX]
}

TEST_F(X64OpcodeEmissionTest, PtrIndirectIncDecExpandThroughRbx) {
    const auto inc = EmitLine(static_cast<DWORD>(ASMOp::INCMEM4), "@n");
    EXPECT_EQ(inc[10], 0xFF);
    EXPECT_EQ(inc[11], 0x03); // INC dword [RBX]

    const auto dec = EmitLine(static_cast<DWORD>(ASMOp::DECMEM1), "@n");
    EXPECT_EQ(dec[10], 0xFE);
    EXPECT_EQ(dec[11], 0x0B); // DEC byte [RBX]
}

TEST_F(X64OpcodeEmissionTest, PtrIndirectSse2MovsdFormsExpandThroughRbx) {
    // Wave 8: the legacy x87 FLD/FSTP slots are now SSE2 MOVSD XMM0 in place.
    const auto store = EmitLine(static_cast<DWORD>(ASMOp::MOVSDMEMXMM0), "@d");
    EXPECT_EQ(store[10], 0xF2);
    EXPECT_EQ(store[11], 0x0F);
    EXPECT_EQ(store[12], 0x11); // MOVSD qword [RBX], XMM0
    EXPECT_EQ(store[13], 0x03);

    const auto load = EmitLine(static_cast<DWORD>(ASMOp::MOVSDXMM0MEM), "@d");
    EXPECT_EQ(load[10], 0xF2);
    EXPECT_EQ(load[11], 0x0F);
    EXPECT_EQ(load[12], 0x10); // MOVSD XMM0, qword [RBX]
    EXPECT_EQ(load[13], 0x03);
}

TEST_F(X64OpcodeEmissionTest, PtrIndirectJmpExpandsThroughRbx) {
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::JMPREL), "@target");
    EXPECT_EQ(bytes[10], 0xFF);
    EXPECT_EQ(bytes[11], 0x23); // JMP qword [RBX]
}

// --- WriteASMLine2IMM(op, NULL, imm, size): immediate fills the data1 slot ----

TEST_F(X64OpcodeEmissionTest, ImmPathWritesImmediateIntoMovRegImmSlot) {
    // TaskEmitter loads integer constants via WriteASMLine2IMM(op, NULL, value, size).
    // The instruction must NOT be left truncated (B8 alone would consume the
    // next instruction's bytes as the missing imm32).
    const auto before = m_pWriter->GetCurrentMCPosition();
    ASSERT_TRUE(m_pWriter->WriteASMLine2IMM(
        static_cast<DWORD>(ASMOp::MOVEAXIMM4), nullptr, const_cast<char*>("42"), 2));
    const auto after = m_pWriter->GetCurrentMCPosition();
    const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
    const auto bytes = AsBytes(raw + before, after - before);

    EXPECT_EQ(bytes, (std::vector<uint8_t>{ 0xB8, 42, 0, 0, 0 })); // MOV EAX, imm32
}

TEST_F(X64OpcodeEmissionTest, ImmPathWritesByteImmediateIntoMovRegImmSlot) {
    const auto before = m_pWriter->GetCurrentMCPosition();
    ASSERT_TRUE(m_pWriter->WriteASMLine2IMM(
        static_cast<DWORD>(ASMOp::MOVEAXIMM1), nullptr, const_cast<char*>("7"), 0));
    const auto after = m_pWriter->GetCurrentMCPosition();
    const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
    const auto bytes = AsBytes(raw + before, after - before);

    EXPECT_EQ(bytes, (std::vector<uint8_t>{ 0xB0, 7 })); // MOV AL, imm8
}

// --- PUSHAD / POPAD ----------------------------------------------------------

TEST_F(X64OpcodeEmissionTest, PushAdExpandsToExplicitRegisterPushes) {
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::PUSHAD), "");
    // PUSH RAX,RBX,RCX,RDX,RSI,RDI,RBP  (no REX needed in 64-bit mode)
    const std::vector<uint8_t> expected = { 0x50, 0x53, 0x51, 0x52, 0x56, 0x57, 0x55 };
    EXPECT_EQ(bytes, expected);
}

TEST_F(X64OpcodeEmissionTest, PopAdExpandsToExplicitRegisterPops) {
    const auto bytes = EmitLine(static_cast<DWORD>(ASMOp::POPAD), "");
    // POP RBP,RDI,RSI,RDX,RCX,RBX,RAX (reverse order)
    const std::vector<uint8_t> expected = { 0x5D, 0x5F, 0x5E, 0x5A, 0x59, 0x5B, 0x58 };
    EXPECT_EQ(bytes, expected);
}

// --- Unchanged forms keep their legacy byte encoding -------------------------

TEST_F(X64OpcodeEmissionTest, UnchangedRegisterRelativeFormsKeepLegacyBytes) {
    const auto cmp = EmitLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
    EXPECT_EQ(cmp.size(), 5u);
    EXPECT_EQ(cmp[0], 0x3D); // CMP EAX, imm32

    const auto addEsp = EmitLine(static_cast<DWORD>(ASMOp::ADDESP), "8");
    // Wave 4: ADD ESP carries REX.W so the full 64-bit RSP is adjusted
    // (ADD RSP,imm32); the imm32 slot is patched at link time.
    EXPECT_EQ(addEsp.size(), 7u);
    EXPECT_EQ(addEsp[0], 0x48); // REX.W
    EXPECT_EQ(addEsp[1], 0x81);
    EXPECT_EQ(addEsp[2], 0xC4); // ADD RSP, imm32

    const auto rel = EmitLine(static_cast<DWORD>(ASMOp::MOVEAXECXOFF4), "4");
    EXPECT_EQ(rel.size(), 6u);
    EXPECT_EQ(rel[0], 0x8B);
    EXPECT_EQ(rel[1], 0x81); // MOV EAX, [RCX+disp32]

    const auto setE = EmitLine(static_cast<DWORD>(ASMOp::SETE), "");
    EXPECT_EQ(setE, (std::vector<uint8_t>{ 0x0F, 0x94, 0xC0 }));

    const auto cdq = EmitLine(static_cast<DWORD>(ASMOp::CDQ), "");
    EXPECT_EQ(cdq, (std::vector<uint8_t>{ 0x99 }));

    const auto callEbx = EmitLine(static_cast<DWORD>(ASMOp::CALLEBX), "");
    EXPECT_EQ(callEbx, (std::vector<uint8_t>{ 0xFF, 0xD3 })); // CALL RBX
}

// --- Reference records carry the true slot positions -------------------------

TEST_F(X64OpcodeEmissionTest, ReferenceRecordsCarrySlotPositions) {
    // Instruction lengths: moffs 9, MOV r,imm64 10, PtrIndirect 12, MOV r,imm32 5.
    const auto start = m_pWriter->GetCurrentMCPosition();

    EmitLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "@a");       // 9 bytes; slot at +1, 8 bytes
    EXPECT_EQ(LastRef().machineCodeOffset, start + 1u);

    EmitLine(static_cast<DWORD>(ASMOp::MOVEBXIMM4), "@b");       // 10 bytes; slot at +2, 8 bytes
    EXPECT_EQ(LastRef().machineCodeOffset, start + 9u + 2u);

    EmitLine(static_cast<DWORD>(ASMOp::MOVEBXMEM4), "@c");       // 12 bytes; slot at +2, 8 bytes
    EXPECT_EQ(LastRef().machineCodeOffset, start + 9u + 10u + 2u);

    EmitLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "7");        // 5 bytes; slot at +1, 4 bytes
    EXPECT_EQ(LastRef().machineCodeOffset, start + 9u + 10u + 12u + 1u);
}

// ---------------------------------------------------------------------------
// Runtime patching tests
// ---------------------------------------------------------------------------

class X64OpcodeEmissionRuntimeTest : public ::testing::Test {
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

TEST_F(X64OpcodeEmissionRuntimeTest, PatchAddressKindWritesPointerWidth) {
    m_code.assign(16, 0);
    m_values = { 0x1122334455667788ULL };
    m_positions = { 2u };
    m_types = { static_cast<DWORD>(ReferenceKind::Variable) }; // 3
    Patch();
    uintptr_t written = 0;
    std::memcpy(&written, m_code.data() + 2, sizeof(written));
    EXPECT_EQ(written, 0x1122334455667788ULL);
}

TEST_F(X64OpcodeEmissionRuntimeTest, PatchImmediateKindWritesFourBytes) {
    m_code.assign(16, 0);
    m_values = { 0xDEADBEEFULL };
    m_positions = { 4u };
    m_types = { static_cast<DWORD>(ReferenceKind::Immediate) }; // 4
    Patch();
    uint32_t written = 0;
    std::memcpy(&written, m_code.data() + 4, sizeof(written));
    EXPECT_EQ(written, 0xDEADBEEFu);
}

TEST_F(X64OpcodeEmissionRuntimeTest, PatchCodeLabelWritesRel32FromPosPlus4) {
    m_code.assign(16, 0);
    m_values = { 0x1000ULL };
    m_positions = { 4u };
    m_types = { static_cast<DWORD>(ReferenceKind::CodeLabel) }; // 5
    Patch();
    int32_t written = 0;
    std::memcpy(&written, m_code.data() + 4, sizeof(written));
    EXPECT_EQ(written, static_cast<int32_t>(0x1000 - (4 + 4)));
}

TEST_F(X64OpcodeEmissionRuntimeTest, PatchDataLabelWritesPointerWidth) {
    m_code.assign(16, 0);
    m_values = { 0x80000000ULL };
    m_positions = { 8u };
    m_types = { static_cast<DWORD>(ReferenceKind::DataLabel) }; // 6
    Patch();
    uintptr_t written = 0;
    std::memcpy(&written, m_code.data() + 8, sizeof(written));
    EXPECT_EQ(written, 0x80000000ULL);
}
