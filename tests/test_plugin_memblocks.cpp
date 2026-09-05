//
// test_plugin_memblocks.cpp - Comprehensive Unit Tests for DBProMemblocksDebug.dll
//

#include <gtest/gtest.h>
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "globstruct.h"
#include "cruntimeerrors.h"

namespace {

typedef void (*PFN_Constructor)(void);
typedef void (*PFN_Destructor)(void);
typedef void (*PFN_SetErrorHandler)(LPVOID);
typedef void (*PFN_PassCoreData)(LPVOID);

typedef void (*PFN_MakeMemblock)(int, int);
typedef void (*PFN_DeleteMemblock)(int);
typedef int (*PFN_MemblockExist)(int);
typedef int (*PFN_GetMemblockSize)(int);
typedef char* (*PFN_GetMemblockPtr)(int);
typedef void (*PFN_CopyMemblock)(int, int, int, int, int);

typedef void (*PFN_WriteMemblockByte)(int, int, int);
typedef int (*PFN_ReadMemblockByte)(int, int);
typedef void (*PFN_WriteMemblockWord)(int, int, int);
typedef int (*PFN_ReadMemblockWordLLL)(int, int);
typedef void (*PFN_WriteMemblockDWord)(int, int, DWORD);
typedef DWORD (*PFN_ReadMemblockDWord)(int, int);
typedef void (*PFN_WriteMemblockFloat)(int, int, float);
typedef DWORD (*PFN_ReadMemblockFloat)(int, int);

typedef const DBP_DiagnosticContext* (*PFN_GetLastDiagnosticContext)(void);
typedef void (*PFN_ClearLastDiagnosticContext)(void);

class MemblocksPluginFixture : public ::testing::Test {
protected:
    HMODULE hMod = nullptr;
    CRuntimeErrorHandler m_TestErrorHandler{0};
    GlobStruct m_TestGlob{};

    PFN_Constructor pConstructor = nullptr;
    PFN_Destructor pDestructor = nullptr;
    PFN_SetErrorHandler pSetErrorHandler = nullptr;
    PFN_PassCoreData pPassCoreData = nullptr;

    PFN_MakeMemblock pMakeMemblock = nullptr;
    PFN_DeleteMemblock pDeleteMemblock = nullptr;
    PFN_MemblockExist pMemblockExist = nullptr;
    PFN_GetMemblockSize pGetMemblockSize = nullptr;
    PFN_GetMemblockPtr pGetMemblockPtr = nullptr;
    PFN_CopyMemblock pCopyMemblock = nullptr;

    PFN_WriteMemblockByte pWriteMemblockByte = nullptr;
    PFN_ReadMemblockByte pReadMemblockByte = nullptr;
    PFN_WriteMemblockWord pWriteMemblockWord = nullptr;
    PFN_ReadMemblockWordLLL pReadMemblockWordLLL = nullptr;
    PFN_WriteMemblockDWord pWriteMemblockDWord = nullptr;
    PFN_ReadMemblockDWord pReadMemblockDWord = nullptr;
    PFN_WriteMemblockFloat pWriteMemblockFloat = nullptr;
    PFN_ReadMemblockFloat pReadMemblockFloat = nullptr;

    PFN_GetLastDiagnosticContext pGetLastDiagnosticContext = nullptr;
    PFN_ClearLastDiagnosticContext pClearLastDiagnosticContext = nullptr;

    void SetUp() override {
        hMod = LoadLibraryA("plugins/DBProMemblocksDebug.dll");
        if (!hMod) {
            hMod = LoadLibraryA("DBProMemblocksDebug.dll");
        }
        ASSERT_NE(hMod, nullptr) << "Failed to load DBProMemblocksDebug.dll";

        pConstructor = (PFN_Constructor)GetProcAddress(hMod, "?Constructor@@YAXXZ");
        pDestructor = (PFN_Destructor)GetProcAddress(hMod, "?Destructor@@YAXXZ");
        pSetErrorHandler = (PFN_SetErrorHandler)GetProcAddress(hMod, "?SetErrorHandler@@YAXPEAX@Z");
        pPassCoreData = (PFN_PassCoreData)GetProcAddress(hMod, "?PassCoreData@@YAXPEAX@Z");

        pMakeMemblock = (PFN_MakeMemblock)GetProcAddress(hMod, "?MakeMemblock@@YAXHH@Z");
        pDeleteMemblock = (PFN_DeleteMemblock)GetProcAddress(hMod, "?DeleteMemblock@@YAXH@Z");
        pMemblockExist = (PFN_MemblockExist)GetProcAddress(hMod, "?MemblockExist@@YAHH@Z");
        pGetMemblockSize = (PFN_GetMemblockSize)GetProcAddress(hMod, "?GetMemblockSize@@YAHH@Z");
        pGetMemblockPtr = (PFN_GetMemblockPtr)GetProcAddress(hMod, "?GetMemblockPtr@@YAPEADH@Z");
        pCopyMemblock = (PFN_CopyMemblock)GetProcAddress(hMod, "?CopyMemblock@@YAXHHHHH@Z");

        pWriteMemblockByte = (PFN_WriteMemblockByte)GetProcAddress(hMod, "?WriteMemblockByte@@YAXHHH@Z");
        pReadMemblockByte = (PFN_ReadMemblockByte)GetProcAddress(hMod, "?ReadMemblockByte@@YAHHH@Z");
        pWriteMemblockWord = (PFN_WriteMemblockWord)GetProcAddress(hMod, "?WriteMemblockWord@@YAXHHH@Z");
        pReadMemblockWordLLL = (PFN_ReadMemblockWordLLL)GetProcAddress(hMod, "?ReadMemblockWordLLL@@YAHHH@Z");
        pWriteMemblockDWord = (PFN_WriteMemblockDWord)GetProcAddress(hMod, "?WriteMemblockDWord@@YAXHHK@Z");
        pReadMemblockDWord = (PFN_ReadMemblockDWord)GetProcAddress(hMod, "?ReadMemblockDWord@@YAKHH@Z");
        pWriteMemblockFloat = (PFN_WriteMemblockFloat)GetProcAddress(hMod, "?WriteMemblockFloat@@YAXHHM@Z");
        pReadMemblockFloat = (PFN_ReadMemblockFloat)GetProcAddress(hMod, "?ReadMemblockFloat@@YAKHH@Z");

        pGetLastDiagnosticContext = (PFN_GetLastDiagnosticContext)GetProcAddress(hMod, "?GetLastDiagnosticContext@@YAPEBUDBP_DiagnosticContext@@XZ");
        pClearLastDiagnosticContext = (PFN_ClearLastDiagnosticContext)GetProcAddress(hMod, "?ClearLastDiagnosticContext@@YAXXZ");

        ASSERT_NE(pMakeMemblock, nullptr);
        ASSERT_NE(pDeleteMemblock, nullptr);
        ASSERT_NE(pMemblockExist, nullptr);
        ASSERT_NE(pGetMemblockSize, nullptr);
        ASSERT_NE(pGetMemblockPtr, nullptr);

        m_TestErrorHandler.dwErrorCode = 0;
        if (pSetErrorHandler) pSetErrorHandler(&m_TestErrorHandler);
        if (pPassCoreData) pPassCoreData(&m_TestGlob);
        if (pClearLastDiagnosticContext) pClearLastDiagnosticContext();
    }

    void TearDown() override {
        if (pDeleteMemblock && pMemblockExist) {
            for (int i = 1; i <= 257; ++i) {
                if (pMemblockExist(i)) {
                    pDeleteMemblock(i);
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

TEST_F(MemblocksPluginFixture, MemblockLifecycleAndAllocation) {
    const int kMbi = 1;
    const int kSize = 1024;

    EXPECT_EQ(pMemblockExist(kMbi), 0);

    pMakeMemblock(kMbi, kSize);
    EXPECT_EQ(pMemblockExist(kMbi), 1);
    EXPECT_EQ(pGetMemblockSize(kMbi), kSize);

    char* ptr = pGetMemblockPtr(kMbi);
    ASSERT_NE(ptr, nullptr);

    pDeleteMemblock(kMbi);
    EXPECT_EQ(pMemblockExist(kMbi), 0);
}

TEST_F(MemblocksPluginFixture, ByteOperationsAndBoundaryData) {
    const int kMbi = 1;
    pMakeMemblock(kMbi, 256);

    pWriteMemblockByte(kMbi, 0, 0x00);
    pWriteMemblockByte(kMbi, 1, 0x7F);
    pWriteMemblockByte(kMbi, 255, 0xFF);

    EXPECT_EQ(pReadMemblockByte(kMbi, 0), 0);
    EXPECT_EQ(pReadMemblockByte(kMbi, 1), 127);
    EXPECT_EQ(pReadMemblockByte(kMbi, 255), 255);

    pDeleteMemblock(kMbi);
}

TEST_F(MemblocksPluginFixture, WordOperations) {
    const int kMbi = 1;
    pMakeMemblock(kMbi, 64);

    pWriteMemblockWord(kMbi, 0, 0);
    pWriteMemblockWord(kMbi, 2, 12345);
    pWriteMemblockWord(kMbi, 4, 65535);

    EXPECT_EQ(pReadMemblockWordLLL(kMbi, 0), 0);
    EXPECT_EQ(pReadMemblockWordLLL(kMbi, 2), 12345);
    EXPECT_EQ(pReadMemblockWordLLL(kMbi, 4), 65535);

    pDeleteMemblock(kMbi);
}

TEST_F(MemblocksPluginFixture, DWordOperations) {
    const int kMbi = 1;
    pMakeMemblock(kMbi, 64);

    pWriteMemblockDWord(kMbi, 0, 0x00000000);
    pWriteMemblockDWord(kMbi, 4, 0x12345678);
    pWriteMemblockDWord(kMbi, 8, 0xFFFFFFFF);

    EXPECT_EQ(pReadMemblockDWord(kMbi, 0), 0x00000000U);
    EXPECT_EQ(pReadMemblockDWord(kMbi, 4), 0x12345678U);
    EXPECT_EQ(pReadMemblockDWord(kMbi, 8), 0xFFFFFFFFU);

    pDeleteMemblock(kMbi);
}

TEST_F(MemblocksPluginFixture, FloatOperationsAndPrecision) {
    const int kMbi = 1;
    pMakeMemblock(kMbi, 64);

    const float val1 = 0.0f;
    const float val2 = 3.14159265f;
    const float val3 = -999.5f;

    pWriteMemblockFloat(kMbi, 0, val1);
    pWriteMemblockFloat(kMbi, 4, val2);
    pWriteMemblockFloat(kMbi, 8, val3);

    DWORD raw1 = pReadMemblockFloat(kMbi, 0);
    DWORD raw2 = pReadMemblockFloat(kMbi, 4);
    DWORD raw3 = pReadMemblockFloat(kMbi, 8);

    float read1 = 0.0f, read2 = 0.0f, read3 = 0.0f;
    memcpy(&read1, &raw1, sizeof(float));
    memcpy(&read2, &raw2, sizeof(float));
    memcpy(&read3, &raw3, sizeof(float));

    EXPECT_FLOAT_EQ(read1, val1);
    EXPECT_FLOAT_EQ(read2, val2);
    EXPECT_FLOAT_EQ(read3, val3);

    pDeleteMemblock(kMbi);
}

TEST_F(MemblocksPluginFixture, CopyMemblockBetweenBlocks) {
    pMakeMemblock(1, 128);
    pMakeMemblock(2, 128);

    // Populate block 1 with pattern
    for (int i = 0; i < 32; ++i) {
        pWriteMemblockByte(1, 10 + i, 100 + i);
    }

    // Copy 32 bytes from block 1 (offset 10) to block 2 (offset 20)
    pCopyMemblock(1, 2, 10, 20, 32);

    // Verify block 2 receives pattern
    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(pReadMemblockByte(2, 20 + i), 100 + i);
    }

    pDeleteMemblock(1);
    pDeleteMemblock(2);
}

TEST_F(MemblocksPluginFixture, Diagnostics_DuplicateCreationTriggersError) {
    m_TestErrorHandler.dwErrorCode = 0;
    pMakeMemblock(1, 128);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, 0U);

    // Duplicate creation without deletion
    pMakeMemblock(1, 256);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKALREADYEXISTS));

    if (pGetLastDiagnosticContext) {
        const DBP_DiagnosticContext* ctx = pGetLastDiagnosticContext();
        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_MEMBLOCKALREADYEXISTS));
        EXPECT_STREQ(ctx->description, "Memblock already exists");
    }

    pDeleteMemblock(1);
}

TEST_F(MemblocksPluginFixture, Diagnostics_DeleteNonExistentTriggersError) {
    m_TestErrorHandler.dwErrorCode = 0;
    pDeleteMemblock(42);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKNOTEXIST));

    if (pGetLastDiagnosticContext) {
        const DBP_DiagnosticContext* ctx = pGetLastDiagnosticContext();
        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_MEMBLOCKNOTEXIST));
        EXPECT_STREQ(ctx->description, "Memblock does not exist");
    }
}

TEST_F(MemblocksPluginFixture, Diagnostics_IllegalIdRangeTriggersError) {
    m_TestErrorHandler.dwErrorCode = 0;
    pMakeMemblock(0, 64);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKRANGEILLEGAL));

    m_TestErrorHandler.dwErrorCode = 0;
    pMakeMemblock(-5, 64);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKRANGEILLEGAL));

    m_TestErrorHandler.dwErrorCode = 0;
    pMakeMemblock(258, 64);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKRANGEILLEGAL));

    if (pGetLastDiagnosticContext) {
        const DBP_DiagnosticContext* ctx = pGetLastDiagnosticContext();
        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_MEMBLOCKRANGEILLEGAL));
        EXPECT_STREQ(ctx->description, "Memblock number illegal");
    }
}

TEST_F(MemblocksPluginFixture, Diagnostics_InvalidSizeTriggersError) {
    m_TestErrorHandler.dwErrorCode = 0;
    pMakeMemblock(2, 0);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKSIZEINVALID));

    m_TestErrorHandler.dwErrorCode = 0;
    pMakeMemblock(2, -100);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKSIZEINVALID));

    if (pGetLastDiagnosticContext) {
        const DBP_DiagnosticContext* ctx = pGetLastDiagnosticContext();
        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_MEMBLOCKSIZEINVALID));
        EXPECT_STREQ(ctx->description, "Memblock size invalid");
    }
}

TEST_F(MemblocksPluginFixture, Diagnostics_OffsetOutsideRangeTriggersError) {
    pMakeMemblock(1, 32);

    m_TestErrorHandler.dwErrorCode = 0;
    pReadMemblockByte(1, 40); // Offset 40 > 32
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKOUTSIDERANGE));

    m_TestErrorHandler.dwErrorCode = 0;
    pWriteMemblockByte(1, 40, 1);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKOUTSIDERANGE));

    m_TestErrorHandler.dwErrorCode = 0;
    pReadMemblockByte(1, -1);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKOUTSIDERANGE));

    if (pGetLastDiagnosticContext) {
        const DBP_DiagnosticContext* ctx = pGetLastDiagnosticContext();
        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_MEMBLOCKOUTSIDERANGE));
        EXPECT_STREQ(ctx->description, "Memblock offset outside range");
    }

    pDeleteMemblock(1);
}

TEST_F(MemblocksPluginFixture, Diagnostics_ByteAndWordValueRangeValidation) {
    pMakeMemblock(1, 32);

    m_TestErrorHandler.dwErrorCode = 0;
    pWriteMemblockByte(1, 0, 300); // 300 > 255
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKNOTABYTE));

    m_TestErrorHandler.dwErrorCode = 0;
    pWriteMemblockByte(1, 0, -10);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKNOTABYTE));

    m_TestErrorHandler.dwErrorCode = 0;
    pWriteMemblockWord(1, 0, 70000); // 70000 > 65535
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKNOTAWORD));

    m_TestErrorHandler.dwErrorCode = 0;
    pWriteMemblockWord(1, 0, -1);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_MEMBLOCKNOTAWORD));

    pDeleteMemblock(1);
}
