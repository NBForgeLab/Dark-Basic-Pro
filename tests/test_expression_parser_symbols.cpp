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
