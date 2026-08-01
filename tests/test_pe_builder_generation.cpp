#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/PEBuilder.h"

TEST(PEBuilderGenerationTest, ResetClearsPreparedImageState) {
    CPEBuilder builder;
    builder.SetPrepared(true);
    builder.SetHeaderSize(4096U);

    builder.Reset();

    EXPECT_FALSE(builder.IsPrepared());
    EXPECT_EQ(builder.GetHeaderSize(), 0U);
}

TEST(PEBuilderGenerationTest, RejectsInvalidAlignmentRelationships) {
    CPEBuilder builder;
    EXPECT_FALSE(builder.ValidatePEHeaderRequirements(
        0x00400000U, 512U, 4096U));
    EXPECT_FALSE(builder.ValidatePEHeaderRequirements(
        0x00400000U, 4096U, 0U));
}
