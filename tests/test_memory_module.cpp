// Minimal test-only module built by the test suite. It exists so the
// MemoryPE loader can be exercised against a real image whose bitness always
// matches the host process (PE32 on a 32-bit build, PE32+ on a 64-bit build).
// The shipped 32-bit runtime DLLs can never load inside a 64-bit process.

#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpReserved;
    return TRUE;
}

extern "C" __declspec(dllexport) int Constructor(void)
{
    return 42;
}

extern "C" __declspec(dllexport) int MD5(void)
{
    return 7;
}
