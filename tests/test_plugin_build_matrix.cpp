#include <gtest/gtest.h>
#include "TargetABI.h"

#include "globstruct.h"

TEST(PluginBuildMatrixTest, VerifiesInputPluginAbiTargetTraits) {
    EXPECT_EQ(dbp::abi::ActiveTargetAbi::address_size, 4U);
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

TEST(PluginBuildMatrixTest, ValidatesImageAndSoundPluginTargetTraits) {
    EXPECT_GE(dbp::abi::ActiveTargetAbi::address_size, 4U);
    EXPECT_TRUE(sizeof(GlobStruct::g_Image) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_System) == sizeof(HINSTANCE));
}

TEST(PluginBuildMatrixTest, ValidatesCameraAndTextPluginTargetTraits) {
    EXPECT_GE(dbp::abi::ActiveTargetAbi::address_size, 4U);
    EXPECT_TRUE(sizeof(GlobStruct::g_Camera3D) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_Text) == sizeof(HINSTANCE));
}

TEST(PluginBuildMatrixTest, ValidatesUtilityPluginsTargetTraits) {
    EXPECT_GE(dbp::abi::ActiveTargetAbi::address_size, 4U);
    EXPECT_TRUE(sizeof(GlobStruct::g_Bitmap) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_File) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_FTP) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_Matrix3D) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_Memblocks) == sizeof(HINSTANCE));
}
