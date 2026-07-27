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

// Declare compiler global pointers defined in dbp_compiler_lib
extern CStructTable*     g_pStructTable;
extern CStatementList*   g_pStatementList;
extern ICodeGenerator*   g_pASMWriter;
extern CError*           g_pErrorReport;

// Characterization suite for the CASMWriter emission hotspots: each test
// drives a machine-code emission path that owns heap CStr formatting
// temporaries, pinning the "emits without crashing and reports success"
// contract before the RAII conversion.
class ASMWriterEmissionTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;
    CASMWriter*      m_pWriter;

    void SetUp() override {
        DBPLogger::Initialize("test_asmwriter_emission.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();
        g_pStructTable->SetStructDefaults();

        // Initialize the backend so the machine block exists (same
        // bootstrap as ASTCodeGenTest)
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

// Double immediate load + push walks the pDWORD1/2Str and pTemp1/2Str
// temporaries in WriteASMXtoEAX (IMM to ST08) and WriteASMEAXtoX (STACK).
TEST_F(ASMWriterEmissionTest, EmitsDoubleImmediateLoadAndPush) {
    CStr value("2.5");
    m_pWriter->WriteASMXtoEAX(PMODE_IMM, &value, NULL, 8, 0);
    m_pWriter->WriteASMEAXtoX(PMODE_STACK, NULL, NULL, 8, 0);
}

// MEMOFF round-trip for a double-word pair walks the pOffset1/2Str
// temporaries in both directions.
TEST_F(ASMWriterEmissionTest, EmitsMemoryOffsetRoundTrip) {
    CStr var("@myvar");
    m_pWriter->WriteASMXtoEAX(PMODE_MEMOFF, &var, NULL, 9, 8);
    m_pWriter->WriteASMEAXtoX(PMODE_MEMOFF, &var, NULL, 9, 8);
}

// EBP and EBP-OFFSET addressing walks the pDoubleStr/pOffset1/2Str
// temporaries used for local variable access.
TEST_F(ASMWriterEmissionTest, EmitsLocalVariableRoundTrip) {
    CStr local("@:12");
    m_pWriter->WriteASMXtoEAX(PMODE_EBP, &local, NULL, 9, 0);
    m_pWriter->WriteASMEAXtoX(PMODE_EBP, &local, NULL, 9, 0);
    m_pWriter->WriteASMXtoEAX(PMODE_EBPOFF, &local, NULL, 9, 4);
    m_pWriter->WriteASMEAXtoX(PMODE_EBPOFF, &local, NULL, 9, 4);
}

// Array element access walks WriteASMARRtoEAX/WriteASMEAXtoARR with their
// pOffset1/2Str temporaries (and the array-check leap markers when active).
TEST_F(ASMWriterEmissionTest, EmitsArrayElementRoundTrip) {
    CStr arr("@myarr");
    m_pWriter->WriteASMXtoEAX(PMODE_MEMARR, &arr, NULL, 2, 0);
    m_pWriter->WriteASMEAXtoX(PMODE_MEMARR, &arr, NULL, 2, 0);
}

// The runtime error hook task owns the pLineStr temporary.
TEST_F(ASMWriterEmissionTest, EmitsRuntimeErrorHookTask) {
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP2(42, ASMTASK_RUNTIMEERRORHOOK, NULL, 0, NULL, 0));
}

// The debug statement hook pushes four numeric strings to the stack
// (pProgStr/pLineStr/pStartStr/pEndStr temporaries).
TEST_F(ASMWriterEmissionTest, EmitsDebugStatementHookTask) {
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP2(7, ASMTASK_DEBUGSTATEMENTHOOK, NULL, 0, NULL, 0));
}

// The CALL task cuts "dll,command" with GetLeft/RightOfPosition (new[]
// buffers) and owns the pTokenCommandStr temporary.
TEST_F(ASMWriterEmissionTest, EmitsDllCallTask) {
    CStr call("@mycore.dll,@myfunc");
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP2(1, ASMTASK_CALL, &call, 0, NULL, 0));
}

// Completing a leap marker replaces the ref-label with a fresh new[]
// buffer built from the pTempStr temporary.
TEST_F(ASMWriterEmissionTest, CompletesLeapMarker) {
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerJumpNotEqual(1));
    ASSERT_TRUE(m_pWriter->WriteASMLeapMarkerEnd(1));
}

// Registering a DLL command walks AddCommandToTable's pRawCommandString
// temporary and returns the allocated command index.
TEST_F(ASMWriterEmissionTest, RegistersDllCommandInTable) {
    EXPECT_GT(m_pWriter->AddCommandToTable((LPSTR)"@mycore.dll", (LPSTR)"@myfunc"), 0u);
}
