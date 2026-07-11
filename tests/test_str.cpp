#include <gtest/gtest.h>
#include "Str.h"

TEST(CStrTest, BasicOperationsAndMutators) {
    // 1. Default constructor
    CStr s1;
    EXPECT_STREQ(s1.GetStr(), "");
    EXPECT_EQ(s1.Length(), 0);

    // 2. Value constructor
    CStr s2("Hello World");
    EXPECT_STREQ(s2.GetStr(), "Hello World");
    EXPECT_EQ(s2.Length(), 11);

    // 3. Mutator SetText
    s1.SetText("DarkBasic");
    EXPECT_STREQ(s1.GetStr(), "DarkBasic");
    EXPECT_EQ(s1.Length(), 9);

    // 4. Mutator AddText
    s1.AddText(" Pro");
    EXPECT_STREQ(s1.GetStr(), "DarkBasic Pro");
    EXPECT_EQ(s1.Length(), 13);

    // 5. Mutator AddChar
    s1.AddChar('!');
    EXPECT_STREQ(s1.GetStr(), "DarkBasic Pro!");
    EXPECT_EQ(s1.Length(), 14);

    // 6. Operation MakeUpper
    s1.MakeUpper();
    EXPECT_STREQ(s1.GetStr(), "DARKBASIC PRO!");
}

TEST(CStrTest, ParsingAndTypeChecks) {
    // Integer only value check
    CStr sInt("12345");
    EXPECT_TRUE(sInt.IsTextIntegerOnlyValue());
    EXPECT_TRUE(sInt.IsTextNumericValue());

    CStr sNegInt("-987");
    EXPECT_TRUE(sNegInt.IsTextIntegerOnlyValue());
    EXPECT_TRUE(sNegInt.IsTextNumericValue());

    // Float numeric check
    CStr sFloat("12.34");
    EXPECT_FALSE(sFloat.IsTextIntegerOnlyValue());
    EXPECT_TRUE(sFloat.IsTextNumericValue());

    // Hex check
    CStr sHex("0xAF12");
    EXPECT_TRUE(sHex.IsTextHexValue());

    // Non-numeric check
    CStr sAlpha("12A34");
    EXPECT_FALSE(sAlpha.IsTextIntegerOnlyValue());
    EXPECT_FALSE(sAlpha.IsTextNumericValue());
}

TEST(CStrTest, TrimmingAndFormatting) {
    // Speech marks
    CStr sSpeech("\"Hello\"");
    sSpeech.EatSpeechMarks();
    EXPECT_STREQ(sSpeech.GetStr(), "Hello");

    // Path extraction
    CStr sPath("C:\\MyDir\\MySubDir\\MyFile.dba");
    sPath.TrimToPathOnly();
    EXPECT_STREQ(sPath.GetStr(), "C:\\MyDir\\MySubDir\\");
}

TEST(CStrTest, LengthSynchronizationAfterMutations) {
    CStr testStr("  hello world   ");
    EXPECT_EQ(testStr.Length(), 16);

    // Perform trim/eat operations
    testStr.EatTrailingEdgeSpacesandTabs();
    EXPECT_EQ(testStr.Length(), 13);
    EXPECT_STREQ(testStr.GetStr(), "  hello world");

    DWORD chopped = 0;
    testStr.EatEdgeSpacesandTabs(&chopped);
    EXPECT_EQ(testStr.Length(), 11);
    EXPECT_STREQ(testStr.GetStr(), "hello world");
}
