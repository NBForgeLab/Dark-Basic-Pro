#include <gtest/gtest.h>
#include <cstring>
#include <vector>
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
// temporaries in WriteASMXtoRAX (IMM to XMM0) and WriteASMRAXtoX (STACK).
TEST_F(ASMWriterEmissionTest, EmitsDoubleImmediateLoadAndPush) {
    CStr value("2.5");
    m_pWriter->WriteASMXtoRAX(static_cast<DWORD>(ParamMode::Imm), &value, NULL, 8, 0);
    m_pWriter->WriteASMRAXtoX(static_cast<DWORD>(ParamMode::Stack), NULL, NULL, 8, 0);
}

// MEMOFF round-trip for a double-word pair walks the pOffset1/2Str
// temporaries in both directions.
TEST_F(ASMWriterEmissionTest, EmitsMemoryOffsetRoundTrip) {
    CStr var("@myvar");
    m_pWriter->WriteASMXtoRAX(static_cast<DWORD>(ParamMode::MemOff), &var, NULL, 9, 8);
    m_pWriter->WriteASMRAXtoX(static_cast<DWORD>(ParamMode::MemOff), &var, NULL, 9, 8);
}

// RBP and RBP-OFFSET addressing walks the pDoubleStr/pOffset1/2Str
// temporaries used for local variable access.
TEST_F(ASMWriterEmissionTest, EmitsLocalVariableRoundTrip) {
    CStr local("@:12");
    m_pWriter->WriteASMXtoRAX(static_cast<DWORD>(ParamMode::Rbp), &local, NULL, 9, 0);
    m_pWriter->WriteASMRAXtoX(static_cast<DWORD>(ParamMode::Rbp), &local, NULL, 9, 0);
    m_pWriter->WriteASMXtoRAX(static_cast<DWORD>(ParamMode::RbpOff), &local, NULL, 9, 4);
    m_pWriter->WriteASMRAXtoX(static_cast<DWORD>(ParamMode::RbpOff), &local, NULL, 9, 4);
}

// Array element access walks WriteASMARRtoRAX/WriteASMRAXtoARR with their
// pOffset1/2Str temporaries (and the array-check leap markers when active).
TEST_F(ASMWriterEmissionTest, EmitsArrayElementRoundTrip) {
    CStr arr("@myarr");
    m_pWriter->WriteASMXtoRAX(static_cast<DWORD>(ParamMode::MemArr), &arr, NULL, 2, 0);
    m_pWriter->WriteASMRAXtoX(static_cast<DWORD>(ParamMode::MemArr), &arr, NULL, 2, 0);
}

// The runtime error hook task owns the pLineStr temporary.
TEST_F(ASMWriterEmissionTest, EmitsRuntimeErrorHookTask) {
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP2(42, static_cast<DWORD>(ASMTask::RuntimeErrorHook), NULL, 0, NULL, 0));
}

TEST_F(ASMWriterEmissionTest, EmitsMultidimensionalArrayOffsetCalculation) {
    CStr offset("@arrayOffset");
    CStr array("@&grid");
    const auto before = m_pWriter->GetCurrentMCPosition();

    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u,
        static_cast<DWORD>(ASMTask::CalcArrayOffset),
        &offset,
        nullptr,
        7u,
        2u,
        &array,
        nullptr,
        101u,
        0u));
    EXPECT_GT(m_pWriter->GetCurrentMCPosition(), before);
}

TEST_F(ASMWriterEmissionTest, CompilerGeneratedLineZeroEmitsPrologueTasks) {
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP1(
        0u, static_cast<DWORD>(ASMTask::PushRegisters), nullptr, 0u));
    ASSERT_GT(m_pWriter->GetCurrentMCPosition(), 0u);
    // x64 callee-saved prologue: PUSH RBX (0x53), PUSH RBP (0x55),
    // PUSH RDI (0x57), PUSH RSI (0x56), PUSH R12-R15 (0x41 0x54..0x57).
    const auto* code = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
    EXPECT_EQ(static_cast<unsigned char>(code[0]), 0x53u);
    EXPECT_EQ(static_cast<unsigned char>(code[1]), 0x55u);
    EXPECT_EQ(static_cast<unsigned char>(code[2]), 0x57u);
    EXPECT_EQ(static_cast<unsigned char>(code[3]), 0x56u);
    EXPECT_EQ(static_cast<unsigned char>(code[4]), 0x41u);
    EXPECT_EQ(static_cast<unsigned char>(code[5]), 0x54u);
}

// The debug statement hook pushes four numeric strings to the stack
// (pProgStr/pLineStr/pStartStr/pEndStr temporaries).
TEST_F(ASMWriterEmissionTest, EmitsDebugStatementHookTask) {
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP2(7, static_cast<DWORD>(ASMTask::DebugStatementHook), NULL, 0, NULL, 0));
}

// The CALL task cuts "dll,command" with GetLeft/RightOfPosition (new[]
// buffers) and owns the pTokenCommandStr temporary.
TEST_F(ASMWriterEmissionTest, EmitsDllCallTask) {
    CStr call("@mycore.dll,@myfunc");
    ASSERT_TRUE(m_pWriter->WriteASMTaskCoreP2(1, static_cast<DWORD>(ASMTask::Call), &call, 0, NULL, 0));
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

// A 32-bit ADD/SUB against RAX zero-extends into the full register, so the
// accumulator forms (05 = ADD EAX,imm32 / 2D = SUB EAX,imm32) silently destroy
// bits 32-63 of any pointer held in RAX. Every site that adjusts a pointer must
// emit the REX.W forms instead: 48 81 C0 = ADD RAX,imm32, 48 81 E8 = SUB
// RAX,imm32. Both keep the 4-byte immediate slot, so the reference-fixup width
// recorded in the executable block is unchanged; only the instruction grows.
namespace {

// Returns the offset of the first occurrence of pNeedle in the window, or -1.
ptrdiff_t FindBytes(const unsigned char* pWindow, size_t nWindow,
                    const std::vector<unsigned char>& needle) {
    if (needle.empty() || nWindow < needle.size()) return -1;
    for (size_t i = 0; i + needle.size() <= nWindow; i++) {
        if (memcmp(pWindow + i, needle.data(), needle.size()) == 0)
            return static_cast<ptrdiff_t>(i);
    }
    return -1;
}

} // namespace

// WriteASMARRtoRAX with a user-defined array element type (1101) has already
// formed the 64-bit element address in RAX via ADD RAX,RBX; the field offset
// that follows must not truncate it.
TEST_F(ASMWriterEmissionTest, UdtArrayElementFieldOffsetUses64BitAddImmediate) {
    CStr arr("@&udtarr");
    CStr index("3");
    const auto before = m_pWriter->GetCurrentMCPosition();

    m_pWriter->WriteASMARRtoRAX(static_cast<DWORD>(ParamMode::MemArr), &arr, &index,
                                static_cast<DWORD>(DBPType::UserDefinedArrayPtr), 0u);

    const auto after = m_pWriter->GetCurrentMCPosition();
    ASSERT_GT(after, before);
    const auto* code = reinterpret_cast<const unsigned char*>(
        m_pWriter->GetMachineCodeBuffer().GetProgramStart()) + before;

    // Anchor on POP RDX; ADD RAX,RBX - the direct-layout element address.
    const ptrdiff_t anchor = FindBytes(code, after - before, {0x5A, 0x48, 0x01, 0xD8});
    ASSERT_GE(anchor, 0) << "element address sequence not emitted";
    EXPECT_EQ(code[anchor + 4], 0x48u);
    EXPECT_EQ(code[anchor + 5], 0x81u);
    EXPECT_EQ(code[anchor + 6], 0xC0u);
}

// PushAddress of a local (RBP-relative) operand: MOV RAX,RBP then the frame
// offset. The frame offset arithmetic runs on a pointer.
TEST_F(ASMWriterEmissionTest, PushAddressLocalOffsetUses64BitAddImmediate) {
    CStr local("@:12");
    const auto before = m_pWriter->GetCurrentMCPosition();

    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::PushAddress),
        &local, nullptr, static_cast<DWORD>(DBPType::UserDefinedPtr), 0u,
        nullptr, nullptr, 0u, 0u));

    const auto after = m_pWriter->GetCurrentMCPosition();
    ASSERT_GT(after, before);
    const auto* code = reinterpret_cast<const unsigned char*>(
        m_pWriter->GetMachineCodeBuffer().GetProgramStart()) + before;

    // MOV RAX,RBP (48 89 E8) | ADD RAX,imm32 (48 81 C0 + 4) | PUSH RAX (50)
    EXPECT_EQ(after - before, 11u);
    EXPECT_EQ(code[0], 0x48u);
    EXPECT_EQ(code[1], 0x89u);
    EXPECT_EQ(code[2], 0xE8u);
    EXPECT_EQ(code[3], 0x48u);
    EXPECT_EQ(code[4], 0x81u);
    EXPECT_EQ(code[5], 0xC0u);
    EXPECT_EQ(code[10], 0x50u);
}

// PushAddress of a global operand carrying an additional member offset:
// MOVABS RAX,imm64 then ADD RAX,imm32.
TEST_F(ASMWriterEmissionTest, PushAddressMemberOffsetUses64BitAddImmediate) {
    CStr udtPtr("@myudtptr");
    const auto before = m_pWriter->GetCurrentMCPosition();

    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::PushAddress),
        &udtPtr, nullptr, static_cast<DWORD>(DBPType::UserDefinedPtr), 8u,
        nullptr, nullptr, 0u, 0u));

    const auto after = m_pWriter->GetCurrentMCPosition();
    ASSERT_GT(after, before);
    const auto* code = reinterpret_cast<const unsigned char*>(
        m_pWriter->GetMachineCodeBuffer().GetProgramStart()) + before;

    // MOVABS RAX,imm64 (48 B8 + 8) | ADD RAX,imm32 (48 81 C0 + 4) | PUSH RAX
    EXPECT_EQ(after - before, 18u);
    EXPECT_EQ(code[0], 0x48u);
    EXPECT_EQ(code[1], 0xB8u);
    EXPECT_EQ(code[10], 0x48u);
    EXPECT_EQ(code[11], 0x81u);
    EXPECT_EQ(code[12], 0xC0u);
    EXPECT_EQ(code[17], 0x50u);
}

// PushUdt of a local (RBP-relative) UDT: the frame offset and the advance to
// the end of the UDT data both operate on a pointer.
TEST_F(ASMWriterEmissionTest, PushUdtLocalOffsetsUse64BitAddImmediate) {
    CStr local("@:12");
    CStr udtSize("16");
    const auto before = m_pWriter->GetCurrentMCPosition();

    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::PushUdt),
        &local, nullptr, static_cast<DWORD>(DBPType::UserDefinedPtr), 0u,
        &udtSize, nullptr, 0u, 0u));

    const auto after = m_pWriter->GetCurrentMCPosition();
    ASSERT_GT(after, before);
    const auto* code = reinterpret_cast<const unsigned char*>(
        m_pWriter->GetMachineCodeBuffer().GetProgramStart()) + before;

    // MOV RAX,RBP (3) | ADD RAX,imm32 (7) | ADD RAX,imm32 (7)
    EXPECT_EQ(after - before, 17u);
    EXPECT_EQ(code[0], 0x48u);
    EXPECT_EQ(code[1], 0x89u);
    EXPECT_EQ(code[2], 0xE8u);
    EXPECT_EQ(code[3], 0x48u);
    EXPECT_EQ(code[4], 0x81u);
    EXPECT_EQ(code[5], 0xC0u);
    EXPECT_EQ(code[10], 0x48u);
    EXPECT_EQ(code[11], 0x81u);
    EXPECT_EQ(code[12], 0xC0u);
}

// PushUdt of a global UDT carrying an additional member offset: both the member
// offset and the advance to the end of the UDT data adjust a pointer.
TEST_F(ASMWriterEmissionTest, PushUdtMemberOffsetsUse64BitAddImmediate) {
    CStr udtPtr("@myudtptr");
    CStr udtSize("16");
    const auto before = m_pWriter->GetCurrentMCPosition();

    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::PushUdt),
        &udtPtr, nullptr, static_cast<DWORD>(DBPType::UserDefinedPtr), 8u,
        &udtSize, nullptr, 0u, 0u));

    const auto after = m_pWriter->GetCurrentMCPosition();
    ASSERT_GT(after, before);
    const auto* code = reinterpret_cast<const unsigned char*>(
        m_pWriter->GetMachineCodeBuffer().GetProgramStart()) + before;

    // MOV EAX,imm32 (5) | ADD RAX,imm32 (7) | ADD RAX,imm32 (7)
    EXPECT_EQ(after - before, 19u);
    EXPECT_EQ(code[0], 0xB8u);
    EXPECT_EQ(code[5], 0x48u);
    EXPECT_EQ(code[6], 0x81u);
    EXPECT_EQ(code[7], 0xC0u);
    EXPECT_EQ(code[12], 0x48u);
    EXPECT_EQ(code[13], 0x81u);
    EXPECT_EQ(code[14], 0xC0u);
}

// PushUdt walks the UDT's stack slots backwards from the end pointer, so the
// per-slot decrement is pointer arithmetic too.
TEST_F(ASMWriterEmissionTest, PushUdtSlotWalkUses64BitSubImmediate) {
    CStr udtPtr("@myudtptr");
    CStr udtSize("16");
    const auto before = m_pWriter->GetCurrentMCPosition();

    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::PushUdt),
        &udtPtr, nullptr, static_cast<DWORD>(DBPType::UserDefinedPtr), 0u,
        &udtSize, nullptr, 0u, 1u));

    const auto after = m_pWriter->GetCurrentMCPosition();
    ASSERT_GT(after, before);
    const auto* code = reinterpret_cast<const unsigned char*>(
        m_pWriter->GetMachineCodeBuffer().GetProgramStart()) + before;

    // MOV EAX,imm32 (5) | ADD RAX,imm32 (7) | SUB RAX,imm32 (7) | PUSH [RAX] (2)
    EXPECT_EQ(after - before, 21u);
    EXPECT_EQ(code[12], 0x48u);
    EXPECT_EQ(code[13], 0x81u);
    EXPECT_EQ(code[14], 0xE8u);
    EXPECT_EQ(code[19], 0xFFu);
    EXPECT_EQ(code[20], 0x30u);
}

// HideAnyHiddenCode replaces printable characters between HIDESTART and
// HIDEEND markers with 'X', leaving the markers themselves and any code
// outside the hidden region untouched.
TEST_F(ASMWriterEmissionTest, HideAnyHiddenCodeReplacesBetweenMarkers) {
    // "ABCHIDESTARTsecretHIDEENDXYZ"
    //   markers stay, "secret" becomes "XXXXXX"
    char data[] = "ABCHIDESTARTsecretHIDEENDXYZ";
    DWORD len = (DWORD)strlen(data);
    ASSERT_TRUE(CDebuggerInterface::HideAnyHiddenCode(data, len));
    EXPECT_STREQ(data, "ABCHIDESTARTXXXXXXHIDEENDXYZ");
}

// Without any markers the data is left untouched.
TEST_F(ASMWriterEmissionTest, HideAnyHiddenCodeNoMarkersLeavesDataIntact) {
    char data[] = "normal code here";
    DWORD len = (DWORD)strlen(data);
    ASSERT_TRUE(CDebuggerInterface::HideAnyHiddenCode(data, len));
    EXPECT_STREQ(data, "normal code here");
}

// Case-insensitive marker detection: "hidestart" / "hideend" also work.
TEST_F(ASMWriterEmissionTest, HideAnyHiddenCodeCaseInsensitiveMarkers) {
    char data[] = "beforehidestartSECREThideendafter";
    DWORD len = (DWORD)strlen(data);
    ASSERT_TRUE(CDebuggerInterface::HideAnyHiddenCode(data, len));
    EXPECT_STREQ(data, "beforehidestartXXXXXXhideendafter");
}

TEST_F(ASMWriterEmissionTest, EmitsShlImm8ExactlyThreeBytes) {
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr val("20");
    ASSERT_TRUE(m_pWriter->WriteASMLine2IMM(static_cast<DWORD>(ASMOp::SHLRAX4), nullptr, val.GetStr(), 0u));
    const auto after = m_pWriter->GetCurrentMCPosition();
    EXPECT_EQ(after - before, 3u);
    const auto* code = reinterpret_cast<const unsigned char*>(
        m_pWriter->GetMachineCodeBuffer().GetProgramStart()) + before;
    EXPECT_EQ(code[0], 0xC1u);
    EXPECT_EQ(code[1], 0xE0u);
    EXPECT_EQ(code[2], 0x14u); // 20 decimal = 0x14
}

