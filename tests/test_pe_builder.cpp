#include <gtest/gtest.h>
#include "../DBProCompiler/DBPCompiler/PEBuilder.h"

TEST(PEBuilderTest, InitialStateIsClean) {
    CPEBuilder builder;
    EXPECT_FALSE(builder.IsPrepared());
    EXPECT_EQ(builder.GetHeaderSize(), 0u);
}

TEST(PEBuilderTest, SectionAlignmentMath) {
    CPEBuilder builder;
    DWORD aligned512 = builder.CalculateAlignedSize(100, 512);
    DWORD aligned4096 = builder.CalculateAlignedSize(100, 4096);
    EXPECT_EQ(aligned512, 512u);
    EXPECT_EQ(aligned4096, 4096u);
}

TEST(PEBuilderTest, ResetClearsState) {
    CPEBuilder builder;
    builder.SetPrepared(true);
    EXPECT_TRUE(builder.IsPrepared());
    builder.Reset();
    EXPECT_FALSE(builder.IsPrepared());
}

TEST(PEBuilderTest, RuntimeDllIndexesMustFitTheLegacyDispatchTable) {
    EXPECT_TRUE(CPEBuilder::IsRuntimeDllIndex(0u));
    EXPECT_TRUE(CPEBuilder::IsRuntimeDllIndex(255u));
    EXPECT_FALSE(CPEBuilder::IsRuntimeDllIndex(256u));
    EXPECT_FALSE(CPEBuilder::IsRuntimeDllIndex(0xFFFF3DFFu));
}
