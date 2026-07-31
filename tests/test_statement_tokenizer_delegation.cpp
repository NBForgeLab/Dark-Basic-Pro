#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/Statement.h"
#include "../DBProCompiler/DBPCompiler/Tokenizer.h"

TEST(StatementTokenizerDelegationTest, StatementHasTokenizerInstance) {
    CStatement stmt;
    CTokenizer& tokenizer = stmt.GetTokenizer();
    EXPECT_EQ(tokenizer.GetSourceBuffer(), nullptr);
    EXPECT_EQ(tokenizer.GetCurrentPosition(), 0u);
}

TEST(StatementTokenizerDelegationTest, TokenizerCanClassifyNamesFromStatement) {
    CStatement stmt;
    CTokenizer& tokenizer = stmt.GetTokenizer();
    int typeString = tokenizer.DetermineNameToken("strVar$");
    int typeFloat = tokenizer.DetermineNameToken("floatVar#");
    int typeInt = tokenizer.DetermineNameToken("intVar");

    EXPECT_EQ(typeString, 1);
    EXPECT_EQ(typeFloat, 2);
    EXPECT_EQ(typeInt, 3);
}

TEST(StatementTokenizerDelegationTest, TokenizerCanCheckReservedWords) {
    CStatement stmt;
    CTokenizer& tokenizer = stmt.GetTokenizer();
    EXPECT_TRUE(tokenizer.DetermineIfReservedWord("if"));
    EXPECT_TRUE(tokenizer.DetermineIfReservedWord("function"));
    EXPECT_FALSE(tokenizer.DetermineIfReservedWord("myCustomVar"));
}
