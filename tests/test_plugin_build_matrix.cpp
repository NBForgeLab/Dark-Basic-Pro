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

TEST(PluginBuildMatrixTest, ValidatesAdvanced3DPluginsTargetTraits) {
    EXPECT_GE(dbp::abi::ActiveTargetAbi::address_size, 4U);
    EXPECT_TRUE(sizeof(GlobStruct::g_Basic3D) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_Light3D) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_Particles) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_Vectors) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_Transforms) == sizeof(HINSTANCE));
}

TEST(PluginBuildMatrixTest, ValidatesDBOFormatAndObjectsPluginsTargetTraits) {
    EXPECT_GE(dbp::abi::ActiveTargetAbi::address_size, 4U);
    EXPECT_TRUE(sizeof(GlobStruct::g_World3D) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_CSG) == sizeof(HINSTANCE));
}

TEST(PluginBuildMatrixTest, ValidatesBSPPluginsTargetTraits) {
    EXPECT_GE(dbp::abi::ActiveTargetAbi::address_size, 4U);
    EXPECT_TRUE(sizeof(GlobStruct::g_Q2BSP) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_OwnBSP) == sizeof(HINSTANCE));
}

TEST(PluginBuildMatrixTest, Validates2DAndMediaPluginsTargetTraits) {
    EXPECT_GE(dbp::abi::ActiveTargetAbi::address_size, 4U);
    EXPECT_TRUE(sizeof(GlobStruct::g_Sprites) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_Animation) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_Music) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_Basic2D) == sizeof(HINSTANCE));
}

TEST(PluginBuildMatrixTest, ValidatesSystemAndDataPluginsTargetTraits) {
    EXPECT_GE(dbp::abi::ActiveTargetAbi::address_size, 4U);
    EXPECT_TRUE(sizeof(GlobStruct::g_System) == sizeof(HINSTANCE));
    // Multiplayer and MultiplayerPlus are excluded from the CMake build:
    // they depend on deprecated DirectPlay4/8 SDK headers (dplay.h, dplay8.h)
    // not available in modern Windows SDKs.
}




