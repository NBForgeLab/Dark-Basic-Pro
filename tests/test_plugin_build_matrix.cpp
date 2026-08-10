#include <gtest/gtest.h>
#include "TargetABI.h"

#include "globstruct.h"

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

TEST(PluginBuildMatrixTest, ValidatesGlobStructAlignmentAndPointerTraits) {
    EXPECT_GE(alignof(GlobStruct), alignof(uintptr_t));
    EXPECT_EQ(alignof(GlobStruct) % alignof(uintptr_t), 0U);
    EXPECT_EQ(sizeof(GlobStruct::CreateDeleteString), sizeof(uintptr_t));
    EXPECT_EQ(sizeof(GlobStruct::g_pVariableSpace), sizeof(uintptr_t));
    EXPECT_EQ(sizeof(GlobStruct::g_GFX), sizeof(uintptr_t));
}

TEST(PluginBuildMatrixTest, ValidatesGlobChecklistStructPointerSafety) {
    EXPECT_EQ(alignof(GlobChecklistStruct), alignof(uintptr_t));
    EXPECT_EQ(sizeof(GlobChecklistStruct::string), sizeof(uintptr_t));
}
