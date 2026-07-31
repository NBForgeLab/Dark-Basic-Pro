#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/Tokenizer.h"

TEST(TokenizerTest, InitialStateIsEmpty) {
    CTokenizer tokenizer;
    EXPECT_EQ(tokenizer.GetSourceBuffer(), nullptr);
    EXPECT_EQ(tokenizer.GetCurrentPosition(), 0u);
}

TEST(TokenizerTest, SetSourceBufferInitializesPosition) {
    CTokenizer tokenizer;
    const char* source = "a = 5\r\nprint a\r\n";
    tokenizer.SetSourceBuffer(source);
    EXPECT_NE(tokenizer.GetSourceBuffer(), nullptr);
    EXPECT_EQ(tokenizer.GetCurrentPosition(), 0u);
}

TEST(TokenizerTest, SkipCommentsAndWhitespace) {
    CTokenizer tokenizer;
    const char* source = "   \t  rem this is a comment\r\n   a = 10";
    tokenizer.SetSourceBuffer(source);
    tokenizer.SkipAllComments();
    
    DWORD pos = tokenizer.GetCurrentPosition();
    EXPECT_GT(pos, 0u);
    EXPECT_EQ(source[pos], 'a');
}

TEST(TokenizerTest, DetermineNameTokenClassification) {
    CTokenizer tokenizer;
    const char* varName = "myVariable#";
    int tokenType = tokenizer.DetermineNameToken(varName);
    EXPECT_NE(tokenType, 0);
}

TEST(TokenizerTest, GetStringToEndOfLineExtractsRestOfLine) {
    CTokenizer tokenizer;
    const char* source = "print \"Hello World\"\r\nnext line";
    tokenizer.SetSourceBuffer(source);
    std::string line = tokenizer.GetStringToEndOfLine();
    EXPECT_EQ(line, "print \"Hello World\"");
}
