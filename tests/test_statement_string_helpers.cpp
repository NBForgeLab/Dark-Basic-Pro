#include <gtest/gtest.h>
#include <cstring>
#include <memory>
#include <windows.h>
#include "Statement.h"
#include "Str.h"

// Characterization pins for the global-free string helpers of CStatement
// (SeperateInitFromType / ContainsAssignmentOperator). These lock the
// observable behaviour a RAII refactor must preserve:
//   * SeperateInitFromType truncates the type in place at '=', returns a
//     heap char[] copy of the trimmed init value, or NULL when there is no
//     assignment (or a NULL input).
//   * ContainsAssignmentOperator reports whether the left-hand side of the
//     first '=' is a valid L-value.
// Both helpers touch no globals, so a stack CStatement is sufficient (no
// CompilerContext bootstrap needed). They are GREEN on the legacy
// new/new[]+SAFE_DELETE implementation and must stay GREEN once the
// internals move to stack CStr / std::make_unique / unique_ptr adoption.

TEST(StatementStringHelpersTest, SeperateInitFromTypeExtractsInitAndTruncatesType) {
    CStatement statement;
    char buf[] = "count = 42";
    LPSTR pInit = statement.SeperateInitFromType(buf);
    ASSERT_NE(pInit, nullptr);
    EXPECT_STREQ(pInit, "42");
    // Type side is truncated in place at '=' (trailing space before '=' kept)
    EXPECT_STREQ(buf, "count ");
    delete[] pInit;
}

TEST(StatementStringHelpersTest, SeperateInitFromTypeReturnsNullWhenNoAssignment) {
    CStatement statement;
    char buf[] = "integer";
    EXPECT_EQ(statement.SeperateInitFromType(buf), nullptr);
    EXPECT_STREQ(buf, "integer");
}

TEST(StatementStringHelpersTest, SeperateInitFromTypeReturnsNullForNullInput) {
    CStatement statement;
    EXPECT_EQ(statement.SeperateInitFromType(nullptr), nullptr);
}

TEST(StatementStringHelpersTest, ContainsAssignmentOperatorDetectsLValueAssignment) {
    CStatement statement;
    CStr expr("myvar=10");
    EXPECT_TRUE(statement.ContainsAssignmentOperator(&expr));
}

TEST(StatementStringHelpersTest, ContainsAssignmentOperatorFalseWithoutEquals) {
    CStatement statement;
    CStr expr("myvar");
    EXPECT_FALSE(statement.ContainsAssignmentOperator(&expr));
}

// Characterization pins for CStatement::SeperateValueFromArrayString - another
// global-free helper. On success it extracts the value inside the first
// (...) into a fresh heap char[] (*pArrValue), frees the caller-owned name
// buffer (allocated with new char[]) and replaces it with a fresh heap char[]
// holding just the trimmed name. The legacy body frees the incoming buffer
// with scalar SAFE_DELETE - an array-new/scalar-delete mismatch a RAII
// refactor must fix while preserving these observable results. The caller
// (DoDeclaration) owns both output buffers as unique_ptr<char[]>, so the pins
// allocate the input with new[] and release the outputs with delete[].

TEST(StatementStringHelpersTest, SeperateValueFromArrayStringExtractsValueAndName) {
    CStatement statement;
    LPSTR pName = new char[16];
    strcpy(pName, "myarr(5)");
    LPSTR pValue = nullptr;
    EXPECT_TRUE(statement.SeperateValueFromArrayString(&pName, &pValue, false));
    ASSERT_NE(pValue, nullptr);
    EXPECT_STREQ(pValue, "5");
    EXPECT_STREQ(pName, "myarr");
    delete[] pValue;
    delete[] pName;
}

TEST(StatementStringHelpersTest, SeperateValueFromArrayStringExtractsMultiCharValue) {
    CStatement statement;
    LPSTR pName = new char[16];
    strcpy(pName, "arr(42)");
    LPSTR pValue = nullptr;
    EXPECT_TRUE(statement.SeperateValueFromArrayString(&pName, &pValue, false));
    ASSERT_NE(pValue, nullptr);
    EXPECT_STREQ(pValue, "42");
    EXPECT_STREQ(pName, "arr");
    delete[] pValue;
    delete[] pName;
}

TEST(StatementStringHelpersTest, SeperateValueFromArrayStringReturnsFalseWithoutBracket) {
    CStatement statement;
    LPSTR pName = new char[16];
    strcpy(pName, "myarr");
    LPSTR pValue = nullptr;
    EXPECT_FALSE(statement.SeperateValueFromArrayString(&pName, &pValue, false));
    EXPECT_EQ(pValue, nullptr);
    EXPECT_STREQ(pName, "myarr");
    delete[] pName;
}
