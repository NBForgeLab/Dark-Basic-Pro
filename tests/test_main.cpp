#include <gtest/gtest.h>
#include <windows.h>

// Define window globals needed by EXEBlock.cpp (which are normally defined in Main.cpp)
HWND g_hTempWindow = NULL;
HWND g_igLoader_HWND = NULL;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
