#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/PEBuilder.h"

TEST(PEBuilderHeadersTest, ValidatesPEHeaderRequirements) {
    CPEBuilder builder;
    EXPECT_TRUE(builder.ValidatePEHeaderRequirements(0x400000, 4096, 512));
    EXPECT_FALSE(builder.ValidatePEHeaderRequirements(0, 4096, 512));
    EXPECT_FALSE(builder.ValidatePEHeaderRequirements(0x400000, 0, 512));
}

TEST(PEBuilderHeadersTest, ValidatesPE64HeaderRequirements) {
    CPEBuilder builder;
    EXPECT_TRUE(builder.ValidatePE64HeaderRequirements(0x140000000ULL, 4096, 512));
    EXPECT_FALSE(builder.ValidatePE64HeaderRequirements(0ULL, 4096, 512));
    EXPECT_FALSE(builder.ValidatePE64HeaderRequirements(0x140000000ULL, 0, 512));
}

TEST(PEBuilderHeadersTest, IdentifiesPeMagicBitness) {
    EXPECT_EQ(CPEBuilder::GetPeMagic(false), IMAGE_NT_OPTIONAL_HDR32_MAGIC);
    EXPECT_EQ(CPEBuilder::GetPeMagic(true), IMAGE_NT_OPTIONAL_HDR64_MAGIC);
}
