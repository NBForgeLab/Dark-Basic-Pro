#include <gtest/gtest.h>
#include "TextConvert.h"

TEST(UnicodeTest, UTF8ToUTF16AndBack) {
    // 1. Standard ASCII
    std::string asciiStr = "Hello World!";
    std::wstring asciiWStr = TextConvert::UTF8ToUTF16(asciiStr);
    EXPECT_EQ(asciiWStr, L"Hello World!");
    EXPECT_EQ(TextConvert::UTF16ToUTF8(asciiWStr), asciiStr);

    // 2. Arabic Text (Unicode UTF-8 / UTF-16)
    std::string arabicStr = "مرحباً بك في DarkBasic";
    std::wstring arabicWStr = TextConvert::UTF8ToUTF16(arabicStr);
    
    // Arabic characters should compile to wide characters
    // 'م' is 0x0645 in Unicode
    EXPECT_EQ(arabicWStr[0], 0x0645);
    
    // Convert back and verify identity
    EXPECT_EQ(TextConvert::UTF16ToUTF8(arabicWStr), arabicStr);
}

TEST(UnicodeTest, EmptyStrings) {
    EXPECT_EQ(TextConvert::UTF8ToUTF16(""), L"");
    EXPECT_EQ(TextConvert::UTF16ToUTF8(L""), "");
}
