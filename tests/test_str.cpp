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

TEST(CStrTest, SetTextSupportsItsOwnBufferAsSource) {
    CStr value("value");

    value.SetText(value.GetStr());

    EXPECT_STREQ(value.GetStr(), "value");
    EXPECT_EQ(value.Length(), 5);
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

// ============================================================
// Phase 1 TDD: Vector-based buffer and modern C++ features
// ============================================================

TEST(CStrTest, VectorBufferResizePreservesContent) {
    CStr s("Hello");
    EXPECT_EQ(s.Length(), 5);

    // Enlarge should preserve existing content
    s.Enlarge(100);
    EXPECT_STREQ(s.GetStr(), "Hello");
    EXPECT_EQ(s.Length(), 5);

    // Adding text after enlarge should work correctly
    s.AddText(" World");
    EXPECT_STREQ(s.GetStr(), "Hello World");
    EXPECT_EQ(s.Length(), 11);
}

TEST(CStrTest, VectorBufferAppendGrowsAutomatically) {
    CStr s("");
    // Append many characters to force multiple resizes
    for (int i = 0; i < 1000; ++i) {
        s.AddChar('A');
    }
    EXPECT_EQ(s.Length(), 1000);
    // Verify all chars are 'A'
    for (DWORD i = 0; i < s.Length(); ++i) {
        EXPECT_EQ(s.GetChar(i), 'A');
    }
}

TEST(CStrTest, MoveConstructorTransfersOwnership) {
    CStr s1("Move Me");
    EXPECT_EQ(s1.Length(), 7);

    CStr s2(std::move(s1));
    EXPECT_STREQ(s2.GetStr(), "Move Me");
    EXPECT_EQ(s2.Length(), 7);
    // Moved-from object should be in valid empty state
    EXPECT_EQ(s1.Length(), 0);
    EXPECT_STREQ(s1.GetStr(), "");
}

TEST(CStrTest, MoveAssignmentTransfersOwnership) {
    CStr s1("Source");
    CStr s2("Target");

    s2 = std::move(s1);
    EXPECT_STREQ(s2.GetStr(), "Source");
    EXPECT_EQ(s2.Length(), 6);
    // Moved-from object should be in valid empty state
    EXPECT_EQ(s1.Length(), 0);
    EXPECT_STREQ(s1.GetStr(), "");
}

TEST(CStrTest, StringViewAccessor) {
    CStr s("Hello View");
    std::string_view view = s.View();
    EXPECT_EQ(view, "Hello View");
    EXPECT_EQ(view.size(), 10);

    // View updates after mutation
    s.AddText("!");
    std::string_view view2 = s.View();
    EXPECT_EQ(view2, "Hello View!");
    EXPECT_EQ(view2.size(), 11);
}

TEST(CStrTest, InsertTextPrependsCorrectly) {
    CStr s("World");
    s.InsertText("Hello ");
    EXPECT_STREQ(s.GetStr(), "Hello World");
    EXPECT_EQ(s.Length(), 11);
}

TEST(CStrTest, SizeConstructorCreatesEmptyBufferOfCapacity) {
    CStr s((DWORD)256);
    EXPECT_EQ(s.Length(), 0);
    EXPECT_STREQ(s.GetStr(), "");

    // Should be able to set text up to the pre-allocated size without issue
    s.SetText("Pre-allocated buffer test");
    EXPECT_STREQ(s.GetStr(), "Pre-allocated buffer test");
}

TEST(CStrTest, NumericTextConversions) {
    CStr s;
    s.SetNumericText(42);
    EXPECT_STREQ(s.GetStr(), "42");

    s.SetNumericText(0);
    EXPECT_STREQ(s.GetStr(), "0");

    s.SetUnsignedNumericText(4294967295U);
    EXPECT_STREQ(s.GetStr(), "4294967295");

    CStr s2;
    s2.SetDWORDNumericText(12345);
    EXPECT_STREQ(s2.GetStr(), "12345");
}

TEST(CStrTest, DestructorDoesNotLeak) {
    // This test verifies RAII - no manual cleanup needed
    // Under a memory sanitizer, this would catch leaks
    for (int i = 0; i < 100; ++i) {
        CStr s("Leak test iteration");
        s.AddText(" - some more data to allocate");
        s.Enlarge(500);
        s.SetText("Reset");
    }
    // If we get here without a crash/leak, RAII is working
    SUCCEED();
}

TEST(CStrTest, EmptyStringOperations) {
    CStr empty;
    EXPECT_EQ(empty.Length(), 0);
    EXPECT_STREQ(empty.GetStr(), "");
    EXPECT_EQ(empty.View(), "");
    EXPECT_EQ(empty.GetValue(), 0.0);
    // Note: empty string is vacuously "numeric" (no chars fail the check)
    EXPECT_TRUE(empty.IsTextNumericValue());
}
