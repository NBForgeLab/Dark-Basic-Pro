#include <gtest/gtest.h>
#include <cstring>
#include <windows.h>
#include "DBPLogger.h"
#include "MachineCodeBuffer.h"
#include "CompilerContext.h"
#include "Statement.h"
#include "StatementList.h"
#include "StructTable.h"
#include "Error.h"
#include "CompilerContext.h"

// Declare compiler global pointers defined in dbp_compiler_lib
extern CStructTable*     g_pStructTable;
extern CStatementList*   g_pStatementList;
extern ICodeGenerator*   g_pASMWriter;
extern CError*           g_pErrorReport;

// ---------------------------------------------------------------------------
// Fixture: tests the CMachineCodeBuffer in isolation (direct API).
// ---------------------------------------------------------------------------
class MachineCodeBufferTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_machine_code_buffer.log");
        m_pContext = new CompilerContext();
        m_pContext->Initialize();
        g_pStructTable->SetStructDefaults();
    }

    void TearDown() override {
        if (m_pContext) {
            m_pContext->Cleanup();
            delete m_pContext;
            m_pContext = nullptr;
        }
        spdlog::shutdown();
    }
};

// After Initialize(), the buffer should be allocated, filled with RET (0xC3),
// and the current position should be zero.
TEST_F(MachineCodeBufferTest, InitializeSetsUpBufferWithRetFill) {
    CMachineCodeBuffer buf;
    ASSERT_TRUE(buf.Initialize(1024));

    EXPECT_EQ(buf.GetMCBlockSize(), 1024u);
    EXPECT_NE(buf.GetProgramStart(), nullptr);
    EXPECT_NE(buf.GetMachineBlock(), nullptr);
    EXPECT_EQ(buf.GetCurrentMCPosition(), 0u);

    // Buffer should be filled with RET (0xC3)
    LPSTR pStart = buf.GetProgramStart();
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(static_cast<unsigned char>(pStart[i]), 0xC3)
            << "Byte " << i << " should be 0xC3 (RET)";
    }
}

// GetCurrentMCPosition tracks how far m_pMachineBlock has advanced.
TEST_F(MachineCodeBufferTest, PositionTrackingAfterByteWrites) {
    CMachineCodeBuffer buf;
    ASSERT_TRUE(buf.Initialize(1024));

    EXPECT_EQ(buf.GetCurrentMCPosition(), 0u);

    // Write a single byte
    buf.WriteByte(0x90); // NOP
    EXPECT_EQ(buf.GetCurrentMCPosition(), 1u);

    // Write another byte
    buf.WriteByte(0xCC); // INT3
    EXPECT_EQ(buf.GetCurrentMCPosition(), 2u);
}

// GetBytePosOfLastInstruction returns the same as GetCurrentMCPosition
// (position of the next byte to be written = byte position after last instruction).
TEST_F(MachineCodeBufferTest, GetBytePosOfLastInstructionMatchesPosition) {
    CMachineCodeBuffer buf;
    ASSERT_TRUE(buf.Initialize(1024));

    EXPECT_EQ(buf.GetBytePosOfLastInstruction(), 0u);

    buf.WriteByte(0x55); // PUSH EBP
    EXPECT_EQ(buf.GetBytePosOfLastInstruction(), 1u);

    buf.WriteDWORD(0xDEADBEEF, 4);
    EXPECT_EQ(buf.GetBytePosOfLastInstruction(), 5u);
}

// WriteDWORD writes a DWORD and advances the pointer by the specified size.
TEST_F(MachineCodeBufferTest, WriteDWORDAdvancesBySize) {
    CMachineCodeBuffer buf;
    ASSERT_TRUE(buf.Initialize(1024));

    buf.WriteDWORD(0x12345678, 4);
    EXPECT_EQ(buf.GetCurrentMCPosition(), 4u);

    // Verify the bytes
    LPSTR p = buf.GetProgramStart();
    EXPECT_EQ(static_cast<unsigned char>(p[0]), 0x78);
    EXPECT_EQ(static_cast<unsigned char>(p[1]), 0x56);
    EXPECT_EQ(static_cast<unsigned char>(p[2]), 0x34);
    EXPECT_EQ(static_cast<unsigned char>(p[3]), 0x12);
}

// WriteDWORD with size=1 advances by 1 byte (byte-sized immediate).
TEST_F(MachineCodeBufferTest, WriteDWORDSize1AdvancesOneByte) {
    CMachineCodeBuffer buf;
    ASSERT_TRUE(buf.Initialize(1024));

    buf.WriteDWORD(0x42, 1);
    EXPECT_EQ(buf.GetCurrentMCPosition(), 1u);
}

// WriteDWORD with size=2 advances by 2 bytes (word-sized immediate).
TEST_F(MachineCodeBufferTest, WriteDWORDSize2AdvancesTwoBytes) {
    CMachineCodeBuffer buf;
    ASSERT_TRUE(buf.Initialize(1024));

    buf.WriteDWORD(0x1234, 2);
    EXPECT_EQ(buf.GetCurrentMCPosition(), 2u);
}

// CheckAndExpandMCBMemory returns false when buffer has plenty of room.
TEST_F(MachineCodeBufferTest, CheckAndExpandReturnsFalseWhenSpaceAvailable) {
    CMachineCodeBuffer buf;
    ASSERT_TRUE(buf.Initialize(1024));

    // Write a few bytes - well within the 1024 buffer
    buf.WriteByte(0x90);
    buf.WriteByte(0x90);

    EXPECT_FALSE(buf.CheckAndExpandMCBMemory());
}

// CheckAndExpandMCBMemory returns true and expands when near the end.
TEST_F(MachineCodeBufferTest, CheckAndExpandReturnsTrueWhenNearEnd) {
    CMachineCodeBuffer buf;
    // Use a small initial buffer so we can fill it quickly
    ASSERT_TRUE(buf.Initialize(128));

    // Fill up to within 100 bytes of end (the expansion threshold)
    for (int i = 0; i < 30; i++) {
        buf.WriteByte(0x90);
    }

    // Now we're at position 30, buffer is 128, barrier is at 28 (128-100).
    // We're past the barrier, so expansion should trigger.
    EXPECT_TRUE(buf.CheckAndExpandMCBMemory());

    // After expansion, size should have grown by 102400
    EXPECT_EQ(buf.GetMCBlockSize(), 128u + 102400u);

    // Position should be preserved
    EXPECT_EQ(buf.GetCurrentMCPosition(), 30u);

    // Pointers should still be valid
    EXPECT_NE(buf.GetProgramStart(), nullptr);
    EXPECT_NE(buf.GetMachineBlock(), nullptr);
}

// After expansion, previously written data is preserved.
TEST_F(MachineCodeBufferTest, DataPreservedAfterExpansion) {
    CMachineCodeBuffer buf;
    ASSERT_TRUE(buf.Initialize(128));

    // Write a known pattern
    buf.WriteByte(0xAA);
    buf.WriteByte(0xBB);
    buf.WriteByte(0xCC);

    // Force expansion
    for (int i = 0; i < 30; i++) {
        buf.WriteByte(0x90);
    }
    buf.CheckAndExpandMCBMemory();

    // Verify original data is intact
    LPSTR pStart = buf.GetProgramStart();
    EXPECT_EQ(static_cast<unsigned char>(pStart[0]), 0xAA);
    EXPECT_EQ(static_cast<unsigned char>(pStart[1]), 0xBB);
    EXPECT_EQ(static_cast<unsigned char>(pStart[2]), 0xCC);
}

// FreeMachineBlock clears the buffer and resets all pointers.
TEST_F(MachineCodeBufferTest, FreeMachineBlockResetsState) {
    CMachineCodeBuffer buf;
    ASSERT_TRUE(buf.Initialize(1024));

    buf.WriteByte(0x90);
    buf.WriteByte(0xCC);

    buf.FreeMachineBlock();

    EXPECT_EQ(buf.GetProgramStart(), nullptr);
    EXPECT_EQ(buf.GetMachineBlock(), nullptr);
    EXPECT_EQ(buf.GetMCBlockSize(), 0u);
    EXPECT_EQ(buf.GetCurrentMCPosition(), 0u);
}

// GetMachineBlockForWrite returns the current write pointer.
TEST_F(MachineCodeBufferTest, GetMachineBlockForWriteReturnsCurrentPosition) {
    CMachineCodeBuffer buf;
    ASSERT_TRUE(buf.Initialize(1024));

    LPSTR pExpected = buf.GetProgramStart();
    EXPECT_EQ(buf.GetMachineBlockForWrite(), pExpected);

    buf.WriteByte(0x90);
    EXPECT_EQ(buf.GetMachineBlockForWrite(), pExpected + 1);
}

// WritePointer writes a 64-bit/32-bit pointer-width value.
TEST_F(MachineCodeBufferTest, WritePointerEmitsFullPointer) {
    CMachineCodeBuffer buf;
    ASSERT_TRUE(buf.Initialize(1024));

    uintptr_t ptrValue = static_cast<uintptr_t>(0x1234567887654321ULL);
    buf.WritePointer(ptrValue, sizeof(uintptr_t));
    EXPECT_EQ(buf.GetCurrentMCPosition(), sizeof(uintptr_t));
}

// Initialize can be called again after FreeMachineBlock (re-init cycle).
TEST_F(MachineCodeBufferTest, ReinitializeAfterFree) {
    CMachineCodeBuffer buf;
    ASSERT_TRUE(buf.Initialize(1024));
    buf.WriteByte(0x90);

    buf.FreeMachineBlock();
    EXPECT_EQ(buf.GetProgramStart(), nullptr);

    ASSERT_TRUE(buf.Initialize(2048));
    EXPECT_EQ(buf.GetMCBlockSize(), 2048u);
    EXPECT_EQ(buf.GetCurrentMCPosition(), 0u);
}

// ---------------------------------------------------------------------------
// Integration fixture: tests CMachineCodeBuffer through CASMWriter to verify
// the extraction preserves existing behavior.
// ---------------------------------------------------------------------------
class MachineCodeBufferIntegrationTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_machine_code_buffer.log");
        m_pContext = new CompilerContext();
        m_pContext->Initialize();
        g_pStructTable->SetStructDefaults();

        char dummyProg[] = "";
        g_pStatementList->MakeStatements(dummyProg, 0);
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
};

// GetCurrentMCPosition returns 0 right after CreateASMHeader.
TEST_F(MachineCodeBufferIntegrationTest, InitialPositionIsZero) {
    EXPECT_EQ(g_pASMWriter->GetCurrentMCPosition(), 0u);
}

// GetBytePosOfLastInstruction returns 0 right after header creation.
TEST_F(MachineCodeBufferIntegrationTest, InitialBytePosIsZero) {
    EXPECT_EQ(g_pASMWriter->GetBytePosOfLastInstruction(), 0u);
}

// Writing an ASM line advances the position.
TEST_F(MachineCodeBufferIntegrationTest, WriteASMLineAdvancesPosition) {
    DWORD posBefore = g_pASMWriter->GetCurrentMCPosition();
    g_pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::POPEAX), "");
    DWORD posAfter = g_pASMWriter->GetCurrentMCPosition();
    EXPECT_GT(posAfter, posBefore);
}

// Multiple writes accumulate position correctly.
TEST_F(MachineCodeBufferIntegrationTest, MultipleWritesAccumulatePosition) {
    g_pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEBP), "");
    DWORD pos1 = g_pASMWriter->GetCurrentMCPosition();
    EXPECT_GT(pos1, 0u);

    g_pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::POPEBP), "");
    DWORD pos2 = g_pASMWriter->GetCurrentMCPosition();
    EXPECT_GT(pos2, pos1);
}
