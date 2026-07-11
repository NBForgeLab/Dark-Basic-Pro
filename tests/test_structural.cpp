#include <gtest/gtest.h>
#include "DataType.h"
#include "PluginRegistry.h"

TEST(StructuralTest, DataTypeEnumCorrectness) {
    // Verify values correspond to standard DBPro structure values
    EXPECT_EQ(static_cast<DWORD>(DataType::Unknown), 0);
    EXPECT_EQ(static_cast<DWORD>(DataType::Array), 1);
    EXPECT_EQ(static_cast<DWORD>(DataType::Integer), 2);
    EXPECT_EQ(static_cast<DWORD>(DataType::String), 3);
    EXPECT_EQ(static_cast<DWORD>(DataType::Float), 4);
}

TEST(StructuralTest, PluginRegistryRegisterAndGet) {
    auto& registry = PluginRegistry::GetInstance();
    registry.Clear();

    // Create mock HINSTANCE handles
    HINSTANCE hMockGFX = reinterpret_cast<HINSTANCE>(0x1000);
    HINSTANCE hMockText = reinterpret_cast<HINSTANCE>(0x2000);

    // Register plugins
    registry.RegisterPlugin("DBProSetupDebug.dll", hMockGFX);
    registry.RegisterPlugin("GFX", hMockGFX);
    registry.RegisterPlugin("DBProTextDebug.dll", hMockText);
    registry.RegisterPlugin("Text", hMockText);

    // Assert lookups
    EXPECT_EQ(registry.GetPlugin("DBProSetupDebug.dll"), hMockGFX);
    EXPECT_EQ(registry.GetPlugin("GFX"), hMockGFX);
    EXPECT_EQ(registry.GetPlugin("DBProTextDebug.dll"), hMockText);
    EXPECT_EQ(registry.GetPlugin("Text"), hMockText);

    // Assert not found returns nullptr
    EXPECT_EQ(registry.GetPlugin("DBProSoundDebug.dll"), nullptr);
    EXPECT_EQ(registry.GetPlugin("Sound"), nullptr);

    // Clear and check
    registry.Clear();
    EXPECT_EQ(registry.GetPlugin("GFX"), nullptr);
}
