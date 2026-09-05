//
// test_error_diagnostics.cpp - Unit Tests for Modernized CError & Diagnostics
//

#include <gtest/gtest.h>
#include <windows.h>
#include <thread>
#include <vector>
#include <string>
#include "cruntimeerrors.h"
#include "cerror.h"

namespace {

class ErrorDiagnosticsTest : public ::testing::Test {
protected:
    CRuntimeErrorHandler m_TestErrorHandler{0};
    LPTOP_LEVEL_EXCEPTION_FILTER m_prevFilter{nullptr};

    void SetUp() override {
        m_TestErrorHandler.dwErrorCode = 0;
        g_pErrorHandler = &m_TestErrorHandler;
        ClearLastDiagnosticContext();
        m_prevFilter = SetUnhandledExceptionFilter(nullptr);
        SetUnhandledExceptionFilter(m_prevFilter);
    }

    void TearDown() override {
        SetUnhandledExceptionFilter(m_prevFilter);
        ClearLastDiagnosticContext();
        g_pErrorHandler = nullptr;
    }
};

} // namespace

TEST_F(ErrorDiagnosticsTest, DescriptionLookupMapsKnownErrorCodes) {
    EXPECT_STREQ(GetRuntimeErrorDescription(RUNTIMEERROR_NOTENOUGHMEMORY), "Not enough memory");
    EXPECT_STREQ(GetRuntimeErrorDescription(RUNTIMEERROR_FILENOTEXIST), "File does not exist");
    EXPECT_STREQ(GetRuntimeErrorDescription(RUNTIMEERROR_DIVIDEBYZERO), "Division by zero");
    EXPECT_STREQ(GetRuntimeErrorDescription(RUNTIMEERROR_MEMBLOCKRANGEILLEGAL), "Memblock number illegal");
    EXPECT_STREQ(GetRuntimeErrorDescription(RUNTIMEERROR_MEMBLOCKALREADYEXISTS), "Memblock already exists");
    EXPECT_STREQ(GetRuntimeErrorDescription(RUNTIMEERROR_MEMBLOCKOUTSIDERANGE), "Memblock offset outside range");
    EXPECT_STREQ(GetRuntimeErrorDescription(RUNTIMEERROR_CANNOTSCANCURRENTDIR), "Cannot scan current directory");
    EXPECT_STREQ(GetRuntimeErrorDescription(RUNTIMEERROR_FILEALREADYOPEN), "File already open");
    EXPECT_STREQ(GetRuntimeErrorDescription(RUNTIMEERROR_FILENUMBERINVALID), "File number invalid");
    EXPECT_STREQ(GetRuntimeErrorDescription(RUNTIMEERROR_B3DMODELNOTEXISTS), "3D object does not exist");
}

TEST_F(ErrorDiagnosticsTest, DescriptionLookupReturnsFallbackForUnknownCodes) {
    EXPECT_STREQ(GetRuntimeErrorDescription(0xDEADBEEF), "Unknown runtime error");
    EXPECT_STREQ(GetRuntimeErrorDescription(999999), "Unknown runtime error");
}

TEST_F(ErrorDiagnosticsTest, RunTimeErrorSetsCodeAndRecordsContext) {
    RunTimeError(RUNTIMEERROR_FILENOTEXIST);

    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_FILENOTEXIST));

    const DBP_DiagnosticContext* ctx = GetLastDiagnosticContext();
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_FILENOTEXIST));
    EXPECT_EQ(ctx->threadId, GetCurrentThreadId());
    EXPECT_GT(ctx->timestampUs, 0ULL);
    EXPECT_STREQ(ctx->description, "File does not exist");
    EXPECT_STREQ(ctx->clue, "");
}

TEST_F(ErrorDiagnosticsTest, RunTimeErrorWithClueRecordsContextDetail) {
    const char* testClue = "C:\\Games\\FPSCreator\\Files\\entitybank\\missing_weapon.fpe";
    RunTimeError(RUNTIMEERROR_FILENOTEXIST, testClue);

    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_FILENOTEXIST));

    const DBP_DiagnosticContext* ctx = GetLastDiagnosticContext();
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_FILENOTEXIST));
    EXPECT_STREQ(ctx->clue, testClue);
    EXPECT_STREQ(ctx->description, "File does not exist");
}

TEST_F(ErrorDiagnosticsTest, RunTimeErrorExCapturesFunctionSource) {
    const char* clue = "Memblock size -100 is negative";
    const char* fn = "MakeMemblock";
    RunTimeErrorEx(RUNTIMEERROR_MEMBLOCKSIZEINVALID, clue, fn);

    const DBP_DiagnosticContext* ctx = GetLastDiagnosticContext();
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_MEMBLOCKSIZEINVALID));
    EXPECT_STREQ(ctx->clue, clue);
    EXPECT_STREQ(ctx->sourceFunction, fn);
    EXPECT_STREQ(ctx->description, "Memblock size invalid");
}

TEST_F(ErrorDiagnosticsTest, ClearLastDiagnosticContextResetsState) {
    RunTimeError(RUNTIMEERROR_MEMBLOCKRANGEILLEGAL, "Invalid ID 999");
    const DBP_DiagnosticContext* ctx = GetLastDiagnosticContext();
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_MEMBLOCKRANGEILLEGAL));

    ClearLastDiagnosticContext();
    ctx = GetLastDiagnosticContext();
    EXPECT_EQ(ctx->errorCode, 0U);
    EXPECT_EQ(ctx->description[0], 0);
    EXPECT_EQ(ctx->clue[0], 0);
    EXPECT_EQ(ctx->sourceFunction[0], 0);
}

TEST_F(ErrorDiagnosticsTest, WarningsDoNotMutateActiveErrorCode) {
    m_TestErrorHandler.dwErrorCode = 0;
    RunTimeWarning(RUNTIMEERROR_CANNOTOPENFILEFORREADING);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, 0U);

    RunTimeSoftWarning(RUNTIMEERROR_CANNOTOPENFILEFORWRITING);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, 0U);
}

TEST_F(ErrorDiagnosticsTest, NullErrorHandlerSafety) {
    g_pErrorHandler = nullptr;
    EXPECT_NO_THROW({
        RunTimeError(RUNTIMEERROR_GENERICERROR);
        RunTimeError(RUNTIMEERROR_FILENOTEXIST, "test_file.txt");
        RunTimeWarning(RUNTIMEERROR_CANNOTOPENFILEFORREADING);
        RunTimeSoftWarning(RUNTIMEERROR_CANNOTOPENFILEFORWRITING);
    });
}

TEST_F(ErrorDiagnosticsTest, ConcurrentThreadsDiagnosticsSafety) {
    constexpr int kNumThreads = 8;
    constexpr int kIterations = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([t]() {
            for (int i = 0; i < kIterations; ++i) {
                char clue[64];
                snprintf(clue, sizeof(clue), "Thread %d iteration %d", t, i);
                RunTimeError(RUNTIMEERROR_NOTENOUGHMEMORY, clue);
                const DBP_DiagnosticContext* ctx = GetLastDiagnosticContext();
                EXPECT_NE(ctx, nullptr);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    const DBP_DiagnosticContext* ctx = GetLastDiagnosticContext();
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_NOTENOUGHMEMORY));
}

TEST_F(ErrorDiagnosticsTest, RunTimeErrorExCapturesSourceFileAndLine) {
    const char* clue = "File bounds check failed";
    const char* fn = "OpenFileDirect";
    const char* file = "FileCore.cpp";
    const uint32_t line = 142;

    RunTimeErrorEx(RUNTIMEERROR_CANNOTOPENFILEFORREADING, clue, fn, file, line);

    const DBP_DiagnosticContext* ctx = GetLastDiagnosticContext();
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_CANNOTOPENFILEFORREADING));
    EXPECT_STREQ(ctx->clue, clue);
    EXPECT_STREQ(ctx->sourceFunction, fn);
    EXPECT_STREQ(ctx->sourceFile, file);
    EXPECT_EQ(ctx->sourceLine, line);
    EXPECT_STREQ(ctx->description, "Cannot open file for reading");
}

TEST_F(ErrorDiagnosticsTest, DbpRecordErrorMacroCapturesSourceLocation) {
    const char* clue = "Database entry corrupt";
    const int expectedLine = __LINE__ + 1;
    DBP_RECORD_ERROR(RUNTIMEERROR_INVALIDFILE, clue);

    const DBP_DiagnosticContext* ctx = GetLastDiagnosticContext();
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_INVALIDFILE));
    EXPECT_STREQ(ctx->clue, clue);
    EXPECT_STRNE(ctx->sourceFunction, "");
    EXPECT_NE(strstr(ctx->sourceFile, "test_error_diagnostics.cpp"), nullptr);
    EXPECT_EQ(ctx->sourceLine, static_cast<uint32_t>(expectedLine));
    EXPECT_STREQ(ctx->description, "Invalid file");
}

TEST_F(ErrorDiagnosticsTest, DbpHandlePluginExceptionLogsAndHandlesSafely) {
    LONG resNull = DBP_HandlePluginException(nullptr, "NullTest");
    EXPECT_EQ(resNull, EXCEPTION_CONTINUE_SEARCH);

    EXCEPTION_RECORD record{};
    record.ExceptionCode = static_cast<DWORD>(0xC0000005);
    record.ExceptionAddress = reinterpret_cast<void*>(&GetRuntimeErrorDescription);

    CONTEXT contextRecord{};
    EXCEPTION_POINTERS exInfo{};
    exInfo.ExceptionRecord = &record;
    exInfo.ContextRecord = &contextRecord;

    LONG res = DBP_HandlePluginException(&exInfo, "AccessViolationSimulated");
    EXPECT_EQ(res, EXCEPTION_EXECUTE_HANDLER);

    const DBP_DiagnosticContext* ctx = GetLastDiagnosticContext();
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->errorCode, static_cast<uint32_t>(RUNTIMEERROR_GENERICERROR));
    EXPECT_NE(strstr(ctx->clue, "0xC0000005"), nullptr);
    EXPECT_NE(strstr(ctx->clue, "ACCESS_VIOLATION"), nullptr);
    EXPECT_STREQ(ctx->sourceFunction, "AccessViolationSimulated");
    EXPECT_NE(strstr(ctx->sourceFile, "dbp_tests"), nullptr);
    EXPECT_EQ(m_TestErrorHandler.dwErrorCode, static_cast<DWORD>(RUNTIMEERROR_GENERICERROR));
}

TEST_F(ErrorDiagnosticsTest, DbpUnhandledExceptionFilterGeneratesDmpAndTxtReportsAndCleansUp) {
    DBP_RECORD_ERROR(RUNTIMEERROR_NOTENOUGHMEMORY, "Allocation failed in mesh builder");

    EXCEPTION_RECORD record{};
    record.ExceptionCode = static_cast<DWORD>(0xC0000005);
    record.ExceptionAddress = reinterpret_cast<void*>(&GetRuntimeErrorDescription);

    CONTEXT contextRecord{};
    contextRecord.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&contextRecord);

    EXCEPTION_POINTERS exInfo{};
    exInfo.ExceptionRecord = &record;
    exInfo.ContextRecord = &contextRecord;

    LONG res = DBP_UnhandledExceptionFilter(&exInfo);
    EXPECT_EQ(res, EXCEPTION_EXECUTE_HANDLER);

    WIN32_FIND_DATAA fd{};
    HANDLE hFind = FindFirstFileA("*_Crash_*.txt", &fd);
    std::vector<std::string> txtFiles;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            txtFiles.push_back(fd.cFileName);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    HANDLE hFindDmp = FindFirstFileA("*_Crash_*.dmp", &fd);
    std::vector<std::string> dmpFiles;
    if (hFindDmp != INVALID_HANDLE_VALUE) {
        do {
            dmpFiles.push_back(fd.cFileName);
        } while (FindNextFileA(hFindDmp, &fd));
        FindClose(hFindDmp);
    }

    EXPECT_FALSE(txtFiles.empty());
    EXPECT_FALSE(dmpFiles.empty());

    for (const auto& file : txtFiles) {
        FILE* fp = nullptr;
        fopen_s(&fp, file.c_str(), "r");
        if (fp) {
            char buffer[4096] = {};
            fread(buffer, 1, sizeof(buffer) - 1, fp);
            fclose(fp);

            EXPECT_NE(strstr(buffer, "0xC0000005"), nullptr);
            EXPECT_NE(strstr(buffer, "ACCESS_VIOLATION"), nullptr);
            EXPECT_NE(strstr(buffer, "Allocation failed in mesh builder"), nullptr);
            EXPECT_NE(strstr(buffer, "Not enough memory"), nullptr);
            EXPECT_NE(strstr(buffer, "MiniDump File:"), nullptr);
            EXPECT_NE(strstr(buffer, "MiniDump Written:  yes"), nullptr);
            EXPECT_NE(strstr(buffer, "Source Line:"), nullptr);
            EXPECT_NE(strstr(buffer, "Source File:"), nullptr);
            EXPECT_NE(strstr(buffer, "Source Function:"), nullptr);
        }
        DeleteFileA(file.c_str());
    }

    for (const auto& file : dmpFiles) {
        DeleteFileA(file.c_str());
    }

    EXPECT_EQ(FindFirstFileA("*_Crash_*.txt", &fd), INVALID_HANDLE_VALUE);
    EXPECT_EQ(FindFirstFileA("*_Crash_*.dmp", &fd), INVALID_HANDLE_VALUE);
}

TEST_F(ErrorDiagnosticsTest, DbpUnhandledExceptionFilterWithoutPriorContextOutputsDefaultFieldsAndCleansUp) {
    ClearLastDiagnosticContext();

    EXCEPTION_RECORD record{};
    record.ExceptionCode = static_cast<DWORD>(0xC0000005);
    record.ExceptionAddress = reinterpret_cast<void*>(&GetRuntimeErrorDescription);

    CONTEXT contextRecord{};
    contextRecord.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&contextRecord);

    EXCEPTION_POINTERS exInfo{};
    exInfo.ExceptionRecord = &record;
    exInfo.ContextRecord = &contextRecord;

    LONG res = DBP_UnhandledExceptionFilter(&exInfo);
    EXPECT_EQ(res, EXCEPTION_EXECUTE_HANDLER);

    WIN32_FIND_DATAA fd{};
    HANDLE hFind = FindFirstFileA("*_Crash_*.txt", &fd);
    std::vector<std::string> txtFiles;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            txtFiles.push_back(fd.cFileName);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    HANDLE hFindDmp = FindFirstFileA("*_Crash_*.dmp", &fd);
    std::vector<std::string> dmpFiles;
    if (hFindDmp != INVALID_HANDLE_VALUE) {
        do {
            dmpFiles.push_back(fd.cFileName);
        } while (FindNextFileA(hFindDmp, &fd));
        FindClose(hFindDmp);
    }

    EXPECT_FALSE(txtFiles.empty());
    EXPECT_FALSE(dmpFiles.empty());

    for (const auto& file : txtFiles) {
        FILE* fp = nullptr;
        fopen_s(&fp, file.c_str(), "r");
        if (fp) {
            char buffer[4096] = {};
            fread(buffer, 1, sizeof(buffer) - 1, fp);
            fclose(fp);

            EXPECT_NE(strstr(buffer, "0xC0000005"), nullptr);
            EXPECT_NE(strstr(buffer, "ACCESS_VIOLATION"), nullptr);
            EXPECT_NE(strstr(buffer, "Source Function:   None"), nullptr);
            EXPECT_NE(strstr(buffer, "Source File:       None"), nullptr);
            EXPECT_NE(strstr(buffer, "Source Line:       0"), nullptr);
            EXPECT_NE(strstr(buffer, "No prior runtime error recorded in diagnostic context."), nullptr);
            EXPECT_NE(strstr(buffer, "MiniDump Written:  yes"), nullptr);
        }
        DeleteFileA(file.c_str());
    }

    for (const auto& file : dmpFiles) {
        DeleteFileA(file.c_str());
    }

    EXPECT_EQ(FindFirstFileA("*_Crash_*.txt", &fd), INVALID_HANDLE_VALUE);
    EXPECT_EQ(FindFirstFileA("*_Crash_*.dmp", &fd), INVALID_HANDLE_VALUE);
}

TEST_F(ErrorDiagnosticsTest, DbpInstallCrashHandlerRegistersFilter) {
    EXPECT_NO_THROW({
        DBP_InstallCrashHandler();
    });
}

