#include <gtest/gtest.h>

#include "ASMWriter.h"
#include "CodeGenerationSession.h"

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
