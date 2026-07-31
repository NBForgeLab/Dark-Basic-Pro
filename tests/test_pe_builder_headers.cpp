#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/PEBuilder.h"

TEST(PEBuilderHeadersTest, ValidatesPEHeaderRequirements) {
    CPEBuilder builder;
    EXPECT_TRUE(builder.ValidatePEHeaderRequirements(0x400000, 4096, 512));
    EXPECT_FALSE(builder.ValidatePEHeaderRequirements(0, 4096, 512));
    EXPECT_FALSE(builder.ValidatePEHeaderRequirements(0x400000, 0, 512));
}
