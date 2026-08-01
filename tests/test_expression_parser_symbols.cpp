#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/ExpressionParser.h"

TEST(ExpressionParserSymbolsTest, DetectsPlusSymbolAndPriority) {
    CExpressionParser parser;
    DWORD dwMathType = 0;
    DWORD dwPriority = 0;
    DWORD dwSymbolWidth = 0;

    EXPECT_TRUE(parser.CheckForSymbol("+", 0, &dwMathType, &dwPriority, &dwSymbolWidth));
    EXPECT_GT(dwPriority, 0u);
    EXPECT_GT(dwSymbolWidth, 0u);
}

TEST(ExpressionParserSymbolsTest, DetectsWordOperatorsCaseInsensitively) {
    CExpressionParser parser;
    DWORD mathType = 0;
    DWORD priority = 0;
    DWORD width = 0;

    ASSERT_TRUE(parser.CheckForSymbol(
        " and ", 0, &mathType, &priority, &width));
    EXPECT_EQ(mathType, 41u);
    EXPECT_EQ(width, 5u);

    ASSERT_TRUE(parser.CheckForSymbol(
        " mod ", 0, &mathType, &priority, &width));
    EXPECT_EQ(mathType, 6u);
    EXPECT_EQ(width, 5u);
}
