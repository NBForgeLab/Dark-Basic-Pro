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

#if !defined(_WIN64) && !defined(__x86_64__)
extern "C" DWORD __cdecl asm_dynamic_call(void* func, const DWORD* args, int argc);

static DWORD __stdcall dummy_stdcall(DWORD a, DWORD b) {
    return a * 10 + b;
}

static DWORD __cdecl dummy_cdecl(DWORD a, DWORD b, DWORD c) {
    return a + b + c;
}
#endif

TEST(DynamicCallTest, AssemblyCallParity) {
#if defined(_WIN64) || defined(__x86_64__)
    GTEST_SKIP() << "asm_dynamic_call is a 32-bit x86 legacy assembly test.";
#else
    DWORD args_stdcall[] = { 5, 8 };
    DWORD res_stdcall = asm_dynamic_call((void*)&dummy_stdcall, args_stdcall, 2);
    EXPECT_EQ(res_stdcall, 58);

    DWORD args_cdecl[] = { 10, 20, 30 };
    DWORD res_cdecl = asm_dynamic_call((void*)&dummy_cdecl, args_cdecl, 3);
    EXPECT_EQ(res_cdecl, 60);
#endif
}
