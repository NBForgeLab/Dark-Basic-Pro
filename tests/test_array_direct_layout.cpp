// test_array_direct_layout.cpp
// Validates the DIRECT array layout: header (56 bytes) immediately followed by element data.
// Handle = pointer to first data element. Element n at handle + n * itemSize.
// No ref table, no flag table.

#include <gtest/gtest.h>
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <memory>

// ---- Mirror the production header/layout from DBDLLCore.cpp ----
#pragma pack(push, 4)
struct DBProArrayHeader {
	uint32_t dimensions[9]; // 36 bytes
	uint32_t magic;         // 4 bytes  kDBProArrayMagic = 0xDB574152
	uint32_t size;          // 4 bytes
	uint32_t itemSize;      // 4 bytes
	uint32_t typeId;        // 4 bytes
	uint32_t cursor;        // 4 bytes
};
#pragma pack(pop)
static_assert(sizeof(DBProArrayHeader) == 56, "DBProArrayHeader must be exactly 56 bytes");

static constexpr uint32_t kDBProArrayMagic = 0xDB574152;

static inline DBProArrayHeader* GetArrayHeader(uintptr_t handle) noexcept {
	if (!handle) return nullptr;
	return reinterpret_cast<DBProArrayHeader*>(reinterpret_cast<char*>(handle) - sizeof(DBProArrayHeader));
}

static inline bool IsDynamicArrayMemory(const void* ptr) noexcept {
	if (!ptr) return false;
	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(ptr, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
	if (mbi.State != MEM_COMMIT ||
		(mbi.Protect != PAGE_READWRITE && mbi.Protect != PAGE_WRITECOPY) ||
		mbi.Type != MEM_PRIVATE)
		return false;
	const auto* pHeader = reinterpret_cast<const DBProArrayHeader*>(ptr);
	return (pHeader->magic == kDBProArrayMagic);
}

static inline bool IsValidArrayHandle(uintptr_t handle) noexcept {
	if (!handle) return false;
	const char* pHead = reinterpret_cast<const char*>(handle) - sizeof(DBProArrayHeader);
	return IsDynamicArrayMemory(pHead);
}

// Minimal CreateArray matching the refactored production code
static uintptr_t TestCreateArray(uint32_t size, uint32_t itemSize, uint32_t typeId) {
	size_t totalBytes = sizeof(DBProArrayHeader) + static_cast<size_t>(size) * itemSize;
	char* pRawMem = new char[totalBytes];
	memset(pRawMem, 0, totalBytes);
	auto* pHeader = reinterpret_cast<DBProArrayHeader*>(pRawMem);
	pHeader->magic = kDBProArrayMagic;
	pHeader->size = size;
	pHeader->itemSize = itemSize;
	pHeader->typeId = typeId;
	pHeader->cursor = 0;
	return reinterpret_cast<uintptr_t>(pRawMem + sizeof(DBProArrayHeader));
}

static void TestDeleteArray(uintptr_t handle) {
	if (!handle) return;
	char* pHead = reinterpret_cast<char*>(handle) - sizeof(DBProArrayHeader);
	if (IsDynamicArrayMemory(pHead)) {
		auto* pHeader = reinterpret_cast<DBProArrayHeader*>(pHead);
		pHeader->magic = 0;
		delete[] pHead;
	}
}

// String helpers (mirrored from production)
static constexpr uint32_t kDBProStringMagic = 0xDB575247;
struct DBProStringHeader { uint32_t magic; uint32_t size; };

static inline char* AllocateDynamicString(size_t len) {
	const size_t totalBytes = sizeof(DBProStringHeader) + len + 1;
	char* pMem = new char[totalBytes];
	memset(pMem, 0, totalBytes);
	auto* pH = reinterpret_cast<DBProStringHeader*>(pMem);
	pH->magic = kDBProStringMagic;
	pH->size = static_cast<uint32_t>(len);
	return pMem + sizeof(DBProStringHeader);
}

static inline bool IsDynamicHeapString(const void* ptr) {
	if (!ptr) return false;
	const char* strPtr = static_cast<const char*>(ptr);
	const void* headerPtr = strPtr - sizeof(DBProStringHeader);
	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(headerPtr, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
	if (mbi.State != MEM_COMMIT || (mbi.Protect != PAGE_READWRITE && mbi.Protect != PAGE_WRITECOPY)) return false;
	const auto* pH = reinterpret_cast<const DBProStringHeader*>(headerPtr);
	return (pH->magic == kDBProStringMagic);
}

static inline void FreeDynamicString(void* ptr) {
	if (!ptr) return;
	char* strPtr = static_cast<char*>(ptr);
	void* headerPtr = strPtr - sizeof(DBProStringHeader);
	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(headerPtr, &mbi, sizeof(mbi)) == sizeof(mbi) &&
		mbi.State == MEM_COMMIT &&
		(mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_WRITECOPY))
	{
		auto* pH = reinterpret_cast<DBProStringHeader*>(headerPtr);
		if (pH->magic == kDBProStringMagic) {
			pH->magic = 0;
			delete[] reinterpret_cast<char*>(pH);
		}
	}
}

// ============================================================
// TEST 1: Direct element write/read for itemSize 4 (integer array)
// ============================================================
TEST(ArrayDirectLayoutTest, IntegerArrayDirectElementAccess) {
	constexpr uint32_t N = 10;
	constexpr uint32_t itemSize = 4; // DWORD
	uintptr_t handle = TestCreateArray(N, itemSize, 1);
	ASSERT_NE(handle, 0u);
	ASSERT_TRUE(IsValidArrayHandle(handle));

	// Write values directly: handle + idx * itemSize
	for (uint32_t i = 0; i < N; ++i) {
		uint32_t* pElem = reinterpret_cast<uint32_t*>(handle + i * itemSize);
		*pElem = i * 100 + 42;
	}

	// Read back and verify
	for (uint32_t i = 0; i < N; ++i) {
		uint32_t val = *reinterpret_cast<uint32_t*>(handle + i * itemSize);
		EXPECT_EQ(val, i * 100 + 42) << "Mismatch at index " << i;
	}

	// Verify header fields
	auto* hdr = GetArrayHeader(handle);
	EXPECT_EQ(hdr->size, N);
	EXPECT_EQ(hdr->itemSize, itemSize);
	EXPECT_EQ(hdr->typeId, 1u);
	EXPECT_EQ(hdr->magic, kDBProArrayMagic);

	TestDeleteArray(handle);
}

// ============================================================
// TEST 2: Direct element write/read for itemSize 48 (UDT array)
// ============================================================
TEST(ArrayDirectLayoutTest, UDTArrayDirectElementAccessNoOverlap) {
	constexpr uint32_t N = 5;
	constexpr uint32_t itemSize = 48; // Simulating a UDT
	uintptr_t handle = TestCreateArray(N, itemSize, 9);
	ASSERT_NE(handle, 0u);

	// Write distinct patterns into each element
	for (uint32_t i = 0; i < N; ++i) {
		char* pElem = reinterpret_cast<char*>(handle + i * itemSize);
		memset(pElem, static_cast<int>(0xA0 + i), itemSize);
	}

	// Read back and verify no overlap
	for (uint32_t i = 0; i < N; ++i) {
		char* pElem = reinterpret_cast<char*>(handle + i * itemSize);
		for (uint32_t b = 0; b < itemSize; ++b) {
			EXPECT_EQ(static_cast<uint8_t>(pElem[b]), static_cast<uint8_t>(0xA0 + i))
				<< "Byte " << b << " of element " << i << " corrupted";
		}
	}

	TestDeleteArray(handle);
}

// ============================================================
// TEST 3: String array - store via slot, free doesn't crash, double-free safe
// ============================================================
TEST(ArrayDirectLayoutTest, StringArrayStoreAndFreeSafe) {
	constexpr uint32_t N = 4;
	constexpr uint32_t itemSize = sizeof(char*);
	uintptr_t handle = TestCreateArray(N, itemSize, 2); // typeId 2 = string
	ASSERT_NE(handle, 0u);

	// Store dynamic strings into slots
	char** slots = reinterpret_cast<char**>(handle);
	slots[0] = AllocateDynamicString(10);
	strcpy_s(slots[0], 11, "Hello");
	slots[1] = AllocateDynamicString(10);
	strcpy_s(slots[1], 11, "World");
	slots[2] = nullptr; // null slot
	slots[3] = AllocateDynamicString(20);
	strcpy_s(slots[3], 21, "DarkBasic Pro");

	// Verify strings are readable
	EXPECT_STREQ(slots[0], "Hello");
	EXPECT_STREQ(slots[1], "World");
	EXPECT_EQ(slots[2], nullptr);
	EXPECT_STREQ(slots[3], "DarkBasic Pro");

	// Free all strings (simulating FreeStringsFromArray for typeId==2)
	for (uint32_t i = 0; i < N; ++i) {
		if (slots[i] && IsDynamicHeapString(slots[i])) {
			FreeDynamicString(slots[i]);
		}
		slots[i] = nullptr;
	}

	// Double-free must be safe (all slots are now null)
	for (uint32_t i = 0; i < N; ++i) {
		if (slots[i] && IsDynamicHeapString(slots[i])) {
			FreeDynamicString(slots[i]);
		}
		slots[i] = nullptr;
	}

	TestDeleteArray(handle);
}

// ============================================================
// TEST 4: Expand preserves content (simulates ArrayInsertAtBottom/Top/ExpandArray)
// ============================================================
TEST(ArrayDirectLayoutTest, ExpandPreservesContent) {
	constexpr uint32_t initialSize = 3;
	constexpr uint32_t itemSize = 4;
	uintptr_t handle = TestCreateArray(initialSize, itemSize, 1);

	// Fill with known values
	for (uint32_t i = 0; i < initialSize; ++i)
		*reinterpret_cast<uint32_t*>(handle + i * itemSize) = (i + 1) * 10;

	// Simulate ExpandArray: create bigger, copy old data
	uint32_t newSize = initialSize + 2;
	uintptr_t newHandle = TestCreateArray(newSize, itemSize, 1);
	memcpy(reinterpret_cast<void*>(newHandle), reinterpret_cast<void*>(handle), initialSize * itemSize);

	// Old elements preserved
	for (uint32_t i = 0; i < initialSize; ++i)
		EXPECT_EQ(*reinterpret_cast<uint32_t*>(newHandle + i * itemSize), (i + 1) * 10);

	// New elements are zero
	for (uint32_t i = initialSize; i < newSize; ++i)
		EXPECT_EQ(*reinterpret_cast<uint32_t*>(newHandle + i * itemSize), 0u);

	TestDeleteArray(handle);
	TestDeleteArray(newHandle);
}

// ============================================================
// TEST 5: Insert-at-top shifts elements correctly
// ============================================================
TEST(ArrayDirectLayoutTest, InsertAtTopShiftsElementsCorrectly) {
	constexpr uint32_t itemSize = 4;
	// Start with [10, 20, 30]
	uintptr_t handle = TestCreateArray(3, itemSize, 1);
	*reinterpret_cast<uint32_t*>(handle + 0 * itemSize) = 10;
	*reinterpret_cast<uint32_t*>(handle + 1 * itemSize) = 20;
	*reinterpret_cast<uint32_t*>(handle + 2 * itemSize) = 30;

	// Expand by 1 -> [10, 20, 30, 0]
	uintptr_t expanded = TestCreateArray(4, itemSize, 1);
	auto* expHdr = GetArrayHeader(expanded);
	memcpy(expHdr->dimensions, GetArrayHeader(handle)->dimensions, 36);
	memcpy(reinterpret_cast<void*>(expanded), reinterpret_cast<void*>(handle), 3 * itemSize);
	TestDeleteArray(handle);

	// Shift: save last slot (0), move [10,20,30] to [1..3], put 0 at [0]
	char temp[4];
	memcpy(temp, reinterpret_cast<char*>(expanded) + 3 * itemSize, itemSize);
	memmove(reinterpret_cast<char*>(expanded) + itemSize, reinterpret_cast<char*>(expanded), 3 * itemSize);
	memcpy(reinterpret_cast<char*>(expanded), temp, itemSize);

	// Result should be [0, 10, 20, 30]
	EXPECT_EQ(*reinterpret_cast<uint32_t*>(expanded + 0 * itemSize), 0u);
	EXPECT_EQ(*reinterpret_cast<uint32_t*>(expanded + 1 * itemSize), 10u);
	EXPECT_EQ(*reinterpret_cast<uint32_t*>(expanded + 2 * itemSize), 20u);
	EXPECT_EQ(*reinterpret_cast<uint32_t*>(expanded + 3 * itemSize), 30u);

	TestDeleteArray(expanded);
}

// ============================================================
// TEST 6: Delete element shifts down correctly
// ============================================================
TEST(ArrayDirectLayoutTest, DeleteElementShiftsDownCorrectly) {
	constexpr uint32_t itemSize = 4;
	// [10, 20, 30, 40]
	uintptr_t handle = TestCreateArray(4, itemSize, 1);
	*reinterpret_cast<uint32_t*>(handle + 0 * itemSize) = 10;
	*reinterpret_cast<uint32_t*>(handle + 1 * itemSize) = 20;
	*reinterpret_cast<uint32_t*>(handle + 2 * itemSize) = 30;
	*reinterpret_cast<uint32_t*>(handle + 3 * itemSize) = 40;

	// Delete element at index 1 (value 20): shift [30,40] down to [1,2]
	int iIndex = 1;
	size_t offset = iIndex * itemSize;
	size_t elementsAfter = 4 - iIndex - 1; // 2
	memmove(reinterpret_cast<char*>(handle) + offset,
	        reinterpret_cast<char*>(handle) + offset + itemSize,
	        elementsAfter * itemSize);

	// Update header size
	auto* hdr = GetArrayHeader(handle);
	hdr->size = 3;

	// Result: [10, 30, 40]
	EXPECT_EQ(*reinterpret_cast<uint32_t*>(handle + 0 * itemSize), 10u);
	EXPECT_EQ(*reinterpret_cast<uint32_t*>(handle + 1 * itemSize), 30u);
	EXPECT_EQ(*reinterpret_cast<uint32_t*>(handle + 2 * itemSize), 40u);
	EXPECT_EQ(hdr->size, 3u);

	TestDeleteArray(handle);
}

// ============================================================
// TEST 7: EmptyArray resets size to 0 keeping type
// ============================================================
TEST(ArrayDirectLayoutTest, EmptyArrayResetsSizeKeepsType) {
	constexpr uint32_t itemSize = 4;
	uintptr_t handle = TestCreateArray(5, itemSize, 1);
	*reinterpret_cast<uint32_t*>(handle) = 42;

	// Simulate EmptyArray
	auto* hdr = GetArrayHeader(handle);
	memset(reinterpret_cast<void*>(handle), 0, hdr->size * itemSize);
	hdr->size = 0;
	hdr->cursor = 0;

	EXPECT_EQ(hdr->size, 0u);
	EXPECT_EQ(hdr->typeId, 1u); // type preserved
	EXPECT_EQ(hdr->itemSize, itemSize);

	TestDeleteArray(handle);
}

// ============================================================
// TEST 8: DimDDD(0->N) then UnDimDD; ReDimCore preserve semantics
// ============================================================
TEST(ArrayDirectLayoutTest, DimAndReDimPreserveSemantics) {
	constexpr uint32_t itemSize = 4;

	// Dim: create array of 5 integers
	uintptr_t handle = TestCreateArray(5, itemSize, 1);
	auto* hdr = GetArrayHeader(handle);
	hdr->dimensions[0] = 5;

	// Fill values
	for (uint32_t i = 0; i < 5; ++i)
		*reinterpret_cast<uint32_t*>(handle + i * itemSize) = i + 1;

	// ReDim to 8 (preserve): create new, copy min(5,8)=5 elements
	uintptr_t newHandle = TestCreateArray(8, itemSize, 1);
	auto* newHdr = GetArrayHeader(newHandle);
	newHdr->dimensions[0] = 8;
	memcpy(reinterpret_cast<void*>(newHandle), reinterpret_cast<void*>(handle), 5 * itemSize);
	TestDeleteArray(handle);

	// First 5 preserved
	for (uint32_t i = 0; i < 5; ++i)
		EXPECT_EQ(*reinterpret_cast<uint32_t*>(newHandle + i * itemSize), i + 1);

	// New slots are zero
	for (uint32_t i = 5; i < 8; ++i)
		EXPECT_EQ(*reinterpret_cast<uint32_t*>(newHandle + i * itemSize), 0u);

	TestDeleteArray(newHandle);
}

// ============================================================
// TEST 9: Next/Previous/ArrayIndexValid cursor walk
// ============================================================
TEST(ArrayDirectLayoutTest, CursorWalkNextPreviousValid) {
	constexpr uint32_t itemSize = 4;
	uintptr_t handle = TestCreateArray(5, itemSize, 1);
	auto* hdr = GetArrayHeader(handle);

	// ArrayIndexToTop
	hdr->cursor = 0;
	EXPECT_EQ(hdr->cursor, 0u);
	EXPECT_LT(hdr->cursor, hdr->size); // valid

	// Next through all elements
	for (uint32_t expected = 1; expected <= 5; ++expected) {
		hdr->cursor++;
		if (hdr->cursor > hdr->size) hdr->cursor = hdr->size;
		EXPECT_EQ(hdr->cursor, expected);
	}
	// At cursor=5, size=5 => invalid
	EXPECT_FALSE(hdr->cursor < hdr->size);

	// Previous back down
	for (int expected = 4; expected >= 0; --expected) {
		if (static_cast<int>(hdr->cursor) > 0)
			hdr->cursor--;
		else
			hdr->cursor = static_cast<uint32_t>(-1);
		if (expected >= 0)
			EXPECT_EQ(hdr->cursor, static_cast<uint32_t>(expected));
	}

	// Previous from 0 -> -1
	if (static_cast<int>(hdr->cursor) > 0)
		hdr->cursor--;
	else
		hdr->cursor = static_cast<uint32_t>(-1);
	EXPECT_EQ(hdr->cursor, static_cast<uint32_t>(-1));
	EXPECT_FALSE(hdr->cursor < hdr->size); // invalid

	TestDeleteArray(handle);
}

// ============================================================
// TEST 10: Handle points to data, header is at handle - 56
// ============================================================
TEST(ArrayDirectLayoutTest, HeaderIsExactly56BytesBeforeHandle) {
	constexpr uint32_t itemSize = 8;
	uintptr_t handle = TestCreateArray(3, itemSize, 3);
	auto* hdr = GetArrayHeader(handle);

	// The header should be exactly 56 bytes before the handle
	EXPECT_EQ(reinterpret_cast<char*>(handle) - reinterpret_cast<char*>(hdr), 56);

	// First element should be at handle itself
	uint64_t* firstElem = reinterpret_cast<uint64_t*>(handle);
	*firstElem = 0xDEADBEEFCAFEBABEULL;
	EXPECT_EQ(*firstElem, 0xDEADBEEFCAFEBABEULL);

	// Second element at handle + 8
	uint64_t* secondElem = reinterpret_cast<uint64_t*>(handle + 8);
	*secondElem = 0x123456789ABCDEF0ULL;
	EXPECT_EQ(*secondElem, 0x123456789ABCDEF0ULL);

	// No corruption of header
	EXPECT_EQ(hdr->magic, kDBProArrayMagic);
	EXPECT_EQ(hdr->size, 3u);

	TestDeleteArray(handle);
}
