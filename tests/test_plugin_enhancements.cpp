#include <gtest/gtest.h>
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <vector>
#include "globstruct.h"

namespace {

typedef void (*PFN_CreateFileMap)(int iID, DWORD_PTR dwName, DWORD dwSize);
typedef void (*PFN_OpenFileMap)(int iID, DWORD_PTR dwName);
typedef void (*PFN_CloseFileMap)(int iID);
typedef void (*PFN_DestroyFileMap)(int iID);
typedef void (*PFN_SetFileMapDWORD)(int iID, DWORD dwOffset, DWORD dwValue);
typedef DWORD (*PFN_GetFileMapDWORD)(int iID, DWORD dwOffset);
typedef void (*PFN_SetFileMapString)(int iID, DWORD dwOffset, DWORD_PTR dwString);
typedef DWORD_PTR (*PFN_GetFileMapString)(DWORD_PTR dwDestStr, int iID, DWORD dwOffset);
typedef void (*PFN_SetFileMapFloat)(int iID, DWORD dwOffset, float fValue);
typedef DWORD (*PFN_GetFileMapFloat)(int iID, DWORD dwOffset);
typedef void (*PFN_SetEventAndWait)(int iID);

typedef void (*PFN_CreateFileBlock)(int iID, char* szFilename);
typedef void (*PFN_CloseFileBlock)(int iID);
typedef void (*PFN_DeleteFileBlock)(int iID);
typedef int (*PFN_GetFileBlockExists)(int iID);
typedef int (*PFN_GetFileBlockDataExists)(int iID, char* szFile);
typedef void (*PFN_AddFileToBlock)(int iID, char* szFile);
typedef int (*PFN_GetFileBlockCount)(int iID);
typedef int (*PFN_GetFileBlockCompression)(int iID);
typedef void (*PFN_SetFileBlockCompression)(int iID, int iLevel);
typedef void (*PFN_SaveFileBlock)(int iID);
typedef void (*PFN_OpenFileBlock)(char* szFile, int iID, LPCSTR szKey);
typedef void (*PFN_ExtractFileFromBlock)(int iID, const char* szFilename, const char* szPath);
typedef int (*PFN_GetFileBlockSize)(int iID);
typedef void (*PFN_PerformCheckListForFileBlockData)(int iID);
typedef void (*PFN_ReceiveCoreDataPtr)(LPVOID pCore);

typedef int (*PFN_GetInstalledMemory)(int iReturn);
typedef int (*PFN_GetMemoryAvailable)(int iReturn);
typedef int (*PFN_GetMemoryPercentUsed)(void);
typedef int (*PFN_GetMemoryPercentFree)(void);

class EnhancementsModuleFixture : public ::testing::Test {
protected:
    HMODULE hMod = nullptr;

    // IPC & Shared Memory
    PFN_CreateFileMap pCreateFileMap = nullptr;
    PFN_OpenFileMap pOpenFileMap = nullptr;
    PFN_CloseFileMap pCloseFileMap = nullptr;
    PFN_DestroyFileMap pDestroyFileMap = nullptr;
    PFN_SetFileMapDWORD pSetFileMapDWORD = nullptr;
    PFN_GetFileMapDWORD pGetFileMapDWORD = nullptr;
    PFN_SetFileMapString pSetFileMapString = nullptr;
    PFN_GetFileMapString pGetFileMapString = nullptr;
    PFN_SetFileMapFloat pSetFileMapFloat = nullptr;
    PFN_GetFileMapFloat pGetFileMapFloat = nullptr;
    PFN_SetEventAndWait pSetEventAndWait = nullptr;

    // FileBlocks
    PFN_CreateFileBlock pCreateFileBlock = nullptr;
    PFN_CloseFileBlock pCloseFileBlock = nullptr;
    PFN_DeleteFileBlock pDeleteFileBlock = nullptr;
    PFN_GetFileBlockExists pGetFileBlockExists = nullptr;
    PFN_GetFileBlockDataExists pGetFileBlockDataExists = nullptr;
    PFN_AddFileToBlock pAddFileToBlock = nullptr;
    PFN_GetFileBlockCount pGetFileBlockCount = nullptr;
    PFN_GetFileBlockCompression pGetFileBlockCompression = nullptr;
    PFN_SetFileBlockCompression pSetFileBlockCompression = nullptr;
    PFN_SaveFileBlock pSaveFileBlock = nullptr;
    PFN_OpenFileBlock pOpenFileBlock = nullptr;
    PFN_ExtractFileFromBlock pExtractFileFromBlock = nullptr;
    PFN_GetFileBlockSize pGetFileBlockSize = nullptr;
    PFN_PerformCheckListForFileBlockData pPerformCheckListForFileBlockData = nullptr;
    PFN_ReceiveCoreDataPtr pReceiveCoreDataPtr = nullptr;

    // OSMemory
    PFN_GetInstalledMemory pGetInstalledMemory = nullptr;
    PFN_GetMemoryAvailable pGetMemoryAvailable = nullptr;
    PFN_GetMemoryPercentUsed pGetMemoryPercentUsed = nullptr;
    PFN_GetMemoryPercentFree pGetMemoryPercentFree = nullptr;

    void SetUp() override {
        // Load the EnhancementsFREE plugin from binary directory
        hMod = LoadLibraryA("plugins/EnhancementsFREE.dll");
        if (!hMod) {
            hMod = LoadLibraryA("EnhancementsFREE.dll");
        }
        ASSERT_NE(hMod, nullptr) << "Failed to load EnhancementsFREE.dll";

        // IPC FileMapping
        pCreateFileMap = (PFN_CreateFileMap)GetProcAddress(hMod, "?CreateFileMap@@YAXH_KK@Z");
        pOpenFileMap = (PFN_OpenFileMap)GetProcAddress(hMod, "?OpenFileMap@@YAXH_K@Z");
        pCloseFileMap = (PFN_CloseFileMap)GetProcAddress(hMod, "?CloseFileMap@@YAXH@Z");
        pDestroyFileMap = (PFN_DestroyFileMap)GetProcAddress(hMod, "?DestroyFileMap@@YAXH@Z");
        pSetFileMapDWORD = (PFN_SetFileMapDWORD)GetProcAddress(hMod, "?SetFileMapDWORD@@YAXHKK@Z");
        pGetFileMapDWORD = (PFN_GetFileMapDWORD)GetProcAddress(hMod, "?GetFileMapDWORD@@YAKHK@Z");
        pSetFileMapString = (PFN_SetFileMapString)GetProcAddress(hMod, "?SetFileMapString@@YAXHK_K@Z");
        pGetFileMapString = (PFN_GetFileMapString)GetProcAddress(hMod, "?GetFileMapString@@YA_K_KHK@Z");
        pSetFileMapFloat = (PFN_SetFileMapFloat)GetProcAddress(hMod, "?SetFileMapFloat@@YAXHKM@Z");
        pGetFileMapFloat = (PFN_GetFileMapFloat)GetProcAddress(hMod, "?GetFileMapFloat@@YAKHK@Z");
        pSetEventAndWait = (PFN_SetEventAndWait)GetProcAddress(hMod, "?SetEventAndWait@@YAXH@Z");

        // FileBlocks
        pCreateFileBlock = (PFN_CreateFileBlock)GetProcAddress(hMod, "?CreateFileBlock@@YAXHPEAD@Z");
        pCloseFileBlock = (PFN_CloseFileBlock)GetProcAddress(hMod, "?CloseFileBlock@@YAXH@Z");
        pDeleteFileBlock = (PFN_DeleteFileBlock)GetProcAddress(hMod, "?DeleteFileBlock@@YAXH@Z");
        pGetFileBlockExists = (PFN_GetFileBlockExists)GetProcAddress(hMod, "?GetFileBlockExists@@YAHH@Z");
        pGetFileBlockDataExists = (PFN_GetFileBlockDataExists)GetProcAddress(hMod, "?GetFileBlockDataExists@@YAHHPEAD@Z");
        pAddFileToBlock = (PFN_AddFileToBlock)GetProcAddress(hMod, "?AddFileToBlock@@YAXHPEAD@Z");
        pGetFileBlockCount = (PFN_GetFileBlockCount)GetProcAddress(hMod, "?GetFileBlockCount@@YAHH@Z");
        pGetFileBlockCompression = (PFN_GetFileBlockCompression)GetProcAddress(hMod, "?GetFileBlockCompression@@YAHH@Z");
        pSetFileBlockCompression = (PFN_SetFileBlockCompression)GetProcAddress(hMod, "?SetFileBlockCompression@@YAXHH@Z");
        pSaveFileBlock = (PFN_SaveFileBlock)GetProcAddress(hMod, "?SaveFileBlock@@YAXH@Z");
        pOpenFileBlock = (PFN_OpenFileBlock)GetProcAddress(hMod, "?OpenFileBlock@@YAXPEADHPEBD@Z");
        pExtractFileFromBlock = (PFN_ExtractFileFromBlock)GetProcAddress(hMod, "?ExtractFileFromBlock@@YAXHPEBD0@Z");
        pGetFileBlockSize = (PFN_GetFileBlockSize)GetProcAddress(hMod, "?GetFileBlockSize@@YAHH@Z");
        pPerformCheckListForFileBlockData = (PFN_PerformCheckListForFileBlockData)GetProcAddress(hMod, "?PerformCheckListForFileBlockData@@YAXH@Z");
        pReceiveCoreDataPtr = (PFN_ReceiveCoreDataPtr)GetProcAddress(hMod, "?ReceiveCoreDataPtr@@YAXPEAX@Z");

        // OSMemory
        pGetInstalledMemory = (PFN_GetInstalledMemory)GetProcAddress(hMod, "?GetInstalledMemory@@YAHH@Z");
        pGetMemoryAvailable = (PFN_GetMemoryAvailable)GetProcAddress(hMod, "?GetMemoryAvailable@@YAHH@Z");
        pGetMemoryPercentUsed = (PFN_GetMemoryPercentUsed)GetProcAddress(hMod, "?GetMemoryPercentUsed@@YAHXZ");
        pGetMemoryPercentFree = (PFN_GetMemoryPercentFree)GetProcAddress(hMod, "?GetMemoryPercentFree@@YAHXZ");

        ASSERT_NE(pCreateFileMap, nullptr);
        ASSERT_NE(pOpenFileMap, nullptr);
        ASSERT_NE(pDestroyFileMap, nullptr);
        ASSERT_NE(pSetFileMapDWORD, nullptr);
        ASSERT_NE(pGetFileMapDWORD, nullptr);
        ASSERT_NE(pSetFileMapString, nullptr);
        ASSERT_NE(pGetFileMapString, nullptr);
        ASSERT_NE(pSetFileMapFloat, nullptr);
        ASSERT_NE(pGetFileMapFloat, nullptr);

        ASSERT_NE(pCreateFileBlock, nullptr);
        ASSERT_NE(pCloseFileBlock, nullptr);
        ASSERT_NE(pGetFileBlockExists, nullptr);
        ASSERT_NE(pSaveFileBlock, nullptr);
        ASSERT_NE(pOpenFileBlock, nullptr);
        ASSERT_NE(pExtractFileFromBlock, nullptr);
        ASSERT_NE(pGetFileBlockSize, nullptr);
        ASSERT_NE(pPerformCheckListForFileBlockData, nullptr);

        ASSERT_NE(pGetInstalledMemory, nullptr);
        ASSERT_NE(pGetMemoryAvailable, nullptr);
        ASSERT_NE(pGetMemoryPercentUsed, nullptr);
        ASSERT_NE(pGetMemoryPercentFree, nullptr);
    }

    void TearDown() override {
        if (pCloseFileBlock) {
            pCloseFileBlock(1);
        }
        if (pDestroyFileMap) {
            pDestroyFileMap(1);
            pDestroyFileMap(2);
        }
        if (pReceiveCoreDataPtr) {
            pReceiveCoreDataPtr(nullptr);
        }
        if (hMod) {
            FreeLibrary(hMod);
            hMod = nullptr;
        }
    }
};

} // namespace

// ============================================================================
// IPC / FileMapping Tests
// ============================================================================

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

    // Read directly using pGetFileMapString (verifies SetupString standalone heap fallback)
    DWORD_PTR resPtr = pGetFileMapString(0, 1, 1000);
    ASSERT_NE(resPtr, 0ULL);
    EXPECT_STREQ(reinterpret_cast<const char*>(resPtr), testStr);
    HeapFree(GetProcessHeap(), 0, reinterpret_cast<LPVOID>(resPtr));

    // Verify DWORD raw layout in buffer as well
    char buffer[256] = {};
    for (int i = 0; i < 48; i += 4) {
        DWORD dw = pGetFileMapDWORD(1, 1000 + i);
        memcpy(&buffer[i], &dw, 4);
    }
    EXPECT_STREQ(buffer, testStr);

    pDestroyFileMap(1);
}

TEST_F(EnhancementsModuleFixture, FloatExchangeAndPrecision) {
    const char* mapName = "DBP_TEST_MAP_FLOAT";
    pCreateFileMap(1, (DWORD_PTR)mapName, 4096);

    const float testValue = 3.14159265f;
    pSetFileMapFloat(1, 200, testValue);

    DWORD rawBits = pGetFileMapFloat(1, 200);
    float readBack = 0.0f;
    memcpy(&readBack, &rawBits, sizeof(readBack));
    EXPECT_FLOAT_EQ(readBack, testValue);

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

// ============================================================================
// FileBlocks Tests
// ============================================================================

TEST_F(EnhancementsModuleFixture, FileBlocksBoundaryAndDirectoryPreservation) {
    char origCwd[MAX_PATH] = {};
    DWORD len = GetCurrentDirectoryA(MAX_PATH, origCwd);
    ASSERT_GT(len, 0U);

    ASSERT_NE(pCreateFileBlock, nullptr);
    ASSERT_NE(pGetFileBlockExists, nullptr);

    // Out-of-bounds IDs must not crash
    EXPECT_NO_THROW(pCreateFileBlock(-1, nullptr));
    EXPECT_NO_THROW(pCreateFileBlock(255, nullptr));
    EXPECT_EQ(pGetFileBlockExists(-1), 0);
    EXPECT_EQ(pGetFileBlockExists(255), 0);

    if (pCloseFileBlock) {
        EXPECT_NO_THROW(pCloseFileBlock(-1));
        EXPECT_NO_THROW(pCloseFileBlock(255));
    }

    // Valid ID with null filename must safely handle without crash
    EXPECT_NO_THROW(pCreateFileBlock(1, nullptr));

    // Create temporary valid zip block
    char tempPath[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, tempPath);
    char testZip[MAX_PATH] = {};
    snprintf(testZip, sizeof(testZip), "%stest_fb_tdd.zip", tempPath);

    EXPECT_NO_THROW(pCreateFileBlock(1, testZip));
    EXPECT_EQ(pGetFileBlockExists(1), 1);
    if (pCloseFileBlock) {
        EXPECT_NO_THROW(pCloseFileBlock(1));
    }
    DeleteFileA(testZip);

    char finalCwd[MAX_PATH] = {};
    GetCurrentDirectoryA(MAX_PATH, finalCwd);
    EXPECT_STREQ(origCwd, finalCwd);
}

TEST_F(EnhancementsModuleFixture, FileBlocksDataExistsAndSearch) {
    ASSERT_NE(pGetFileBlockDataExists, nullptr);

    // Invalid IDs must return 0 without crashing
    EXPECT_EQ(pGetFileBlockDataExists(-1, nullptr), 0);
    EXPECT_EQ(pGetFileBlockDataExists(255, nullptr), 0);
    EXPECT_EQ(pGetFileBlockDataExists(1, nullptr), 0);

    char tempPath[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, tempPath);
    char testZip[MAX_PATH] = {};
    snprintf(testZip, sizeof(testZip), "%stest_fb_search.zip", tempPath);

    pCreateFileBlock(1, testZip);
    EXPECT_EQ(pGetFileBlockExists(1), 1);

    // Query non-existent file inside empty archive
    char nonExistent[] = "missing_file.txt";
    EXPECT_EQ(pGetFileBlockDataExists(1, nonExistent), 0);

    if (pCloseFileBlock) {
        pCloseFileBlock(1);
    }
    DeleteFileA(testZip);
}

TEST_F(EnhancementsModuleFixture, FileBlocksLifecycleAndCompression) {
    ASSERT_NE(pCreateFileBlock, nullptr);
    ASSERT_NE(pGetFileBlockCompression, nullptr);
    ASSERT_NE(pSetFileBlockCompression, nullptr);

    char tempPath[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, tempPath);
    char testZip[MAX_PATH] = {};
    snprintf(testZip, sizeof(testZip), "%stest_fb_compression.zip", tempPath);

    pCreateFileBlock(1, testZip);
    EXPECT_EQ(pGetFileBlockExists(1), 1);

    // Default compression is 9
    EXPECT_EQ(pGetFileBlockCompression(1), 9);

    // Change compression level
    pSetFileBlockCompression(1, 5);
    EXPECT_EQ(pGetFileBlockCompression(1), 5);

    // Out-of-bounds IDs return 0 safely
    EXPECT_EQ(pGetFileBlockCompression(-1), 0);
    EXPECT_EQ(pGetFileBlockCompression(255), 0);
    EXPECT_NO_THROW(pSetFileBlockCompression(-1, 5));
    EXPECT_NO_THROW(pSetFileBlockCompression(255, 5));

    if (pCloseFileBlock) {
        pCloseFileBlock(1);
    }
    DeleteFileA(testZip);
}

static void DBPTestCreateDeleteString(DWORD_PTR* pdwPtr, DWORD dwSize) {
    if (*pdwPtr) {
        HeapFree(GetProcessHeap(), 0, reinterpret_cast<LPVOID>(*pdwPtr));
        *pdwPtr = 0;
    }
    if (dwSize > 0) {
        *pdwPtr = reinterpret_cast<DWORD_PTR>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSize));
    }
}

TEST_F(EnhancementsModuleFixture, FileBlocksArchiveCreationAndExtractionRoundtrip) {
    char origCwd[MAX_PATH] = {};
    DWORD len = GetCurrentDirectoryA(MAX_PATH, origCwd);
    ASSERT_GT(len, 0U);

    // Create isolated test environment in temp directory
    std::filesystem::path tempBase = std::filesystem::temp_directory_path() / "dbp_test_fb_roundtrip";
    std::error_code ec;
    std::filesystem::remove_all(tempBase, ec);
    std::filesystem::create_directories(tempBase / "source_data", ec);
    std::filesystem::create_directories(tempBase / "extracted_data", ec);

    std::filesystem::path srcDir = tempBase / "source_data";
    std::filesystem::path extDir = tempBase / "extracted_data";
    std::filesystem::path archiveFile = tempBase / "test_project.fpm";

    // Write source files to be archived
    const std::string headerContent = "FPSC_HEADER_SIGNATURE_V1_TEST_CONTENT_1234567890";
    const std::string mapContent = "\x01\x02\x03\x04\x05\x06\x07\x08BINARY_FPM_PAYLOAD";

    {
        std::ofstream hOut(srcDir / "header.dat", std::ios::binary);
        hOut.write(headerContent.data(), headerContent.size());
    }
    {
        std::ofstream mOut(srcDir / "map.fpmb", std::ios::binary);
        mOut.write(mapContent.data(), mapContent.size());
    }

    // Set working directory to source_data to simulate how FPS Creator operates
    SetCurrentDirectoryA(srcDir.string().c_str());

    // 1. Create file block
    std::string archivePathStr = archiveFile.string();
    pCreateFileBlock(1, archivePathStr.data());
    EXPECT_EQ(pGetFileBlockExists(1), 1);

    // 2. Add files
    char fnHeader[] = "header.dat";
    char fnMap[] = "map.fpmb";
    pAddFileToBlock(1, fnHeader);
    pAddFileToBlock(1, fnMap);

    // 3. Save file block (writes archive to disk)
    pSaveFileBlock(1);
    EXPECT_TRUE(std::filesystem::exists(archiveFile));
    EXPECT_GT(std::filesystem::file_size(archiveFile), 0U);

    // Verify GetFileBlockSize reports non-zero
    int blockSize = pGetFileBlockSize(1);
    EXPECT_GT(blockSize, 0);

    // 4. Close file block
    pCloseFileBlock(1);
    EXPECT_EQ(pGetFileBlockExists(1), 0);

    // Restore working directory
    SetCurrentDirectoryA(origCwd);

    // 5. Open file block
    pOpenFileBlock(archivePathStr.data(), 1, "mypassword");
    EXPECT_EQ(pGetFileBlockExists(1), 1);
    EXPECT_EQ(pGetFileBlockCount(1), 2);
    EXPECT_EQ(pGetFileBlockDataExists(1, fnHeader), 1);
    EXPECT_EQ(pGetFileBlockDataExists(1, fnMap), 1);

    char missingFile[] = "non_existent.ele";
    EXPECT_EQ(pGetFileBlockDataExists(1, missingFile), 0);

    // 6. Extract files to extracted_data
    std::string extDirStr = extDir.string();
    pExtractFileFromBlock(1, fnHeader, extDirStr.c_str());
    pExtractFileFromBlock(1, fnMap, extDirStr.c_str());

    pCloseFileBlock(1);
    EXPECT_EQ(pGetFileBlockExists(1), 0);

    // 7. Verify extracted content matches original byte-for-byte
    std::filesystem::path extHeaderPath = extDir / "header.dat";
    std::filesystem::path extMapPath = extDir / "map.fpmb";
    ASSERT_TRUE(std::filesystem::exists(extHeaderPath));
    ASSERT_TRUE(std::filesystem::exists(extMapPath));

    std::ifstream hIn(extHeaderPath, std::ios::binary);
    std::string extractedHeader((std::istreambuf_iterator<char>(hIn)), std::istreambuf_iterator<char>());
    EXPECT_EQ(extractedHeader, headerContent);

    std::ifstream mIn(extMapPath, std::ios::binary);
    std::string extractedMap((std::istreambuf_iterator<char>(mIn)), std::istreambuf_iterator<char>());
    EXPECT_EQ(extractedMap, mapContent);

    // Clean up
    std::filesystem::remove_all(tempBase, ec);

    // Verify CWD was preserved
    char finalCwd[MAX_PATH] = {};
    GetCurrentDirectoryA(MAX_PATH, finalCwd);
    EXPECT_STREQ(origCwd, finalCwd);
}

TEST_F(EnhancementsModuleFixture, FileBlocksChecklistDataExtraction) {
    ASSERT_NE(pReceiveCoreDataPtr, nullptr);
    ASSERT_NE(pPerformCheckListForFileBlockData, nullptr);

    GlobStruct testGlob{};
    testGlob.CreateDeleteString = &DBPTestCreateDeleteString;
    pReceiveCoreDataPtr(&testGlob);

    char origCwd[MAX_PATH] = {};
    DWORD len = GetCurrentDirectoryA(MAX_PATH, origCwd);
    ASSERT_GT(len, 0U);

    std::filesystem::path tempBase = std::filesystem::temp_directory_path() / "dbp_test_fb_checklist";
    std::error_code ec;
    std::filesystem::remove_all(tempBase, ec);
    std::filesystem::create_directories(tempBase, ec);

    std::filesystem::path f1 = tempBase / "header.dat";
    std::filesystem::path f2 = tempBase / "universephy.dbo";
    {
        std::ofstream o1(f1); o1 << "test1";
        std::ofstream o2(f2); o2 << "test2";
    }

    SetCurrentDirectoryA(tempBase.string().c_str());

    std::filesystem::path archiveFile = tempBase / "checklist_test.fpm";
    std::string archivePathStr = archiveFile.string();
    pCreateFileBlock(1, archivePathStr.data());

    char fn1[] = "header.dat";
    char fn2[] = "universephy.dbo";
    pAddFileToBlock(1, fn1);
    pAddFileToBlock(1, fn2);
    pSaveFileBlock(1);
    pCloseFileBlock(1);

    SetCurrentDirectoryA(origCwd);

    // Reopen and run checklist
    pOpenFileBlock(archivePathStr.data(), 1, "mypassword");
    EXPECT_EQ(pGetFileBlockExists(1), 1);

    pPerformCheckListForFileBlockData(1);
    EXPECT_TRUE(testGlob.checklistexists);
    EXPECT_EQ(testGlob.checklistqty, 2);

    // Verify filenames are intact and NOT truncated to 7 characters on x64
    std::vector<std::string> names;
    for (int i = 0; i < testGlob.checklistqty; ++i) {
        ASSERT_NE(testGlob.checklist[i].string, nullptr);
        names.push_back(testGlob.checklist[i].string);
    }
    EXPECT_NE(std::find(names.begin(), names.end(), "header.dat"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "universephy.dbo"), names.end());

    // Clean up checklist memory
    if (testGlob.checklist) {
        for (DWORD i = 0; i < testGlob.dwChecklistArraySize; ++i) {
            if (testGlob.checklist[i].string) {
                DWORD_PTR ptr = reinterpret_cast<DWORD_PTR>(testGlob.checklist[i].string);
                DBPTestCreateDeleteString(&ptr, 0);
            }
        }
        DWORD_PTR clPtr = reinterpret_cast<DWORD_PTR>(testGlob.checklist);
        DBPTestCreateDeleteString(&clPtr, 0);
        testGlob.checklist = nullptr;
    }

    pCloseFileBlock(1);
    pReceiveCoreDataPtr(nullptr);
    std::filesystem::remove_all(tempBase, ec);
}

TEST_F(EnhancementsModuleFixture, FileBlocksFailedOpenSafety) {
    char badPath[] = "C:\\_non_existent_folder_xyz_12345\\missing.fpm";
    EXPECT_NO_THROW(pOpenFileBlock(badPath, 1, "mypassword"));
    EXPECT_EQ(pGetFileBlockExists(1), 0);
}

// ============================================================================
// OSMemory Tests
// ============================================================================

TEST_F(EnhancementsModuleFixture, OSMemoryQueriesInstalledAndAvailable) {
    ASSERT_NE(pGetInstalledMemory, nullptr);
    ASSERT_NE(pGetMemoryAvailable, nullptr);

    // Query installed physical memory
    int installedKB = pGetInstalledMemory(0);
    int installedMB = pGetInstalledMemory(1);
    int installedGB = pGetInstalledMemory(2);

    EXPECT_GT(installedKB, 0);
    EXPECT_GT(installedMB, 0);
    EXPECT_GT(installedGB, 0);

    // Scaling consistency check: MB should be roughly KB / 1024
    EXPECT_NEAR(static_cast<double>(installedKB) / 1024.0, static_cast<double>(installedMB), 2.0);
    EXPECT_NEAR(static_cast<double>(installedMB) / 1024.0, static_cast<double>(installedGB), 2.0);

    // Query available physical memory
    int availKB = pGetMemoryAvailable(0);
    int availMB = pGetMemoryAvailable(1);
    int availGB = pGetMemoryAvailable(2);

    EXPECT_GT(availKB, 0);
    EXPECT_GT(availMB, 0);
    EXPECT_GE(availGB, 0);

    // Available must never exceed installed memory
    EXPECT_LE(availKB, installedKB);
    EXPECT_LE(availMB, installedMB);

    // Invalid return mode parameter must safely return 0
    EXPECT_EQ(pGetInstalledMemory(-1), 0);
    EXPECT_EQ(pGetInstalledMemory(3), 0);
    EXPECT_EQ(pGetMemoryAvailable(-1), 0);
    EXPECT_EQ(pGetMemoryAvailable(99), 0);
}

TEST_F(EnhancementsModuleFixture, OSMemoryPercentUsageAndFree) {
    ASSERT_NE(pGetMemoryPercentUsed, nullptr);
    ASSERT_NE(pGetMemoryPercentFree, nullptr);

    int percentUsed = pGetMemoryPercentUsed();
    int percentFree = pGetMemoryPercentFree();

    EXPECT_GE(percentUsed, 0);
    EXPECT_LE(percentUsed, 100);

    EXPECT_GE(percentFree, 0);
    EXPECT_LE(percentFree, 100);

    // Combined usage and free must equal 100%
    EXPECT_EQ(percentUsed + percentFree, 100);
}
