#include <gtest/gtest.h>

#include "ASMWriter.h"
#include "CodeGenerationSession.h"
#include "DBMWriter.h"

TEST(CodeGenerationSessionTest, RejectsEmissionBeforeInitialization) {
    CASMWriter writer;
    CodeGenerationSession session(writer);

    const auto result = session.RequireInitialized("assignment emission");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, CodeGenerationErrorCode::NotInitialized);
}

TEST(CodeGenerationSessionTest, BeginsBackendExactlyOnce) {
    CASMWriter writer;
    CodeGenerationSession session(writer);

    ASSERT_TRUE(session.Begin().has_value());
    EXPECT_TRUE(session.RequireInitialized("assignment emission").has_value());
    const auto secondBegin = session.Begin();
    ASSERT_FALSE(secondBegin.has_value());
    EXPECT_EQ(secondBegin.error().code, CodeGenerationErrorCode::InvalidTransition);
}

TEST(CodeGenerationSessionTest, FinishesOnlyAfterInitialization) {
    CASMWriter writer;
    CodeGenerationSession session(writer);

    EXPECT_FALSE(session.Finish().has_value());
    ASSERT_TRUE(session.Begin().has_value());
    EXPECT_TRUE(session.Finish().has_value());
    EXPECT_EQ(session.state(), CodeGenerationState::Finished);
}

TEST(DBMWriterTest, AppendsOnlyContentAndCrLf) {
    CDBMWriter writer;
    writer.InitializeBufferForTests(32);
    CStr line("abc");

    ASSERT_TRUE(writer.OutputDBM(&line));
    EXPECT_EQ(writer.GetUsedBufferSizeForTests(), 5u);
}

#include "Error.h"

extern CError* g_pErrorReport;

TEST(CodeGenerationSessionTest, BackendRejectsMachineCodeBeforeInitialization) {
    g_pErrorReport = nullptr;
    CASMWriter writer;

    EXPECT_FALSE(writer.CreateASMMiddle(-1, 0x90, -1, nullptr));
    ASSERT_TRUE(writer.CreateASMHeader());
    EXPECT_TRUE(writer.CreateASMMiddle(-1, 0x90, -1, nullptr));
}
