#include <gtest/gtest.h>
#include "VFSHooks.h"
#include "MemoryPE.h"
#include <windows.h>
#include <fstream>

TEST(VFSRegistryTest, RegisterAndLookup) {
    VFSRegistry::Clear();
    std::string testData = "VFS Test Content";
    VFSRegistry::Register("test.txt", testData.c_str(), testData.size());

    EXPECT_TRUE(VFSRegistry::Exists("test.txt"));
    EXPECT_FALSE(VFSRegistry::Exists("nonexistent.txt"));

    const VFSFile* f = VFSRegistry::Get("test.txt");
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->size, testData.size());
    EXPECT_EQ(std::string(f->dataPtr, f->size), testData);
    
    VFSRegistry::Clear();
    EXPECT_FALSE(VFSRegistry::Exists("test.txt"));
}

TEST(VFSHooksTest, InterceptFileOperations) {
    VFSRegistry::Clear();
    std::string testData = "In-memory file data";
    VFSRegistry::Register("virtual_file.txt", testData.c_str(), testData.size());

    // Initialize hooks
    ASSERT_TRUE(VFSHooks::Initialize());
    EXPECT_TRUE(VFSHooks::IsHookActive());

    // Attempt to open virtual file using VFS Hook directly
    HANDLE hFile = Hook_CreateFileA("virtual_file.txt", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    ASSERT_NE(hFile, INVALID_HANDLE_VALUE);

    // Verify size
    DWORD size = Hook_GetFileSize(hFile, NULL);
    EXPECT_EQ(size, testData.size());

    // Verify reading
    char buffer[100] = {0};
    DWORD bytesRead = 0;
    BOOL readRes = Hook_ReadFile(hFile, buffer, testData.size(), &bytesRead, NULL);
    EXPECT_TRUE(readRes);
    EXPECT_EQ(bytesRead, testData.size());
    EXPECT_STREQ(buffer, testData.c_str());

    // Close handle
    Hook_CloseHandle(hFile);

    // Shutdown hooks
    VFSHooks::Shutdown();
    VFSRegistry::Clear();
}

TEST(VFSHooksTest, FallbackToRealFile) {
    // Write a real temp file
    std::string realFilename = "real_temp_file.txt";
    std::ofstream out(realFilename);
    out << "Real file data on disk";
    out.close();

    // Initialize hooks
    ASSERT_TRUE(VFSHooks::Initialize());

    // Attempt to open the real file (not registered in VFS) via VFS Hook
    HANDLE hFile = Hook_CreateFileA(realFilename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    ASSERT_NE(hFile, INVALID_HANDLE_VALUE);

    char buffer[100] = {0};
    DWORD bytesRead = 0;
    BOOL readRes = Hook_ReadFile(hFile, buffer, 22, &bytesRead, NULL);
    EXPECT_TRUE(readRes);
    EXPECT_EQ(bytesRead, 22);
    EXPECT_STREQ(buffer, "Real file data on disk");

    Hook_CloseHandle(hFile);
    VFSHooks::Shutdown();
    std::remove(realFilename.c_str());
}

TEST(MemoryPETest, LoadModuleAndResolveExports) {
    std::string dllPath = "Install/Compiler/plugins/DBProTransformsDebug.dll";
    std::ifstream in(dllPath, std::ios::binary);
    if (!in.is_open()) {
        dllPath = "d:/GitHub-repo/Dark-Basic-Pro/Install/Compiler/plugins/DBProTransformsDebug.dll";
        in.open(dllPath, std::ios::binary);
    }
    ASSERT_TRUE(in.is_open()) << "Failed to find DBProTransformsDebug.dll for testing";

    std::vector<char> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    VFSRegistry::Clear();
    VFSRegistry::Register("DBProTransformsDebug.dll", buffer.data(), buffer.size());

    ASSERT_TRUE(VFSHooks::Initialize());

    HMODULE hModule = MemoryPE::LoadFromVFS("DBProTransformsDebug.dll");
    ASSERT_NE(hModule, nullptr);
    EXPECT_TRUE(MemoryPE::IsMemoryModule(hModule));

    // Must resolve exports via memory module resolver Hook_GetProcAddress or MemoryPE::GetProcAddress
    FARPROC pFunc = Hook_GetProcAddress(hModule, "Constructor");
    if (!pFunc) {
        pFunc = Hook_GetProcAddress(hModule, "MD5");
    }
    EXPECT_NE(pFunc, nullptr);

    MemoryPE::UnloadModule(hModule);
    EXPECT_FALSE(MemoryPE::IsMemoryModule(hModule));

    VFSHooks::Shutdown();
    VFSRegistry::Clear();
}
