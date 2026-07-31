#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/PEBuilder.h"

TEST(PEBuilderGenerationTest, ReturnsFalseWhenFilenameIsNull) {
    CPEBuilder builder;
    EXPECT_FALSE(builder.BuildExecutable(nullptr));
}

TEST(PEBuilderGenerationTest, ReturnsFalseWhenFilenameIsEmpty) {
    CPEBuilder builder;
    EXPECT_FALSE(builder.BuildExecutable(""));
}

TEST(PEBuilderGenerationTest, ValidatesPackageBuilding) {
    CPEBuilder builder;
    EXPECT_FALSE(builder.BuildEXEPackage(nullptr, true, true));
    EXPECT_TRUE(builder.BuildEXEPackage("output.exe", true, true));
}
