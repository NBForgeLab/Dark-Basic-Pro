#include <gtest/gtest.h>
#include "VFSHooks.h"
#include "MemoryPE.h"
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

std::string ReadHandle(HANDLE handle, const DWORD count) {
    std::string result(count, '\0');
    DWORD bytesRead = 0;
    EXPECT_TRUE(Hook_ReadFile(
        handle,
        result.data(),
        count,
        &bytesRead,
        nullptr));
    result.resize(bytesRead);
    return result;
}

} // namespace

TEST(VFSRegistryTest, OwnsRegisteredBytesAfterCallerIsDestroyed) {
    VFSRegistry::Clear();
    {
        std::vector<std::uint8_t> temporary{
            'V', 'F', 'S', ' ', 'o', 'w', 'n', 'e', 'd',
        };
        ASSERT_TRUE(VFSRegistry::RegisterOwned(
            "test.txt",
            std::move(temporary)));
    }

    EXPECT_TRUE(VFSRegistry::Exists("test.txt"));
    EXPECT_FALSE(VFSRegistry::Exists("nonexistent.txt"));

    const auto opened = VFSRegistry::Open("test.txt");
    ASSERT_TRUE(opened) << opened.error().message;
    std::array<char, 32> bytes{};
    const auto read = opened.value()->Read(bytes.data(), bytes.size());
    ASSERT_TRUE(read) << read.error().message;
    EXPECT_EQ(std::string(bytes.data(), read.value()), "VFS owned");
    
    VFSRegistry::Clear();
    EXPECT_FALSE(VFSRegistry::Exists("test.txt"));
}

TEST(VFSHooksTest, InterceptFileOperations) {
    VFSRegistry::Clear();
    std::string testData = "In-memory file data";
    ASSERT_TRUE(VFSRegistry::RegisterOwned(
        "virtual_file.txt",
        std::vector<std::uint8_t>(
            testData.begin(),
            testData.end())));

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

TEST(VFSHooksTest, UsesExactPathsWithoutImplicitBasenameAliases) {
    VFSRegistry::Clear();
    ASSERT_TRUE(VFSRegistry::RegisterOwned(
        "media/same.dat",
        std::vector<std::uint8_t>{'v', 'f', 's'}));

    const auto root =
        std::filesystem::temp_directory_path() / "dbp-vfs-exact-path";
    std::filesystem::create_directories(root);
    const auto diskPath = root / "same.dat";
    {
        std::ofstream output(diskPath, std::ios::binary | std::ios::trunc);
        output << "disk";
    }

    const auto diskHandle = Hook_CreateFileW(
        diskPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    ASSERT_NE(diskHandle, INVALID_HANDLE_VALUE);
    EXPECT_EQ(ReadHandle(diskHandle, 4), "disk");
    EXPECT_TRUE(Hook_CloseHandle(diskHandle));

    const auto virtualHandle = Hook_CreateFileA(
        "media\\same.dat",
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    ASSERT_NE(virtualHandle, INVALID_HANDLE_VALUE);
    EXPECT_EQ(ReadHandle(virtualHandle, 3), "vfs");
    EXPECT_TRUE(Hook_CloseHandle(virtualHandle));

    VFSRegistry::Clear();
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}

TEST(VFSHooksTest, MaintainsIndependentCursorsAndLifetimeAcrossClear) {
    VFSRegistry::Clear();
    ASSERT_TRUE(VFSRegistry::RegisterOwned(
        "cursor.bin",
        std::vector<std::uint8_t>{'a', 'b', 'c', 'd'}));
    const auto first = Hook_CreateFileA(
        "cursor.bin", GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    const auto second = Hook_CreateFileA(
        "cursor.bin", GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(first, INVALID_HANDLE_VALUE);
    ASSERT_NE(second, INVALID_HANDLE_VALUE);

    EXPECT_EQ(ReadHandle(first, 2), "ab");
    EXPECT_EQ(ReadHandle(second, 3), "abc");
    EXPECT_EQ(
        Hook_SetFilePointer(first, 0, nullptr, FILE_BEGIN),
        0U);
    VFSRegistry::Clear();
    EXPECT_EQ(ReadHandle(first, 4), "abcd");
    EXPECT_EQ(ReadHandle(second, 1), "d");
    EXPECT_TRUE(Hook_CloseHandle(first));
    EXPECT_TRUE(Hook_CloseHandle(second));
}

TEST(VFSHooksTest, NeverFallsBackToDiskForMountedWriteRequests) {
    VFSRegistry::Clear();
    const auto diskPath =
        std::filesystem::path("vfs-mounted-write-test.dat");
    {
        std::ofstream output(
            diskPath,
            std::ios::binary | std::ios::trunc);
        output << "disk";
    }
    ASSERT_TRUE(VFSRegistry::RegisterOwned(
        diskPath.string(),
        std::vector<std::uint8_t>{'v', 'f', 's'}));

    const auto handle = Hook_CreateFileA(
        diskPath.string().c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    EXPECT_EQ(handle, INVALID_HANDLE_VALUE);
    EXPECT_EQ(GetLastError(), ERROR_ACCESS_DENIED);
    std::ifstream input(diskPath, std::ios::binary);
    std::string contents{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>(),
    };
    input.close();
    EXPECT_EQ(contents, "disk");
    VFSRegistry::Clear();
    std::error_code ignored;
    std::filesystem::remove(diskPath, ignored);
}

TEST(VFSHooksTest, SupportsConcurrentOpenReadAndClose) {
    VFSRegistry::Clear();
    const std::string expected(4096, 'q');
    ASSERT_TRUE(VFSRegistry::RegisterOwned(
        "concurrent.bin",
        std::vector<std::uint8_t>(expected.begin(), expected.end())));

    std::vector<std::future<bool>> operations;
    for (std::size_t index = 0; index < 16; ++index) {
        operations.push_back(std::async(std::launch::async, [&expected] {
            const auto handle = Hook_CreateFileA(
                "concurrent.bin", GENERIC_READ, FILE_SHARE_READ, nullptr,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle == INVALID_HANDLE_VALUE) {
                return false;
            }
            const auto contents =
                ReadHandle(handle, static_cast<DWORD>(expected.size()));
            return Hook_CloseHandle(handle) &&
                contents == expected;
        }));
    }
    for (auto& operation : operations) {
        EXPECT_TRUE(operation.get());
    }
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
    // The legacy 32-bit runtime DLLs can never execute inside a 64-bit
    // process, so the loader is exercised against a test module whose bitness
    // matches the host build (PE32 on x86, PE32+ on x64).
    std::string dllPath = DBP_TEST_MEMORY_MODULE_PATH;
    std::ifstream in(dllPath, std::ios::binary);
    ASSERT_TRUE(in.is_open()) << "Failed to find test memory module for testing: " << dllPath;

    std::vector<char> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    VFSRegistry::Clear();
    ASSERT_TRUE(VFSRegistry::RegisterOwned(
        "DBProTransformsDebug.dll",
        std::vector<std::uint8_t>(buffer.begin(), buffer.end())));

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

TEST(MemoryPETest, RejectsLegacy32BitModuleOn64BitHost) {
#ifdef _WIN64
    // A 32-bit PE image can never run in a 64-bit process; the loader must
    // refuse it cleanly instead of executing its entry point.
    std::string dllPath =
        (std::filesystem::path(DBP_TEST_SOURCE_ROOT) /
         "Install/Compiler/plugins/DBProTransformsDebug.dll").string();
    std::ifstream in(dllPath, std::ios::binary);
    if (!in.is_open()) {
        GTEST_SKIP() << "Legacy 32-bit DLL not present";
    }
    std::vector<char> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    VFSRegistry::Clear();
    ASSERT_TRUE(VFSRegistry::RegisterOwned(
        "Legacy32.dll",
        std::vector<std::uint8_t>(buffer.begin(), buffer.end())));
    ASSERT_TRUE(VFSHooks::Initialize());

    HMODULE hModule = MemoryPE::LoadFromVFS("Legacy32.dll");
    EXPECT_EQ(hModule, nullptr);
    EXPECT_FALSE(MemoryPE::IsMemoryModule(hModule));

    VFSHooks::Shutdown();
    VFSRegistry::Clear();
#else
    GTEST_SKIP() << "32-bit host may execute 32-bit modules";
#endif
}
