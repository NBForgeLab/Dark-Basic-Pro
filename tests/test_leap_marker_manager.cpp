#include <gtest/gtest.h>
#include <cstring>
#include <windows.h>
#include "DBPLogger.h"
#include "ASMWriter.h"
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

// Characterization tests for the leap marker / backpatching subsystem.
// These tests pin the contract for forward-reference resolution in x86
// code emission: marker placement, leap emission, and finalization.
class LeapMarkerManagerTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;
    CASMWriter*      m_pWriter;

    void SetUp() override {
        DBPLogger::Initialize("test_leap_marker_manager.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();
        g_pStructTable->SetStructDefaults();

        // Bootstrap the backend so the machine block exists
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
};

// --- WriteASMLeapMarkerTop ---

// Recording a top marker succeeds and captures the current machine block position.
TEST_F(LeapMarkerManagerTest, LeapMarkerTopRecordsPosition) {
    DWORD posBefore = m_pWriter->GetCurrentMCPosition();
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerTop());
    // Position should not change just by recording the marker
    EXPECT_EQ(m_pWriter->GetCurrentMCPosition(), posBefore);
}

// --- WriteASMLineLeapToTop ---

// After recording a top marker, emitting some code, and leaping back,
// the leap should succeed (returns true) and advance the machine code position.
TEST_F(LeapMarkerManagerTest, LineLeapToTopAfterMarker) {
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerTop());
    // Emit a small instruction to advance the machine block
    m_pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRBP), nullptr);
    // Leap back to top with a JNE opcode
    ASSERT_TRUE(m_pWriter->WriteASMLineLeapToTop(static_cast<DWORD>(ASMOp::JNE)));
    // Position should have advanced
    EXPECT_GT(m_pWriter->GetCurrentMCPosition(), 0u);
}

// --- WriteASMLeapMarkerJumpToTop ---

// JumpToTop combines a compare-with-zero and a leap back to the top marker.
TEST_F(LeapMarkerManagerTest, LeapMarkerJumpToTopEmitsCompareAndLeap) {
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerTop());
    // Emit some code between marker and jump
    m_pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRBP), nullptr);
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerJumpToTop());
    EXPECT_GT(m_pWriter->GetCurrentMCPosition(), 0u);
}

// --- WriteASMLeapMarkerJumpNotEqual ---

// JNE marker at various indices should succeed.
TEST_F(LeapMarkerManagerTest, LeapMarkerJumpNotEqualAtMultipleIndices) {
    for (DWORD di = 0; di < 5; di++) {
        ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerJumpNotEqual(di));
    }
}

// --- WriteASMLeapMarkerEnd ---

// Finalizing a marker that was set should succeed.
TEST_F(LeapMarkerManagerTest, LeapMarkerEndAfterJumpNotEqual) {
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerJumpNotEqual(1));
    // Emit some code between marker and end
    m_pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRBP), nullptr);
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerEnd(1));
}

// Finalizing a marker that was never set (index unused) is a no-op success.
TEST_F(LeapMarkerManagerTest, LeapMarkerEndWithoutPriorMarkerIsNoOp) {
    // No marker was set at index 5, so End should be a no-op
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerEnd(5));
}

// --- WriteASMLeapForwardMarker ---

// Forward marker emits escape check and a JE leap at index 0.
TEST_F(LeapMarkerManagerTest, LeapForwardMarkerSucceeds) {
    ASSERT_TRUE(m_pWriter->WriteASMLeapForwardMarker());
    EXPECT_GT(m_pWriter->GetCurrentMCPosition(), 0u);
}

// Forward marker followed by end at index 0 resolves the forward reference.
TEST_F(LeapMarkerManagerTest, LeapForwardMarkerThenEnd) {
    ASSERT_TRUE(m_pWriter->WriteASMLeapForwardMarker());
    // Emit some code
    m_pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRBP), nullptr);
    // Finalize the forward reference at index 0
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerEnd(0));
}

// --- WriteASMLeapMarkerJump ---

// Jump marker with JE opcode at various indices.
TEST_F(LeapMarkerManagerTest, LeapMarkerJumpWithJE) {
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 1));
    m_pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRBP), nullptr);
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerEnd(1));
}

// --- Multiple markers in sequence ---

// Multiple independent markers can be placed and resolved.
TEST_F(LeapMarkerManagerTest, MultipleMarkersInSequence) {
    // Place markers at indices 1, 2, 3
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerJumpNotEqual(1));
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerJumpNotEqual(2));
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerJumpNotEqual(3));

    // Emit some code
    m_pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRBP), nullptr);

    // Resolve all three
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerEnd(1));
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerEnd(2));
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerEnd(3));
}

// --- WriteASMLineLeap ---

// Direct leap line emission with an opcode.
TEST_F(LeapMarkerManagerTest, LineLeapEmitsInstruction) {
    DWORD posBefore = m_pWriter->GetCurrentMCPosition();
    ASSERT_TRUE(m_pWriter->WriteASMLineLeap(static_cast<DWORD>(ASMOp::JE), 0));
    EXPECT_GT(m_pWriter->GetCurrentMCPosition(), posBefore);
}

// --- Top marker + leap cycle ---

// A full cycle: record top, emit code, leap back, verify position advanced.
TEST_F(LeapMarkerManagerTest, FullTopLeapCycle) {
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerTop());
    DWORD posAfterMarker = m_pWriter->GetCurrentMCPosition();

    // Emit several instructions
    m_pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRBP), nullptr);
    m_pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRBX), nullptr);

    // Leap back to top
    ASSERT_TRUE(m_pWriter->WriteASMLineLeapToTop(static_cast<DWORD>(ASMOp::JNE)));

    // Position must have advanced past the marker
    EXPECT_GT(m_pWriter->GetCurrentMCPosition(), posAfterMarker);
}

// --- Re-entrant marker at same index ---

// Setting a marker at the same index twice and resolving should work.
TEST_F(LeapMarkerManagerTest, ReuseMarkerIndex) {
    // First use of index 2
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerJumpNotEqual(2));
    m_pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRBP), nullptr);
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerEnd(2));

    // Second use of index 2
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerJumpNotEqual(2));
    m_pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRBX), nullptr);
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerEnd(2));
}
