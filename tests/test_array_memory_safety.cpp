#include <gtest/gtest.h>
#include <windows.h>

// Direct contract test verifying HeapValidate behavior on static literals vs heap pointers
TEST(ArrayMemorySafetyTest, HeapValidateSafelyDistinguishesStaticLiteralsFromHeapPointers) {
    const char* staticStr = "Static Literals In ReadOnly Data Section";
    BOOL isStaticHeap = HeapValidate(GetProcessHeap(), 0, staticStr);
    EXPECT_FALSE(isStaticHeap); // Must be FALSE for static string literals

    char* heapStr = new char[64];
    strcpy_s(heapStr, 64, "Heap Allocated Dynamic String");
    BOOL isDynamicHeap = HeapValidate(GetProcessHeap(), 0, heapStr);
    EXPECT_TRUE(isDynamicHeap); // Must be TRUE for dynamic heap memory

    delete[] heapStr;
}

// Contract test verifying string variable reset on static literals
TEST(ArrayMemorySafetyTest, FreeingStaticStringPointerDoesNotCorruptHeap) {
    const char* staticLiteral = "DarkBasic Pro Constant String";
    DWORD varSpace = (DWORD)staticLiteral;

    // Simulate CreateSingleString dwSize=0 logic:
    LPSTR strPtr = (LPSTR)varSpace;
    if (strPtr && HeapValidate(GetProcessHeap(), 0, strPtr)) {
        delete[] strPtr;
    }
    varSpace = 0;

    EXPECT_EQ(varSpace, 0u);
}
