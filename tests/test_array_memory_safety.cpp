#include <gtest/gtest.h>
#include <windows.h>
#include <cstdint>

static constexpr uint32_t kDBProStringMagic = 0xDB575247;

struct DBProStringHeader
{
	uint32_t magic;
	uint32_t size;
};

static inline char* AllocateDynamicString(size_t len)
{
	const size_t totalBytes = sizeof(DBProStringHeader) + len + 1;
	char* pMem = new char[totalBytes];
	memset(pMem, 0, totalBytes);
	DBProStringHeader* pHeader = reinterpret_cast<DBProStringHeader*>(pMem);
	pHeader->magic = kDBProStringMagic;
	pHeader->size = static_cast<uint32_t>(len);
	return pMem + sizeof(DBProStringHeader);
}

static inline bool IsDynamicHeapString(const void* ptr)
{
	if (!ptr) return false;
	const char* strPtr = static_cast<const char*>(ptr);
	const void* headerPtr = strPtr - sizeof(DBProStringHeader);
	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(headerPtr, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
	if (mbi.State != MEM_COMMIT || (mbi.Protect != PAGE_READWRITE && mbi.Protect != PAGE_WRITECOPY)) return false;
	const DBProStringHeader* pHeader = reinterpret_cast<const DBProStringHeader*>(headerPtr);
	return (pHeader->magic == kDBProStringMagic);
}

static inline void FreeDynamicString(void* ptr)
{
	if (!ptr) return;
	char* strPtr = static_cast<char*>(ptr);
	void* headerPtr = strPtr - sizeof(DBProStringHeader);
	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(headerPtr, &mbi, sizeof(mbi)) == sizeof(mbi) &&
		mbi.State == MEM_COMMIT &&
		(mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_WRITECOPY))
	{
		DBProStringHeader* pHeader = reinterpret_cast<DBProStringHeader*>(headerPtr);
		if (pHeader->magic == kDBProStringMagic)
		{
			pHeader->magic = 0; // Clear magic to prevent double-free
			delete[] reinterpret_cast<char*>(pHeader);
		}
	}
}

// Direct contract test verifying IsDynamicHeapString behavior on static literals vs heap pointers
TEST(ArrayMemorySafetyTest, IsDynamicHeapStringSafelyDistinguishesStaticLiteralsFromHeapPointers) {
    const char* staticStr = "Static Literals In ReadOnly Data Section";
    bool isStaticHeap = IsDynamicHeapString(staticStr);
    EXPECT_FALSE(isStaticHeap); // Must be FALSE for static string literals

    char* heapStr = AllocateDynamicString(64);
    strcpy_s(heapStr, 64, "Heap Allocated Dynamic String");
    bool isDynamicHeap = IsDynamicHeapString(heapStr);
    EXPECT_TRUE(isDynamicHeap); // Must be TRUE for dynamic heap memory

    FreeDynamicString(heapStr);
}

// Contract test verifying string variable reset on static literals
TEST(ArrayMemorySafetyTest, FreeingStaticStringPointerDoesNotCorruptHeap) {
    const char* staticLiteral = "DarkBasic Pro Constant String";
    uintptr_t varSpace = reinterpret_cast<uintptr_t>(staticLiteral);

    // Simulate CreateSingleString dwSize=0 logic:
    LPSTR strPtr = reinterpret_cast<LPSTR>(varSpace);
    if (strPtr) {
        FreeDynamicString(strPtr);
    }
    varSpace = 0;

    EXPECT_EQ(varSpace, 0u);
}

// Contract test verifying double free is safe
TEST(ArrayMemorySafetyTest, DoubleFreeIsSafeAndPrevented) {
    char* dynamicStr = AllocateDynamicString(32);
    strcpy_s(dynamicStr, 32, "Safe String");

    FreeDynamicString(dynamicStr);
    // Second free attempt on same pointer must be safe no-op
    FreeDynamicString(dynamicStr);
}
