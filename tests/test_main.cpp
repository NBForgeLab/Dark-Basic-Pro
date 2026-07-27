#include <gtest/gtest.h>
#include <windows.h>
#include "CrashHandler.h"
#include "DB3.h"

// Define window globals needed by EXEBlock.cpp (which are normally defined in Main.cpp)
HWND g_hTempWindow = NULL;
HWND g_igLoader_HWND = NULL;

int main(int argc, char** argv) {
    // Tests run unattended: suppress blocking DB3_CRASH()/assert dialogs
    // exactly like the CLI compiler does in headless mode
    db3::g_bHeadlessMode = true;

    db3::SetupDiagnosticHandlers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
