#include <gtest/gtest.h>
#include <cstring>
#include <windows.h>
#include "DebuggerInterface.h"
#include "DebugInfo.h"

// Access the global CDebugInfo instance used by GetDataFromDebugger
extern CDebugInfo g_DebugInfo;

// ---------------------------------------------------------------------------
// Characterization tests for CDebuggerInterface (extracted from CASMWriter).
// These pin the current behaviour of the debugger communication methods
// before and after the extraction refactor.
// ---------------------------------------------------------------------------

class DebuggerInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure the debugger state is in a known baseline
        CDebuggerInterface::InitDebuggerState();
    }

    void TearDown() override {
    }
};

// -- HideAnyHiddenCode -------------------------------------------------------

// Replaces printable characters between HIDESTART and HIDEEND markers with 'X',
// leaving the markers themselves and any code outside the hidden region untouched.
TEST_F(DebuggerInterfaceTest, HideAnyHiddenCodeReplacesBetweenMarkers) {
    char data[] = "ABCHIDESTARTsecretHIDEENDXYZ";
    DWORD len = (DWORD)strlen(data);
    ASSERT_TRUE(CDebuggerInterface::HideAnyHiddenCode(data, len));
    EXPECT_STREQ(data, "ABCHIDESTARTXXXXXXHIDEENDXYZ");
}

// Without any markers the data is left untouched.
TEST_F(DebuggerInterfaceTest, HideAnyHiddenCodeNoMarkersLeavesDataIntact) {
    char data[] = "normal code here";
    DWORD len = (DWORD)strlen(data);
    ASSERT_TRUE(CDebuggerInterface::HideAnyHiddenCode(data, len));
    EXPECT_STREQ(data, "normal code here");
}

// Case-insensitive marker detection: "hidestart" / "hideend" also work.
TEST_F(DebuggerInterfaceTest, HideAnyHiddenCodeCaseInsensitiveMarkers) {
    char data[] = "beforehidestartSECREThideendafter";
    DWORD len = (DWORD)strlen(data);
    ASSERT_TRUE(CDebuggerInterface::HideAnyHiddenCode(data, len));
    EXPECT_STREQ(data, "beforehidestartXXXXXXhideendafter");
}

// -- SendDataToDebugger ------------------------------------------------------

// When no debugger window exists, SendDataToDebugger should return 0
// (the lResult stays at its initial value since FindWindowW returns NULL).
TEST_F(DebuggerInterfaceTest, SendDataToDebuggerReturnsZeroWhenNoDebugger) {
    char testData[] = "test";
    LRESULT result = CDebuggerInterface::SendDataToDebugger(1, testData, (DWORD)strlen(testData));
    EXPECT_EQ(result, 0);
}

// -- GetDataFromDebugger -----------------------------------------------------

// When no debugger message has arrived, GetDataFromDebugger should return
// an empty data buffer (5 zero bytes) with dwDataSize=4.
TEST_F(DebuggerInterfaceTest, GetDataFromDebuggerReturnsEmptyWhenNoMessage) {
    // Ensure no message is pending
    g_DebugInfo.SetMessageArrived(false);

    LPSTR pData = nullptr;
    DWORD dwSize = 0;
    CDebuggerInterface::GetDataFromDebugger(99, &pData, &dwSize);

    ASSERT_NE(pData, nullptr);
    EXPECT_EQ(dwSize, 4u);
    // Should be zeroed out
    for (DWORD i = 0; i < 5; ++i) {
        EXPECT_EQ(pData[i], '\0');
    }

    delete[] pData;
}

// -- State accessors ---------------------------------------------------------

// After InitDebuggerState, IsInternalDebuggerActive should be false.
TEST_F(DebuggerInterfaceTest, InitDebuggerStateSetsInternalDebuggerFalse) {
    EXPECT_FALSE(CDebuggerInterface::IsInternalDebuggerActive());
}

// GetDebuggerProcessInfo should return a zeroed PROCESS_INFORMATION after init.
TEST_F(DebuggerInterfaceTest, InitDebuggerStateZeroesProcessInfo) {
    PROCESS_INFORMATION& pi = CDebuggerInterface::GetDebuggerProcessInfo();
    EXPECT_EQ(pi.hProcess, nullptr);
    EXPECT_EQ(pi.hThread, nullptr);
    EXPECT_EQ(pi.dwProcessId, 0u);
    EXPECT_EQ(pi.dwThreadId, 0u);
}
