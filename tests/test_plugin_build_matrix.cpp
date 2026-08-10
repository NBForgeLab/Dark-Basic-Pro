#include <gtest/gtest.h>
#include "TargetABI.h"

TEST(PluginBuildMatrixTest, VerifiesInputPluginAbiTargetTraits) {
    EXPECT_GE(dbp::abi::ActiveTargetAbi::address_size, 4U);
    #if defined(_WIN64) || defined(__x86_64__)
    EXPECT_EQ(dbp::abi::ActiveTargetAbi::address_size, 8U);
    #else
    EXPECT_EQ(dbp::abi::ActiveTargetAbi::address_size, 4U);
    #endif
}

TEST(PluginBuildMatrixTest, ValidatesInputPluginExportsStructSize) {
    // Structure size requirement for standard DBP DLL plugin parameters
    struct PluginPassCoreData {
        void* pErrorPtr;
        void* pCorePtr;
    };
    EXPECT_GE(sizeof(PluginPassCoreData), sizeof(void*) * 2);
}
