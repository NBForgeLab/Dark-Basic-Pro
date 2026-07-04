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
