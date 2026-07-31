#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/ExpressionParser.h"

TEST(ExpressionParserTest, ValidatesSimpleNumberExpression) {
    CExpressionParser parser;
    EXPECT_TRUE(parser.IsNumericLiteral("12345"));
    EXPECT_FALSE(parser.IsNumericLiteral("abc123"));
}
