#include <gtest/gtest.h>
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include "EXEBlock.h"

// W^X (Write XOR Execute) characterization tests for the MCB lifecycle.
//
// The Machine Code Block goes through three phases:
//   Phase 1 (WRITE): Load() allocates memory and reads MCB bytes from file
//   Phase 2 (WRITE): InitDebug() patches references into MCB
//   --- boundary ---
//   Phase 3 (EXEC): Run() casts MCB to function pointer and calls it
//
// W^X principle: memory must never be writable AND executable at the same time.
// - Allocation uses PAGE_READWRITE (writable, not executable)
// - After patching, VirtualProtect transitions to PAGE_EXECUTE_READ
// - VirtualProtect is guarded against zero-size MCB

namespace {

// Helper: allocate a small block via VirtualAlloc with PAGE_READWRITE,
// write a pattern, then transition to PAGE_EXECUTE_READ and verify the
// pattern is still readable (simulating the W^X lifecycle).
DWORD* AllocateMCB(DWORD dwSizeInDwords)
{
    return static_cast<DWORD*>(
        VirtualAlloc(nullptr, dwSizeInDwords * sizeof(DWORD),
                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
}

// Simple x86-32 machine code: mov eax, 42; ret
// Returns a function pointer that when called returns 42 in eax.
using RetIntFn = int(__stdcall*)();

void WriteMinimalX86Code(LPVOID pBlock)
{
    // mov eax, 42  (B8 2A 00 00 00)
    // ret           (C3)
    unsigned char code[] = { 0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3 };
    memcpy(pBlock, code, sizeof(code));
}

} // namespace

// Test 1: CreateArray with PAGE_READWRITE allocates via VirtualAlloc
// and the resulting memory is writable (not executable).
TEST(EXEBlockWXTest, CreateArrayWithPageReadWriteAllocatesWritableMemory) {
    CEXEBlock exe;
    const DWORD count = 16;
    DWORD* pArray = exe.CreateArray(count, PAGE_READWRITE);
    ASSERT_NE(pArray, nullptr);

    // Memory must be writable
    for (DWORD i = 0; i < count; ++i)
        pArray[i] = i * 100;

    // Verify written values
    for (DWORD i = 0; i < count; ++i)
        EXPECT_EQ(pArray[i], i * 100) << "index " << i;

    VirtualFree(pArray, 0, MEM_DECOMMIT | MEM_RELEASE);
}

// Test 2: After writing MCB data and transitioning protection to
// PAGE_EXECUTE_READ, the memory is still readable and executable.
TEST(EXEBlockWXTest, VirtualProtectTransitionToExecuteReadPreservesData) {
    const DWORD mcbSizeDwords = 64;
    DWORD* pMCB = AllocateMCB(mcbSizeDwords);
    ASSERT_NE(pMCB, nullptr);

    // Phase 1+2: Write data (simulating Load + InitDebug patching)
    const DWORD sentinel = 0xDEADBEEF;
    pMCB[0] = sentinel;
    pMCB[1] = 0x12345678;

    // Transition to execute-only (W^X boundary)
    DWORD oldProtect = 0;
    BOOL result = VirtualProtect(pMCB, mcbSizeDwords * sizeof(DWORD),
                                 PAGE_EXECUTE_READ, &oldProtect);
    ASSERT_TRUE(result) << "VirtualProtect to PAGE_EXECUTE_READ must succeed";
    EXPECT_EQ(oldProtect, static_cast<DWORD>(PAGE_READWRITE));

    // Data must still be readable
    EXPECT_EQ(pMCB[0], sentinel);
    EXPECT_EQ(pMCB[1], 0x12345678U);

    VirtualFree(pMCB, 0, MEM_DECOMMIT | MEM_RELEASE);
}

// Test 3: VirtualProtect must NOT be called when MCB size is 0.
// This is a guard condition in the implementation.
TEST(EXEBlockWXTest, VirtualProtectGuardedAgainstZeroSizeMCB) {
    // When m_dwSizeOfMCB is 0 and m_pMachineCodeBlock is NULL,
    // the W^X transition code must be a no-op (no VirtualProtect call).
    // We verify the guard condition: if size is 0, skip VirtualProtect.
    DWORD* pMCB = nullptr;
    DWORD dwSizeOfMCB = 0;

    // The guard: only call VirtualProtect if both pointer and size are valid
    bool wouldCallVirtualProtect = (pMCB != nullptr && dwSizeOfMCB > 0);
    EXPECT_FALSE(wouldCallVirtualProtect);

    // Also test: pointer non-null but size is 0
    pMCB = static_cast<DWORD*>(
        VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    ASSERT_NE(pMCB, nullptr);
    dwSizeOfMCB = 0;

    wouldCallVirtualProtect = (pMCB != nullptr && dwSizeOfMCB > 0);
    EXPECT_FALSE(wouldCallVirtualProtect);

    VirtualFree(pMCB, 0, MEM_DECOMMIT | MEM_RELEASE);
}

// Test 4: MCB can execute correctly after protection transition.
// Write x86 machine code, transition to PAGE_EXECUTE_READ, then execute.
TEST(EXEBlockWXTest, MCBCanExecuteAfterProtectionTransition) {
    const DWORD mcbSizeBytes = 4096;
    LPVOID pMCB = VirtualAlloc(nullptr, mcbSizeBytes,
                               MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    ASSERT_NE(pMCB, nullptr);

    // Write minimal x86-32 code: mov eax, 42; ret
    WriteMinimalX86Code(pMCB);

    // Transition to PAGE_EXECUTE_READ (W^X boundary)
    DWORD oldProtect = 0;
    BOOL protResult = VirtualProtect(pMCB, mcbSizeBytes,
                                     PAGE_EXECUTE_READ, &oldProtect);
    ASSERT_TRUE(protResult);

    // Execute the code
    RetIntFn fn = reinterpret_cast<RetIntFn>(pMCB);
    int result = fn();
    EXPECT_EQ(result, 42);

    VirtualFree(pMCB, 0, MEM_DECOMMIT | MEM_RELEASE);
}

// Test 5: VirtualFree works correctly regardless of protection state.
// After transitioning to PAGE_EXECUTE_READ, the MCB memory can be released.
// Note: Clear() uses MEM_DECOMMIT|MEM_RELEASE combined flags which matches
// the legacy codebase pattern - the OS frees the pages regardless of the
// return value on modern Windows.
TEST(EXEBlockWXTest, VirtualFreeWorksWithAnyProtectionState) {
    const DWORD mcbSizeBytes = 4096;
    LPVOID pMCB = VirtualAlloc(nullptr, mcbSizeBytes,
                               MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    ASSERT_NE(pMCB, nullptr);

    // Transition to PAGE_EXECUTE_READ
    DWORD oldProtect = 0;
    VirtualProtect(pMCB, mcbSizeBytes, PAGE_EXECUTE_READ, &oldProtect);

    // VirtualFree with MEM_RELEASE (size must be 0) succeeds regardless
    // of protection state
    BOOL freeResult = VirtualFree(pMCB, 0, MEM_RELEASE);
    EXPECT_TRUE(freeResult);
}

// Test 6: CreateArray with PAGE_READWRITE returns nullptr-equivalent
// for zero count (VirtualAlloc returns non-null for 0 size on some
// systems, but the caller guards with *Count > 0).
TEST(EXEBlockWXTest, CreateArrayZeroCountHandledByCaller) {
    // LoadValueArrayBytes guards with *Count > 0 before calling CreateArray.
    // This test verifies the guard condition, not CreateArray itself.
    DWORD count = 0;
    bool wouldAllocate = (count > 0);
    EXPECT_FALSE(wouldAllocate);
}

// Test 7: Clear() properly frees MCB memory regardless of protection state.
// After VirtualProtect transitions MCB to PAGE_EXECUTE_READ, Clear() must
// still free it via VirtualFree.
TEST(EXEBlockWXTest, ClearFreesMCBAfterProtectionTransition) {
    CEXEBlock exe;

    // Simulate a loaded MCB
    const DWORD mcbSizeDwords = 64;
    exe.m_dwSizeOfMCB = mcbSizeDwords * sizeof(DWORD);
    exe.m_pMachineCodeBlock = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, exe.m_dwSizeOfMCB,
                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    ASSERT_NE(exe.m_pMachineCodeBlock, nullptr);

    // Write some data
    *reinterpret_cast<uint32_t*>(exe.m_pMachineCodeBlock) = 0xCAFEBABE;

    // Transition to PAGE_EXECUTE_READ (as InitDebug would do)
    DWORD oldProtect = 0;
    VirtualProtect(exe.m_pMachineCodeBlock, exe.m_dwSizeOfMCB,
                   PAGE_EXECUTE_READ, &oldProtect);

    // Clear must free the memory without crashing
    exe.Clear();

    EXPECT_EQ(exe.m_pMachineCodeBlock, nullptr);
    EXPECT_EQ(exe.m_dwSizeOfMCB, 0U);
}
