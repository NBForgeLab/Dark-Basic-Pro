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

    void SetUp() override {
        m_TestErrorHandler.dwErrorCode = 0;
        g_pErrorHandler = &m_TestErrorHandler;
        ClearLastDiagnosticContext();
    }

    void TearDown() override {
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
