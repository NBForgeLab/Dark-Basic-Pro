#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/Tokenizer.h"
#include "../DBProCompiler/DBPCompiler/Statement.h"

TEST(TokenizerKeywordsTest, ResolvesDarkBasicKeywordsToTokenEnumValues) {
    CTokenizer tokenizer;
    EXPECT_EQ(tokenizer.DetermineKeywordToken("DO"), static_cast<int>(Token::Do));
    EXPECT_EQ(tokenizer.DetermineKeywordToken("LOOP"), static_cast<int>(Token::Loop));
    EXPECT_EQ(tokenizer.DetermineKeywordToken("WHILE"), static_cast<int>(Token::While));
    EXPECT_EQ(tokenizer.DetermineKeywordToken("FOR"), static_cast<int>(Token::For));
    EXPECT_EQ(tokenizer.DetermineKeywordToken("NEXT"), static_cast<int>(Token::Next));
    EXPECT_EQ(tokenizer.DetermineKeywordToken("FUNCTION"), static_cast<int>(Token::UserFunction));
    EXPECT_EQ(tokenizer.DetermineKeywordToken("IF"), static_cast<int>(Token::If));
    EXPECT_EQ(tokenizer.DetermineKeywordToken("TYPE"), static_cast<int>(Token::Type));
    EXPECT_EQ(tokenizer.DetermineKeywordToken("GLOBAL"), static_cast<int>(Token::Global));
    EXPECT_EQ(tokenizer.DetermineKeywordToken("LOCAL"), static_cast<int>(Token::Local));
    EXPECT_EQ(tokenizer.DetermineKeywordToken("DIM"), static_cast<int>(Token::Dim));
}

TEST(TokenizerKeywordsTest, ReturnsZeroForNonKeyword) {
    CTokenizer tokenizer;
    EXPECT_EQ(tokenizer.DetermineKeywordToken("myCustomVariable"), 0);
}
