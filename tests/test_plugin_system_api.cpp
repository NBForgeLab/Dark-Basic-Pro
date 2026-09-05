#include <gtest/gtest.h>
#include <windows.h>
#include <VersionHelpers.h>
#include <cstdint>

namespace {

typedef int (*PFN_TMEMAvailable)(void);
typedef int (*PFN_DMEMAvailable)(void);
typedef int (*PFN_SMEMAvailable)(void);
typedef int (*PFN_SMEMAvailableMode)(int);

class SystemPluginFixture : public ::testing::Test {
protected:
    HMODULE hMod = nullptr;
    PFN_TMEMAvailable pTMEMAvailable = nullptr;
    PFN_DMEMAvailable pDMEMAvailable = nullptr;
    PFN_SMEMAvailable pSMEMAvailable = nullptr;
    PFN_SMEMAvailableMode pSMEMAvailableMode = nullptr;

    void SetUp() override {
        hMod = LoadLibraryA("plugins/DBProSystemDebug.dll");
        if (!hMod) {
            hMod = LoadLibraryA("DBProSystemDebug.dll");
        }
        ASSERT_NE(hMod, nullptr) << "Failed to load DBProSystemDebug.dll";

        pTMEMAvailable = (PFN_TMEMAvailable)GetProcAddress(hMod, "?TMEMAvailable@@YAHXZ");
        pDMEMAvailable = (PFN_DMEMAvailable)GetProcAddress(hMod, "?DMEMAvailable@@YAHXZ");
        pSMEMAvailable = (PFN_SMEMAvailable)GetProcAddress(hMod, "?SMEMAvailable@@YAHXZ");
        pSMEMAvailableMode = (PFN_SMEMAvailableMode)GetProcAddress(hMod, "?SMEMAvailable@@YAHH@Z");

        ASSERT_NE(pTMEMAvailable, nullptr);
        ASSERT_NE(pDMEMAvailable, nullptr);
        ASSERT_NE(pSMEMAvailable, nullptr);
        ASSERT_NE(pSMEMAvailableMode, nullptr);
    }

    void TearDown() override {
        if (hMod) {
            FreeLibrary(hMod);
            hMod = nullptr;
        }
    }
};

} // namespace

TEST_F(SystemPluginFixture, TMEMAvailableReflects64BitPhysicalMemory) {
    MEMORYSTATUSEX statex{};
    statex.dwLength = sizeof(statex);
    ASSERT_TRUE(GlobalMemoryStatusEx(&statex));

    int actual = pTMEMAvailable();
    int expected = static_cast<int>((statex.ullTotalPhys + (1024 * 1024 - 1)) / (1024 * 1024));

    EXPECT_GT(actual, 0);
    EXPECT_GE(actual, 1024); // At least 1GB installed
    EXPECT_NEAR(actual, expected, 5); // Within rounding tolerance
}

TEST_F(SystemPluginFixture, SMEMAvailableReflectsCurrentAvailableMemory) {
    MEMORYSTATUSEX statex{};
    statex.dwLength = sizeof(statex);
    ASSERT_TRUE(GlobalMemoryStatusEx(&statex));

    int actual = pSMEMAvailable();
    int total = static_cast<int>((statex.ullTotalPhys + (1024 * 1024 - 1)) / (1024 * 1024));

    EXPECT_GT(actual, 0);
    EXPECT_LE(actual, total);
}

TEST_F(SystemPluginFixture, SMEMAvailableModesProvideProcessAndVirtualStats) {
    // Mode 2: Virtual Memory Used in KB
    int vmKB = pSMEMAvailableMode(2);
    EXPECT_GT(vmKB, 0);

    // Mode 0: Process pagefile usage in KB
    int procKB = pSMEMAvailableMode(0);
    EXPECT_GE(procKB, 0);
}

TEST_F(SystemPluginFixture, DMEMAvailableExecutesSafelyWithoutCrash) {
    EXPECT_NO_THROW({
        int dmem = pDMEMAvailable();
        EXPECT_GE(dmem, 0);
    });
}

TEST(SystemVersionHelpersTest, WindowsVersionDetectionAccurate) {
    // Windows 8 or greater helper must be true on Windows 10/11
    EXPECT_TRUE(IsWindows8OrGreater());
    EXPECT_TRUE(IsWindows8Point1OrGreater() || IsWindows8OrGreater());
}
