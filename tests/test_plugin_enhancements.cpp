#include <gtest/gtest.h>
#include <windows.h>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

typedef void (*PFN_CreateFileMap)(int iID, DWORD_PTR dwName, DWORD dwSize);
typedef void (*PFN_OpenFileMap)(int iID, DWORD_PTR dwName);
typedef void (*PFN_CloseFileMap)(int iID);
typedef void (*PFN_DestroyFileMap)(int iID);
typedef void (*PFN_SetFileMapDWORD)(int iID, DWORD dwOffset, DWORD dwValue);
typedef DWORD (*PFN_GetFileMapDWORD)(int iID, DWORD dwOffset);
typedef void (*PFN_SetFileMapString)(int iID, DWORD dwOffset, DWORD_PTR dwString);
typedef DWORD_PTR (*PFN_GetFileMapString)(DWORD_PTR dwDestStr, int iID, DWORD dwOffset);
typedef void (*PFN_SetEventAndWait)(int iID);

class EnhancementsModuleFixture : public ::testing::Test {
protected:
    HMODULE hMod = nullptr;
    PFN_CreateFileMap pCreateFileMap = nullptr;
    PFN_OpenFileMap pOpenFileMap = nullptr;
    PFN_CloseFileMap pCloseFileMap = nullptr;
    PFN_DestroyFileMap pDestroyFileMap = nullptr;
    PFN_SetFileMapDWORD pSetFileMapDWORD = nullptr;
    PFN_GetFileMapDWORD pGetFileMapDWORD = nullptr;
    PFN_SetFileMapString pSetFileMapString = nullptr;
    PFN_GetFileMapString pGetFileMapString = nullptr;

    void SetUp() override {
        // Load the EnhancementsFREE plugin from binary directory
        hMod = LoadLibraryA("plugins/EnhancementsFREE.dll");
        if (!hMod) {
            hMod = LoadLibraryA("EnhancementsFREE.dll");
        }
        ASSERT_NE(hMod, nullptr) << "Failed to load EnhancementsFREE.dll";

        pCreateFileMap = (PFN_CreateFileMap)GetProcAddress(hMod, "?CreateFileMap@@YAXH_KK@Z");
        pOpenFileMap = (PFN_OpenFileMap)GetProcAddress(hMod, "?OpenFileMap@@YAXH_K@Z");
        pCloseFileMap = (PFN_CloseFileMap)GetProcAddress(hMod, "?CloseFileMap@@YAXH@Z");
        pDestroyFileMap = (PFN_DestroyFileMap)GetProcAddress(hMod, "?DestroyFileMap@@YAXH@Z");
        pSetFileMapDWORD = (PFN_SetFileMapDWORD)GetProcAddress(hMod, "?SetFileMapDWORD@@YAXHKK@Z");
        pGetFileMapDWORD = (PFN_GetFileMapDWORD)GetProcAddress(hMod, "?GetFileMapDWORD@@YAKHK@Z");
        pSetFileMapString = (PFN_SetFileMapString)GetProcAddress(hMod, "?SetFileMapString@@YAXHK_K@Z");
        pGetFileMapString = (PFN_GetFileMapString)GetProcAddress(hMod, "?GetFileMapString@@YA_K_KHK@Z");

        ASSERT_NE(pCreateFileMap, nullptr);
        ASSERT_NE(pOpenFileMap, nullptr);
        ASSERT_NE(pDestroyFileMap, nullptr);
        ASSERT_NE(pSetFileMapDWORD, nullptr);
        ASSERT_NE(pGetFileMapDWORD, nullptr);
        ASSERT_NE(pSetFileMapString, nullptr);
    }

    void TearDown() override {
        if (pDestroyFileMap) {
            pDestroyFileMap(1);
            pDestroyFileMap(2);
        }
        if (hMod) {
            FreeLibrary(hMod);
            hMod = nullptr;
        }
    }
};

} // namespace

TEST_F(EnhancementsModuleFixture, FileMapLifecycleAndSizing) {
    const char* mapName = "DBP_TEST_MAP_LIFECYCLE";
    pCreateFileMap(1, (DWORD_PTR)mapName, 4096);

    // Write a DWORD and read it back
    pSetFileMapDWORD(1, 100, 0x12345678);
    DWORD val = pGetFileMapDWORD(1, 100);
    EXPECT_EQ(val, 0x12345678U);

    pDestroyFileMap(1);
}

TEST_F(EnhancementsModuleFixture, ValidStringExchange) {
    const char* mapName = "DBP_TEST_MAP_STRING";
    pCreateFileMap(1, (DWORD_PTR)mapName, 4096);

    const char* testStr = "C:\\Games\\FPSCreator\\Files\\entitybank\\test.fpe";
    pSetFileMapString(1, 1000, (DWORD_PTR)testStr);

    // Read directly using DWORD byte verification
    char buffer[256] = {};
    for (int i = 0; i < 48; i += 4) {
        DWORD dw = pGetFileMapDWORD(1, 1000 + i);
        memcpy(&buffer[i], &dw, 4);
    }
    EXPECT_STREQ(buffer, testStr);

    pDestroyFileMap(1);
}

TEST_F(EnhancementsModuleFixture, InvalidPointerSanitization) {
    const char* mapName = "DBP_TEST_MAP_POINTER_SAFETY";
    pCreateFileMap(1, (DWORD_PTR)mapName, 4096);

    // Test 1: NULL pointer
    EXPECT_NO_THROW({
        pSetFileMapString(1, 1000, 0);
    });

    // Test 2: Low-address unmapped pointer (< 64KB)
    EXPECT_NO_THROW({
        pSetFileMapString(1, 1000, 0x10);
    });

    // Test 3: Arbitrary garbage pointer in user-mode address space
    EXPECT_NO_THROW({
        pSetFileMapString(1, 1000, 0x00007FFFF0000000ULL);
    });

    // Test 4: High kernel-space address
    EXPECT_NO_THROW({
        pSetFileMapString(1, 1000, 0xFFFF800000000000ULL);
    });

    // Test 5: Passing invalid pointer as map name to CreateFileMap
    EXPECT_NO_THROW({
        pCreateFileMap(2, 0x0000000000000008ULL, 1024);
    });

    pDestroyFileMap(1);
    pDestroyFileMap(2);
}

TEST_F(EnhancementsModuleFixture, BufferBoundsOverflowProtection) {
    const char* mapName = "DBP_TEST_MAP_BOUNDS";
    // Allocate only 512 bytes
    pCreateFileMap(1, (DWORD_PTR)mapName, 512);

    // Try to write at offset 1000 (well beyond the 512-byte capacity)
    EXPECT_NO_THROW({
        pSetFileMapDWORD(1, 1000, 0xBAADF00D);
    });

    // Read beyond bounds should safely return 0 without crash
    DWORD val = 0;
    EXPECT_NO_THROW({
        val = pGetFileMapDWORD(1, 1000);
    });
    EXPECT_EQ(val, 0U);

    pDestroyFileMap(1);
}

TEST_F(EnhancementsModuleFixture, ExactBoundaryAndOverflowPrevention) {
    const char* mapName = "DBP_TEST_MAP_EXACT_BOUNDS";
    pCreateFileMap(1, (DWORD_PTR)mapName, 1024);

    // 1. Exact boundary: offset 1020 + 4 bytes == 1024
    pSetFileMapDWORD(1, 1020, 0xCAFEBABE);
    DWORD val = pGetFileMapDWORD(1, 1020);
    EXPECT_EQ(val, 0xCAFEBABEU);

    // 2. Off-by-one boundary breach: offset 1021 + 4 bytes == 1025 > 1024
    pSetFileMapDWORD(1, 1021, 0xDEADBEEF);
    DWORD valOff = pGetFileMapDWORD(1, 1021);
    EXPECT_EQ(valOff, 0U);

    // 3. Integer wrap-around attempt: offset 0xFFFFFFFC + 4 wraps to 0 in 32-bit math
    pSetFileMapDWORD(1, 0xFFFFFFFC, 0x11223344);
    DWORD valWrap = pGetFileMapDWORD(1, 0xFFFFFFFC);
    EXPECT_EQ(valWrap, 0U);

    pDestroyFileMap(1);
}

TEST_F(EnhancementsModuleFixture, SimulatedChildProcessHandshake) {
    // Simulates the exact handshake between FPSCreator.exe (Editor) and FPSC-MapEditor.exe
    const char* exchangeMap = "FPSEXCHANGE_TDD_SIM";
    pCreateFileMap(1, (DWORD_PTR)exchangeMap, 6144);

    // MapEditor sets executable name and parameters
    const char* gameExe = "FPSC-Game.exe";
    const char* gameParams = "-t";
    pSetFileMapString(1, 1000, (DWORD_PTR)gameExe);
    pSetFileMapString(1, 1256, (DWORD_PTR)gameParams);
    pSetFileMapDWORD(1, 920, 1); // Trigger signal

    // Editor checks trigger signal
    DWORD trigger = pGetFileMapDWORD(1, 920);
    EXPECT_EQ(trigger, 1U);

    // Read back string data
    char exeBuf[256] = {};
    for (int i = 0; i < 16; i += 4) {
        DWORD dw = pGetFileMapDWORD(1, 1000 + i);
        memcpy(&exeBuf[i], &dw, 4);
    }
    EXPECT_STREQ(exeBuf, gameExe);

    char paramBuf[64] = {};
    for (int i = 0; i < 8; i += 4) {
        DWORD dw = pGetFileMapDWORD(1, 1256 + i);
        memcpy(&paramBuf[i], &dw, 4);
    }
    EXPECT_STREQ(paramBuf, gameParams);

    // Editor acknowledges by clearing trigger and setting ack flag
    pSetFileMapDWORD(1, 920, 0);
    pSetFileMapDWORD(1, 44, 1);
    EXPECT_EQ(pGetFileMapDWORD(1, 920), 0U);
    EXPECT_EQ(pGetFileMapDWORD(1, 44), 1U);

    pDestroyFileMap(1);
}

