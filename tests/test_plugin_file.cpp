//
// test_plugin_file.cpp - Comprehensive Unit Tests for DBProFileDebug.dll
//

#include <gtest/gtest.h>
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include "globstruct.h"
#include "cruntimeerrors.h"

namespace {

// Typedefs for DBProFileDebug.dll exports
typedef void (*PFN_Constructor)(void);
typedef void (*PFN_Destructor)(void);
typedef void (*PFN_SetErrorHandler)(LPVOID);
typedef void (*PFN_PassCoreData)(LPVOID);

typedef int (*PFN_FileExist)(DWORD_PTR);
typedef int (*PFN_FileSize)(DWORD_PTR);
typedef int (*PFN_PathExist)(DWORD_PTR);
typedef int (*PFN_CanMakeFile)(DWORD_PTR);
typedef void (*PFN_MakeFile)(DWORD_PTR);
typedef void (*PFN_DeleteFileA)(DWORD_PTR);
typedef void (*PFN_CopyFileA)(DWORD_PTR, DWORD_PTR);
typedef void (*PFN_MoveFileA)(DWORD_PTR, DWORD_PTR);
typedef void (*PFN_RenameFile)(DWORD_PTR, DWORD_PTR);

typedef void (*PFN_MakeDir)(DWORD_PTR);
typedef void (*PFN_DeleteDir)(DWORD_PTR);
typedef void (*PFN_SetDir)(DWORD_PTR);
typedef DWORD_PTR (*PFN_GetDir)(DWORD_PTR);

typedef void (*PFN_FindFirst)(void);
typedef void (*PFN_FindNext)(void);
typedef DWORD_PTR (*PFN_GetFileName)(DWORD_PTR);
typedef int (*PFN_GetFileType)(void);

typedef void (*PFN_OpenToRead)(int, DWORD_PTR);
typedef void (*PFN_OpenToWrite)(int, DWORD_PTR);
typedef void (*PFN_CloseFile)(int);
typedef int (*PFN_FileOpen)(int);
typedef int (*PFN_FileEnd)(int);

typedef int (*PFN_ReadByte)(int);
typedef int (*PFN_ReadWord)(int);
typedef int (*PFN_ReadLong)(int);
typedef DWORD (*PFN_ReadFloat)(int);
typedef DWORD_PTR (*PFN_ReadString)(int, DWORD_PTR);

typedef void (*PFN_WriteByte)(int, int);
typedef void (*PFN_WriteWord)(int, int);
typedef void (*PFN_WriteLong)(int, int);
typedef void (*PFN_WriteFloat)(int, float);
typedef void (*PFN_WriteString)(int, DWORD_PTR);

typedef void (*PFN_WriteByteToFile)(DWORD_PTR, int, int);
typedef int (*PFN_ReadByteFromFile)(DWORD_PTR, int);

typedef const DBP_DiagnosticContext* (*PFN_GetLastDiagnosticContext)(void);
typedef void (*PFN_ClearLastDiagnosticContext)(void);

static void DBPTestCreateDeleteString(DWORD_PTR* pdwPtr, DWORD dwSize) {
    if (*pdwPtr) {
        HeapFree(GetProcessHeap(), 0, reinterpret_cast<LPVOID>(*pdwPtr));
        *pdwPtr = 0;
    }
    if (dwSize > 0) {
        *pdwPtr = reinterpret_cast<DWORD_PTR>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSize));
    }
}

class CurrentDirectoryGuard {
    char m_OrigCwd[MAX_PATH] = {};
public:
    CurrentDirectoryGuard() {
        GetCurrentDirectoryA(MAX_PATH, m_OrigCwd);
    }
    ~CurrentDirectoryGuard() {
        SetCurrentDirectoryA(m_OrigCwd);
    }
};

class FilePluginFixture : public ::testing::Test {
protected:
    HMODULE hMod = nullptr;
    CRuntimeErrorHandler m_TestErrorHandler{0};
    GlobStruct m_TestGlob{};

    PFN_Constructor pConstructor = nullptr;
    PFN_Destructor pDestructor = nullptr;
    PFN_SetErrorHandler pSetErrorHandler = nullptr;
    PFN_PassCoreData pPassCoreData = nullptr;

    PFN_FileExist pFileExist = nullptr;
    PFN_FileSize pFileSize = nullptr;
    PFN_PathExist pPathExist = nullptr;
    PFN_CanMakeFile pCanMakeFile = nullptr;
    PFN_MakeFile pMakeFile = nullptr;
    PFN_DeleteFileA pDeleteFileA = nullptr;
    PFN_CopyFileA pCopyFileA = nullptr;
    PFN_MoveFileA pMoveFileA = nullptr;
    PFN_RenameFile pRenameFile = nullptr;

    PFN_MakeDir pMakeDir = nullptr;
    PFN_DeleteDir pDeleteDir = nullptr;
    PFN_SetDir pSetDir = nullptr;
    PFN_GetDir pGetDir = nullptr;

    PFN_FindFirst pFindFirst = nullptr;
    PFN_FindNext pFindNext = nullptr;
    PFN_GetFileName pGetFileName = nullptr;
    PFN_GetFileType pGetFileType = nullptr;

    PFN_OpenToRead pOpenToRead = nullptr;
    PFN_OpenToWrite pOpenToWrite = nullptr;
    PFN_CloseFile pCloseFile = nullptr;
    PFN_FileOpen pFileOpen = nullptr;
    PFN_FileEnd pFileEnd = nullptr;

    PFN_ReadByte pReadByte = nullptr;
    PFN_ReadWord pReadWord = nullptr;
    PFN_ReadLong pReadLong = nullptr;
    PFN_ReadFloat pReadFloat = nullptr;
    PFN_ReadString pReadString = nullptr;

    PFN_WriteByte pWriteByte = nullptr;
    PFN_WriteWord pWriteWord = nullptr;
    PFN_WriteLong pWriteLong = nullptr;
    PFN_WriteFloat pWriteFloat = nullptr;
    PFN_WriteString pWriteString = nullptr;

    PFN_WriteByteToFile pWriteByteToFile = nullptr;
    PFN_ReadByteFromFile pReadByteFromFile = nullptr;

    PFN_GetLastDiagnosticContext pGetLastDiagnosticContext = nullptr;
    PFN_ClearLastDiagnosticContext pClearLastDiagnosticContext = nullptr;

    void SetUp() override {
        hMod = LoadLibraryA("plugins/DBProFileDebug.dll");
        if (!hMod) {
            hMod = LoadLibraryA("DBProFileDebug.dll");
        }
        ASSERT_NE(hMod, nullptr) << "Failed to load DBProFileDebug.dll";

        pConstructor = (PFN_Constructor)GetProcAddress(hMod, "?Constructor@@YAXXZ");
        pDestructor = (PFN_Destructor)GetProcAddress(hMod, "?Destructor@@YAXXZ");
        pSetErrorHandler = (PFN_SetErrorHandler)GetProcAddress(hMod, "?SetErrorHandler@@YAXPEAX@Z");
        pPassCoreData = (PFN_PassCoreData)GetProcAddress(hMod, "?PassCoreData@@YAXPEAX@Z");

        pFileExist = (PFN_FileExist)GetProcAddress(hMod, "?FileExist@@YAH_K@Z");
        pFileSize = (PFN_FileSize)GetProcAddress(hMod, "?FileSize@@YAH_K@Z");
        pPathExist = (PFN_PathExist)GetProcAddress(hMod, "?PathExist@@YAH_K@Z");
        pCanMakeFile = (PFN_CanMakeFile)GetProcAddress(hMod, "?CanMakeFile@@YAH_K@Z");
        pMakeFile = (PFN_MakeFile)GetProcAddress(hMod, "?MakeFile@@YAX_K@Z");
        pDeleteFileA = (PFN_DeleteFileA)GetProcAddress(hMod, "?DeleteFileA@@YAX_K@Z");
        pCopyFileA = (PFN_CopyFileA)GetProcAddress(hMod, "?CopyFileA@@YAX_K0@Z");
        pMoveFileA = (PFN_MoveFileA)GetProcAddress(hMod, "?MoveFileA@@YAX_K0@Z");
        pRenameFile = (PFN_RenameFile)GetProcAddress(hMod, "?RenameFile@@YAX_K0@Z");

        pMakeDir = (PFN_MakeDir)GetProcAddress(hMod, "?MakeDir@@YAX_K@Z");
        pDeleteDir = (PFN_DeleteDir)GetProcAddress(hMod, "?DeleteDir@@YAX_K@Z");
        pSetDir = (PFN_SetDir)GetProcAddress(hMod, "?SetDir@@YAX_K@Z");
        pGetDir = (PFN_GetDir)GetProcAddress(hMod, "?GetDir@@YA_K_K@Z");

        pFindFirst = (PFN_FindFirst)GetProcAddress(hMod, "?FindFirst@@YAXXZ");
        pFindNext = (PFN_FindNext)GetProcAddress(hMod, "?FindNext@@YAXXZ");
        pGetFileName = (PFN_GetFileName)GetProcAddress(hMod, "?GetFileName@@YA_K_K@Z");
        pGetFileType = (PFN_GetFileType)GetProcAddress(hMod, "?GetFileType@@YAHXZ");

        pOpenToRead = (PFN_OpenToRead)GetProcAddress(hMod, "?OpenToRead@@YAXH_K@Z");
        pOpenToWrite = (PFN_OpenToWrite)GetProcAddress(hMod, "?OpenToWrite@@YAXH_K@Z");
        pCloseFile = (PFN_CloseFile)GetProcAddress(hMod, "?CloseFile@@YAXH@Z");
        pFileOpen = (PFN_FileOpen)GetProcAddress(hMod, "?FileOpen@@YAHH@Z");
        pFileEnd = (PFN_FileEnd)GetProcAddress(hMod, "?FileEnd@@YAHH@Z");

        pReadByte = (PFN_ReadByte)GetProcAddress(hMod, "?ReadByte@@YAHH@Z");
        pReadWord = (PFN_ReadWord)GetProcAddress(hMod, "?ReadWord@@YAHH@Z");
        pReadLong = (PFN_ReadLong)GetProcAddress(hMod, "?ReadLong@@YAHH@Z");
        pReadFloat = (PFN_ReadFloat)GetProcAddress(hMod, "?ReadFloat@@YAKH@Z");
        pReadString = (PFN_ReadString)GetProcAddress(hMod, "?ReadString@@YA_KH_K@Z");

        pWriteByte = (PFN_WriteByte)GetProcAddress(hMod, "?WriteByte@@YAXHH@Z");
        pWriteWord = (PFN_WriteWord)GetProcAddress(hMod, "?WriteWord@@YAXHH@Z");
        pWriteLong = (PFN_WriteLong)GetProcAddress(hMod, "?WriteLong@@YAXHH@Z");
        pWriteFloat = (PFN_WriteFloat)GetProcAddress(hMod, "?WriteFloat@@YAXHM@Z");
        pWriteString = (PFN_WriteString)GetProcAddress(hMod, "?WriteString@@YAXH_K@Z");

        pWriteByteToFile = (PFN_WriteByteToFile)GetProcAddress(hMod, "?WriteByteToFile@@YAX_KHH@Z");
        pReadByteFromFile = (PFN_ReadByteFromFile)GetProcAddress(hMod, "?ReadByteFromFile@@YAH_KH@Z");

        pGetLastDiagnosticContext = (PFN_GetLastDiagnosticContext)GetProcAddress(hMod, "?GetLastDiagnosticContext@@YAPEBUDBP_DiagnosticContext@@XZ");
        pClearLastDiagnosticContext = (PFN_ClearLastDiagnosticContext)GetProcAddress(hMod, "?ClearLastDiagnosticContext@@YAXXZ");

        ASSERT_NE(pFileExist, nullptr);
        ASSERT_NE(pFileSize, nullptr);
        ASSERT_NE(pOpenToRead, nullptr);
        ASSERT_NE(pOpenToWrite, nullptr);
        ASSERT_NE(pCloseFile, nullptr);
        ASSERT_NE(pReadString, nullptr);
        ASSERT_NE(pWriteString, nullptr);

        // Setup GlobStruct & ErrorHandler
        m_TestErrorHandler.dwErrorCode = 0;
        m_TestGlob.CreateDeleteString = &DBPTestCreateDeleteString;
        if (pSetErrorHandler) pSetErrorHandler(&m_TestErrorHandler);
        if (pPassCoreData) pPassCoreData(&m_TestGlob);
        if (pClearLastDiagnosticContext) pClearLastDiagnosticContext();
    }

    void TearDown() override {
        // Ensure any files left open are closed
        if (pCloseFile && pFileOpen) {
            for (int f = 1; f <= 64; ++f) {
                if (pFileOpen(f)) {
                    pCloseFile(f);
                }
            }
        }
        if (pSetErrorHandler) pSetErrorHandler(nullptr);
        if (pPassCoreData) pPassCoreData(nullptr);
        if (hMod) {
            FreeLibrary(hMod);
            hMod = nullptr;
        }
    }
};

} // namespace

TEST_F(FilePluginFixture, FileLifecycleAndByteOperations) {
    std::filesystem::path tempBase = std::filesystem::temp_directory_path() / "dbp_test_file_lifecycle";
    std::error_code ec;
    std::filesystem::remove_all(tempBase, ec);
    std::filesystem::create_directories(tempBase, ec);

    std::filesystem::path testFilePath = tempBase / "test_lifecycle.dat";
    std::string testFileStr = testFilePath.string();

    // 1. Initially does not exist
    EXPECT_EQ(pFileExist(reinterpret_cast<DWORD_PTR>(testFileStr.c_str())), 0);

    // 2. MakeFile creates empty file
    pMakeFile(reinterpret_cast<DWORD_PTR>(testFileStr.c_str()));
    EXPECT_EQ(pFileExist(reinterpret_cast<DWORD_PTR>(testFileStr.c_str())), 1);
    EXPECT_EQ(pFileSize(reinterpret_cast<DWORD_PTR>(testFileStr.c_str())), 0);

    // In DBPro semantics, OpenToWrite expects the file to not exist yet; delete empty file before stream write
    pDeleteFileA(reinterpret_cast<DWORD_PTR>(testFileStr.c_str()));
    EXPECT_EQ(pFileExist(reinterpret_cast<DWORD_PTR>(testFileStr.c_str())), 0);

    // 3. Write initial byte via stream
    pOpenToWrite(1, reinterpret_cast<DWORD_PTR>(testFileStr.c_str()));
    pWriteByte(1, 0x12);
    pCloseFile(1);
    EXPECT_EQ(pFileSize(reinterpret_cast<DWORD_PTR>(testFileStr.c_str())), 1);

    // 4. Modify byte at pos 0 via WriteByteToFile
    pWriteByteToFile(reinterpret_cast<DWORD_PTR>(testFileStr.c_str()), 0, 0xAB);
    EXPECT_EQ(pFileSize(reinterpret_cast<DWORD_PTR>(testFileStr.c_str())), 1);

    // 5. ReadByteFromFile at pos 0
    int byteVal = pReadByteFromFile(reinterpret_cast<DWORD_PTR>(testFileStr.c_str()), 0);
    EXPECT_EQ(byteVal, 0xAB);

    // 6. DeleteFile
    pDeleteFileA(reinterpret_cast<DWORD_PTR>(testFileStr.c_str()));
    EXPECT_EQ(pFileExist(reinterpret_cast<DWORD_PTR>(testFileStr.c_str())), 0);

    std::filesystem::remove_all(tempBase, ec);
}

TEST_F(FilePluginFixture, DirectoryManagementAndNavigation) {
    CurrentDirectoryGuard cwdGuard;

    std::filesystem::path tempBase = std::filesystem::temp_directory_path() / "dbp_test_file_dir_ops";
    std::error_code ec;
    std::filesystem::remove_all(tempBase, ec);
    std::filesystem::create_directories(tempBase, ec);

    std::filesystem::path subDir = tempBase / "weapons_bank";
    std::string subDirStr = subDir.string();

    // 1. Initially path does not exist
    EXPECT_EQ(pPathExist(reinterpret_cast<DWORD_PTR>(subDirStr.c_str())), 0);

    // 2. MakeDir
    pMakeDir(reinterpret_cast<DWORD_PTR>(subDirStr.c_str()));
    EXPECT_EQ(pPathExist(reinterpret_cast<DWORD_PTR>(subDirStr.c_str())), 1);

    // 3. SetDir into new folder
    pSetDir(reinterpret_cast<DWORD_PTR>(subDirStr.c_str()));

    // 4. Query GetDir
    DWORD_PTR dirStrPtr = 0;
    dirStrPtr = pGetDir(0);
    if (dirStrPtr) {
        std::string currentDir = reinterpret_cast<const char*>(dirStrPtr);
        EXPECT_FALSE(currentDir.empty());
        HeapFree(GetProcessHeap(), 0, reinterpret_cast<LPVOID>(dirStrPtr));
    }

    // 5. Restore CWD and DeleteDir
    cwdGuard.~CurrentDirectoryGuard();
    pDeleteDir(reinterpret_cast<DWORD_PTR>(subDirStr.c_str()));
    EXPECT_EQ(pPathExist(reinterpret_cast<DWORD_PTR>(subDirStr.c_str())), 0);

    std::filesystem::remove_all(tempBase, ec);
}

TEST_F(FilePluginFixture, StreamReadWriteRoundtrip) {
    std::filesystem::path tempBase = std::filesystem::temp_directory_path() / "dbp_test_file_stream";
    std::error_code ec;
    std::filesystem::remove_all(tempBase, ec);
    std::filesystem::create_directories(tempBase, ec);

    std::filesystem::path streamFile = tempBase / "stream_data.bin";
    std::string streamFileStr = streamFile.string();

    const int kFileId = 1;

    // 1. OpenToWrite
    pOpenToWrite(kFileId, reinterpret_cast<DWORD_PTR>(streamFileStr.c_str()));
    EXPECT_EQ(pFileOpen(kFileId), 1);

    // 2. Write formatted binary and string data
    pWriteByte(kFileId, 255);
    pWriteWord(kFileId, 65530);
    pWriteLong(kFileId, 0x12345678);

    const float testFloat = 123.456f;
    pWriteFloat(kFileId, testFloat);

    const char* testMsg = "FPSC_TDD_VERIFICATION_STRING";
    pWriteString(kFileId, reinterpret_cast<DWORD_PTR>(testMsg));

    // 3. CloseFile
    pCloseFile(kFileId);
    EXPECT_EQ(pFileOpen(kFileId), 0);
    EXPECT_GT(pFileSize(reinterpret_cast<DWORD_PTR>(streamFileStr.c_str())), 0);

    // 4. OpenToRead
    pOpenToRead(kFileId, reinterpret_cast<DWORD_PTR>(streamFileStr.c_str()));
    EXPECT_EQ(pFileOpen(kFileId), 1);
    EXPECT_EQ(pFileEnd(kFileId), 0);

    // 5. Read back and verify
    int b = pReadByte(kFileId);
    EXPECT_EQ(b, 255);

    int w = pReadWord(kFileId);
    EXPECT_EQ(w, 65530);

    int l = pReadLong(kFileId);
    EXPECT_EQ(l, 0x12345678);

    DWORD rawF = pReadFloat(kFileId);
    float f = 0.0f;
    memcpy(&f, &rawF, sizeof(f));
    EXPECT_FLOAT_EQ(f, testFloat);

    DWORD_PTR strDest = pReadString(kFileId, 0);
    EXPECT_NE(strDest, 0ULL);
    if (strDest) {
        EXPECT_STREQ(reinterpret_cast<const char*>(strDest), testMsg);
        HeapFree(GetProcessHeap(), 0, reinterpret_cast<LPVOID>(strDest));
    }

    // 6. Check FileEnd: reading past end of file triggers FileEOF
    pReadByte(kFileId);
    EXPECT_EQ(pFileEnd(kFileId), 1);
    pCloseFile(kFileId);
    EXPECT_EQ(pFileOpen(kFileId), 0);

    std::filesystem::remove_all(tempBase, ec);
}

TEST_F(FilePluginFixture, FileSearchIterationDiscoversFiles) {
    CurrentDirectoryGuard cwdGuard;

    std::filesystem::path tempBase = std::filesystem::temp_directory_path() / "dbp_test_file_search";
    std::error_code ec;
    std::filesystem::remove_all(tempBase, ec);
    std::filesystem::create_directories(tempBase, ec);

    // Create 3 specific files
    std::vector<std::string> expectedFiles = { "gun.dbo", "script.fpi", "weapon.fpe" };
    for (const auto& name : expectedFiles) {
        std::filesystem::path fp = tempBase / name;
        pMakeFile(reinterpret_cast<DWORD_PTR>(fp.string().c_str()));
    }

    // Set working directory to search directory
    pSetDir(reinterpret_cast<DWORD_PTR>(tempBase.string().c_str()));

    // Iterate directory files
    std::vector<std::string> foundFiles;
    pFindFirst();
    for (int i = 0; i < 32; ++i) {
        DWORD_PTR fnPtr = pGetFileName(0);
        if (fnPtr) {
            std::string fname = reinterpret_cast<const char*>(fnPtr);
            if (!fname.empty() && fname != "." && fname != "..") {
                foundFiles.push_back(fname);
            }
            HeapFree(GetProcessHeap(), 0, reinterpret_cast<LPVOID>(fnPtr));
        }
        if (pGetFileType() == -1) break;
        pFindNext();
    }

    // Verify all 3 expected files were discovered
    for (const auto& exp : expectedFiles) {
        auto it = std::find(foundFiles.begin(), foundFiles.end(), exp);
        EXPECT_NE(it, foundFiles.end()) << "Expected file not found in search: " << exp;
    }

    std::filesystem::remove_all(tempBase, ec);
}

TEST_F(FilePluginFixture, Diagnostics_NonExistentFileTriggersRuntimeError) {
    m_TestErrorHandler.dwErrorCode = 0;
    if (pClearLastDiagnosticContext) pClearLastDiagnosticContext();

    const char* nonExistent = "C:\\_dbp_tdd_missing_dir_xyz\\missing_file.dat";
    pOpenToRead(1, reinterpret_cast<DWORD_PTR>(nonExistent));

    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_FILENOTEXIST));

    if (pGetLastDiagnosticContext) {
        const DBP_DiagnosticContext* ctx = pGetLastDiagnosticContext();
        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_FILENOTEXIST));
        EXPECT_STREQ(ctx->description, "File does not exist");
    }
}

TEST_F(FilePluginFixture, Diagnostics_DoubleOpenTriggersFileAlreadyOpen) {
    std::filesystem::path tempBase = std::filesystem::temp_directory_path() / "dbp_test_file_double_open";
    std::error_code ec;
    std::filesystem::remove_all(tempBase, ec);
    std::filesystem::create_directories(tempBase, ec);

    std::filesystem::path testFile = tempBase / "sample.dat";
    std::string testFileStr = testFile.string();
    pMakeFile(reinterpret_cast<DWORD_PTR>(testFileStr.c_str()));

    m_TestErrorHandler.dwErrorCode = 0;
    pOpenToRead(1, reinterpret_cast<DWORD_PTR>(testFileStr.c_str()));
    EXPECT_EQ(pFileOpen(1), 1);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, 0U);

    // Attempt to open again on same file index 1
    pOpenToRead(1, reinterpret_cast<DWORD_PTR>(testFileStr.c_str()));
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_FILEALREADYOPEN));

    if (pGetLastDiagnosticContext) {
        const DBP_DiagnosticContext* ctx = pGetLastDiagnosticContext();
        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_FILEALREADYOPEN));
        EXPECT_STREQ(ctx->description, "File already open");
    }

    pCloseFile(1);
    std::filesystem::remove_all(tempBase, ec);
}

TEST_F(FilePluginFixture, Diagnostics_InvalidFileNumberTriggersError) {
    m_TestErrorHandler.dwErrorCode = 0;
    pOpenToRead(0, reinterpret_cast<DWORD_PTR>("valid_or_not.dat"));
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_FILENUMBERINVALID));

    m_TestErrorHandler.dwErrorCode = 0;
    pOpenToRead(65, reinterpret_cast<DWORD_PTR>("valid_or_not.dat"));
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_FILENUMBERINVALID));
}

TEST_F(FilePluginFixture, PointerSafety_NullAndInvalidPointersDoNotCrash) {
    EXPECT_NO_THROW({
        EXPECT_EQ(pFileExist(0), 0);
        EXPECT_EQ(pFileSize(0), 0);
        EXPECT_EQ(pPathExist(0), 0);
        pMakeFile(0);
        pDeleteFileA(0);
        pOpenToRead(1, 0);
    });
}
