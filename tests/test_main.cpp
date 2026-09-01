#include <gtest/gtest.h>
#include "CrashHandler.h"
#include "DB3.h"

// g_hTempWindow / g_igLoader_HWND are defined once by dbp_compiler_lib
// (DBPCompiler.cpp). Every executable that links the library — including this
// one — gets them from there; redefining them here would be a duplicate symbol.

int main(int argc, char** argv) {
    // Tests run unattended: suppress blocking DB3_CRASH()/assert dialogs
    // exactly like the CLI compiler does in headless mode
    db3::g_bHeadlessMode = true;

    db3::SetupDiagnosticHandlers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
