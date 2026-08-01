# Target ABI Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace host-width pointer assumptions with a tested target-ABI boundary, make the affected compiler-core APIs const-correct, and restore a warning-clean Win32 build while preparing the layout code for a future PE32+ target.

**Architecture:** A small header-only binary codec performs bounds-checked `memcpy` transfers, while a separate target-ABI traits header describes serialized 32-bit and 64-bit addresses independently of the compiler host. The existing compiler explicitly selects the PE32 traits; layout, debugger snapshots, and runtime cleanup consume the same traits so future target selection has one controlled seam.

**Tech Stack:** C++17, MSVC v143, Win32/PE32, CMake, GoogleTest, Win32 API.

---

## File map

- Create `DBProCompiler/DBPCompiler/BinaryCodec.h`: alignment-safe, bounds-checked reads and writes for trivially-copyable values.
- Create `DBProCompiler/DBPCompiler/TargetABI.h`: x86/x64 serialized-address traits, active target selection, checked host-address conversion.
- Create `tests/test_target_abi.cpp`: architecture-independent regression tests for 32-bit and 64-bit target address slots.
- Modify `tests/CMakeLists.txt`: compile the new regression tests.
- Modify `DBProCompiler/DBPCompiler/StructTable.cpp`: derive pointer-like language type sizes from the active target ABI.
- Modify `DBProCompiler/DBPCompiler/VarTable.cpp`: derive array slot offsets from the same ABI.
- Modify `tests/test_structural.cpp` and `tests/test_vartable.cpp`: pin the layout contract.
- Modify `DBProCompiler/DBPCompiler/ASMWriter.cpp`: decode variable-space pointers safely and serialize fixed-width fields without typed byte-buffer dereferences.
- Modify `DBProCompiler/DBPCompiler/EXEBlock.cpp`: decode dynamic allocation pointers through the same target ABI boundary.
- Modify `DBProCompiler/DBPCompiler/Declaration.h`, `StructTable.h`, `VarTable.h`, and `StatementList.h`: add mutable/const getter overloads.
- Modify `DBProCompiler/DBPCompiler/Error.cpp` and `tests/test_error.cpp`: consume the token-line result and test the diagnostic behavior.
- Modify `README.md`: accurately describe PE32 as active and PE32+ as a staged future target.

### Task 1: Add the safe binary codec and target ABI traits

**Files:**
- Create: `DBProCompiler/DBPCompiler/BinaryCodec.h`
- Create: `DBProCompiler/DBPCompiler/TargetABI.h`
- Create: `tests/test_target_abi.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Add the new test source to the test target**

Add `test_target_abi.cpp` beside `test_x64_pointer_safety.cpp` in `tests/CMakeLists.txt`.

- [ ] **Step 2: Write failing target ABI tests**

Create `tests/test_target_abi.cpp` with tests equivalent to:

```cpp
#include <gtest/gtest.h>

#include "TargetABI.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace {

TEST(TargetAbiTest, ActiveTargetIsExplicitlyPe32) {
    static_assert(std::is_same_v<dbp::abi::ActiveTargetAbi,
                                 dbp::abi::TargetAbi32>);
    EXPECT_EQ(dbp::abi::ActiveTargetAbi::address_size, 4U);
}

TEST(TargetAbiTest, ReadsUnalignedPe32AddressWithoutAdjacentBytes) {
    std::array<std::byte, 9> bytes{};
    const std::uint32_t expected = 0x12345678U;
    const std::uint32_t adjacent = 0xDEADBEEFU;
    std::memcpy(bytes.data() + 1, &expected, sizeof(expected));
    std::memcpy(bytes.data() + 5, &adjacent, sizeof(adjacent));

    const auto value = dbp::abi::ReadAddress<dbp::abi::TargetAbi32>(
        bytes.data(), bytes.size(), 1);

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, expected);
}

TEST(TargetAbiTest, RejectsTruncatedAndOutOfRangeSlots) {
    std::array<std::byte, 4> bytes{};
    EXPECT_FALSE(dbp::abi::ReadAddress<dbp::abi::TargetAbi32>(
        bytes.data(), bytes.size(), 1));
    EXPECT_FALSE(dbp::abi::ReadAddress<dbp::abi::TargetAbi32>(
        bytes.data(), bytes.size(), bytes.size() + 1));
    EXPECT_FALSE(dbp::abi::ReadAddress<dbp::abi::TargetAbi32>(
        nullptr, bytes.size(), 0));
}

TEST(TargetAbiTest, ModelsFullPe32PlusAddressOnAnyHost) {
    std::array<std::byte, 8> bytes{};
    const std::uint64_t expected = 0x7FFF000088889999ULL;
    std::memcpy(bytes.data(), &expected, sizeof(expected));

    const auto value = dbp::abi::ReadAddress<dbp::abi::TargetAbi64>(
        bytes.data(), bytes.size(), 0);

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, expected);
}

TEST(TargetAbiTest, HostConversionRejectsNarrowing) {
    const auto converted = dbp::abi::ToHostAddress<dbp::abi::TargetAbi64>(
        std::numeric_limits<std::uint64_t>::max());
    if constexpr (sizeof(std::uintptr_t) < sizeof(std::uint64_t)) {
        EXPECT_FALSE(converted.has_value());
    } else {
        ASSERT_TRUE(converted.has_value());
        EXPECT_EQ(*converted, std::numeric_limits<std::uintptr_t>::max());
    }
}

} // namespace
```

- [ ] **Step 3: Run the focused test build and verify RED**

Run:

```powershell
cmake --build --preset windows-x86-debug --target dbp_tests
```

Expected: compilation fails because `TargetABI.h` does not exist.

- [ ] **Step 4: Implement `BinaryCodec.h`**

Create a header with `dbp::binary::ReadTrivial<T>` and
`dbp::binary::WriteTrivial<T>`. Both functions must:

```cpp
static_assert(std::is_trivially_copyable_v<T>);
if (data == nullptr || offset > size || sizeof(T) > size - offset)
    return failure;
std::memcpy(...);
```

`ReadTrivial` returns `std::optional<T>` and `WriteTrivial` returns `bool`.
Use `std::byte` for byte arithmetic and mark both functions `noexcept`.

- [ ] **Step 5: Implement `TargetABI.h`**

Define:

```cpp
namespace dbp::abi {

template <std::size_t AddressBits>
struct TargetAbiTraits;

template <>
struct TargetAbiTraits<32> {
    using address_type = std::uint32_t;
    static constexpr std::size_t address_size = sizeof(address_type);
};

template <>
struct TargetAbiTraits<64> {
    using address_type = std::uint64_t;
    static constexpr std::size_t address_size = sizeof(address_type);
};

using TargetAbi32 = TargetAbiTraits<32>;
using TargetAbi64 = TargetAbiTraits<64>;
using ActiveTargetAbi = TargetAbi32;

template <typename Abi>
[[nodiscard]] std::optional<typename Abi::address_type> ReadAddress(
    const void* data, std::size_t size, std::size_t offset) noexcept;

template <typename Abi>
[[nodiscard]] std::optional<std::uintptr_t> ToHostAddress(
    typename Abi::address_type value) noexcept;

template <typename Pointer, typename Abi = ActiveTargetAbi>
[[nodiscard]] std::optional<Pointer> ReadPointer(
    const void* data, std::size_t size, std::size_t offset) noexcept;

} // namespace dbp::abi
```

`ReadAddress` delegates to `ReadTrivial`. `ToHostAddress` checks for narrowing
before casting. `ReadPointer` requires a pointer type, preserves a stored null as
a valid null pointer, and returns `std::nullopt` only for malformed data or a
non-representable host address.

- [ ] **Step 6: Run the focused tests and verify GREEN**

Run:

```powershell
cmake --build --preset windows-x86-debug --target dbp_tests
& out/build/windows-x86-debug/bin/Debug/dbp_tests.exe --gtest_filter=TargetAbiTest.*
```

Expected: all `TargetAbiTest` cases pass.

- [ ] **Step 7: Commit the foundation**

```powershell
git add DBProCompiler/DBPCompiler/BinaryCodec.h DBProCompiler/DBPCompiler/TargetABI.h tests/test_target_abi.cpp tests/CMakeLists.txt
git commit -m "feat(compiler): add explicit target ABI foundation"
```

### Task 2: Make variable-space layout consume the target ABI

**Files:**
- Modify: `DBProCompiler/DBPCompiler/StructTable.cpp:89-115`
- Modify: `DBProCompiler/DBPCompiler/VarTable.cpp:781-790`
- Modify: `tests/test_structural.cpp`
- Modify: `tests/test_vartable.cpp`

- [ ] **Step 1: Write failing layout contract tests**

Add tests asserting that `string`, every built-in array type, and user-defined
pointer types have `ActiveTargetAbi::address_size`. Add a `CVarTable` test that
sets the array flag, calls `EstablishVarOffsets`, and expects the accumulated
offset to equal the same size.

- [ ] **Step 2: Run the focused tests and verify RED**

Run:

```powershell
cmake --build --preset windows-x86-debug --target dbp_tests
& out/build/windows-x86-debug/bin/Debug/dbp_tests.exe --gtest_filter=StructTableTest.*:VarTableTest.ArrayOffsetUsesActiveTargetAbi
```

Expected: the tests fail to compile or fail because layout still uses literal
pointer widths instead of the new ABI contract.

- [ ] **Step 3: Replace pointer-size literals**

Include `TargetABI.h` and define a local checked `DWORD` constant:

```cpp
constexpr DWORD kTargetAddressSize =
    static_cast<DWORD>(dbp::abi::ActiveTargetAbi::address_size);
static_assert(dbp::abi::ActiveTargetAbi::address_size <=
              std::numeric_limits<DWORD>::max());
```

Use it for `string`, array handles, user-defined pointer types, and the array
branch of `CVarTable::EstablishVarOffsets`. Leave four-byte language numerics and
labels unchanged.

- [ ] **Step 4: Run focused tests and verify GREEN**

Run the filter from Step 2. Expected: all selected tests pass.

- [ ] **Step 5: Commit layout integration**

```powershell
git add DBProCompiler/DBPCompiler/StructTable.cpp DBProCompiler/DBPCompiler/VarTable.cpp tests/test_structural.cpp tests/test_vartable.cpp
git commit -m "refactor(compiler): centralize target pointer layout"
```

### Task 3: Remove unsafe variable-space pointer decoding

**Files:**
- Modify: `DBProCompiler/DBPCompiler/ASMWriter.cpp:755-825`
- Modify: `DBProCompiler/DBPCompiler/EXEBlock.cpp:1637-1648`
- Modify: `tests/test_target_abi.cpp`

- [ ] **Step 1: Extend the failing tests for pointer decoding**

Add cases for `ReadPointer<char*, TargetAbi32>` that verify a stored null is a
successful null result, a valid low address is decoded exactly, and a truncated
slot returns `std::nullopt`.

- [ ] **Step 2: Run focused tests and verify RED**

Expected: failure until `ReadPointer` implements the required null and bounds
semantics.

- [ ] **Step 3: Implement and integrate safe decoding**

In `ASMWriter.cpp`, include `TargetABI.h` directly. Replace both nested
`reinterpret_cast<uintptr_t*>` dereferences with:

```cpp
const auto stringPointer =
    dbp::abi::ReadPointer<LPSTR>(g_pVarSpaceAddressInUse,
                                g_dwVarSpaceSizeInUse,
                                dwOffset);
if (!stringPointer) {
    *pdwDataSize = 0;
    return nullptr;
}
LPSTR pStringInMemory = *stringPointer;
```

Guard a null `pdwDataSize` at function entry. Use `WriteTrivial` for fixed-width
fields written into the transfer buffer so the changed function contains no
typed dereference into `char[]` storage.

In `EXEBlock.cpp`, decode dynamic allocation slots through
`ReadPointer<DWORD*>`. Skip malformed slots after logging the offset; never read
past `m_dwVariableSpaceSize`.

- [ ] **Step 4: Run focused ABI and existing ASM writer tests**

```powershell
cmake --build --preset windows-x86-debug --target dbp_tests
& out/build/windows-x86-debug/bin/Debug/dbp_tests.exe --gtest_filter=TargetAbiTest.*:ASMWriterEmissionTest.*:EXEBlockMemoryTest.*
```

Expected: all selected tests pass.

- [ ] **Step 5: Commit safe decoding**

```powershell
git add DBProCompiler/DBPCompiler/ASMWriter.cpp DBProCompiler/DBPCompiler/EXEBlock.cpp tests/test_target_abi.cpp
git commit -m "fix(compiler): decode runtime pointers through target ABI"
```

### Task 4: Correct API contracts and diagnostic line handling

**Files:**
- Modify: `DBProCompiler/DBPCompiler/Declaration.h`
- Modify: `DBProCompiler/DBPCompiler/StructTable.h`
- Modify: `DBProCompiler/DBPCompiler/VarTable.h`
- Modify: `DBProCompiler/DBPCompiler/StatementList.h`
- Modify: `DBProCompiler/DBPCompiler/Error.cpp:86-99`
- Modify: `tests/test_declaration.cpp`
- Modify: `tests/test_vartable.cpp`
- Modify: `tests/test_error.cpp`

- [ ] **Step 1: Write failing compile-time API tests**

Use `std::declval` and `std::is_same_v` to assert that mutable objects return
mutable pointers and const objects return pointers to const for owned/referenced
state. Cover at least `CDeclaration::GetName`, `CVarTable::GetVarName`,
`CStructTable::GetTypeName`, and `CStatementList::GetProgramStatements`.

- [ ] **Step 2: Write the failing diagnostic test**

In the context-backed error fixture, set token line 37 and word 11 to a known
label, call `AddErrorString`, and assert that `GetParserErrorString()` contains
both the label and `37`. This fails while the getter result is discarded.

- [ ] **Step 3: Run focused tests and verify RED**

```powershell
cmake --build --preset windows-x86-debug --target dbp_tests
```

Expected: compile-time assertions fail because const objects return mutable
pointers; after those assertions are temporarily isolated, the diagnostic test
fails because `dwLineNum` remains zero.

- [ ] **Step 4: Implement paired getter overloads**

For pointer-returning accessors use the pattern:

```cpp
[[nodiscard]] T* GetValue() noexcept { return m_value; }
[[nodiscard]] const T* GetValue() const noexcept { return m_value; }
```

For `unique_ptr<T>`, return `m_value.get()`. Apply the same rule to character
buffers by returning `LPSTR` from mutable objects and `LPCSTR` from const
objects. Scalar getters remain `const noexcept`.

- [ ] **Step 5: Consume the diagnostic line result**

Replace the two statements with:

```cpp
const DWORD dwLineNum = g_pStatementList->GetTokenLineNumber();
```

- [ ] **Step 6: Run focused tests and verify GREEN**

```powershell
cmake --build --preset windows-x86-debug --target dbp_tests
& out/build/windows-x86-debug/bin/Debug/dbp_tests.exe --gtest_filter=DeclarationTest.*:VarTableTest.*:Error*.*
```

Expected: all selected tests pass and MSVC emits no C4834 warning from
`Error.cpp`.

- [ ] **Step 7: Commit API contract fixes**

```powershell
git add DBProCompiler/DBPCompiler/Declaration.h DBProCompiler/DBPCompiler/StructTable.h DBProCompiler/DBPCompiler/VarTable.h DBProCompiler/DBPCompiler/StatementList.h DBProCompiler/DBPCompiler/Error.cpp tests/test_declaration.cpp tests/test_vartable.cpp tests/test_error.cpp
git commit -m "refactor(compiler): enforce const-correct core accessors"
```

### Task 5: Document the boundary and run quality gates

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Correct architecture documentation**

Document that the active production ABI is PE32/Win32, the compiler no longer
derives target layout from host pointer width, and native x64 still requires the
PE32+ code generator, runtime calling conventions, and plug-in ABI migration.

- [ ] **Step 2: Scan the changed production paths**

Run:

```powershell
rg -n "reinterpret_cast<.*uintptr_t\*|dwAddSize = 4.*Pointer|AddStruct\(3, \"string\".*, 4\)" DBProCompiler/DBPCompiler
```

Expected: no match in the variable-space paths changed by this plan.

- [ ] **Step 3: Run a fresh full build**

```powershell
cmake --build --preset windows-x86-debug --clean-first
```

Expected: exit code zero and no warnings caused by changed files.

- [ ] **Step 4: Run the complete test suite**

```powershell
ctest --preset windows-x86-debug --output-on-failure
& out/build/windows-x86-debug/bin/Debug/dbp_tests.exe --gtest_brief=1
```

Expected: CTest reports zero failures and GoogleTest reports all registered cases
passing.

- [ ] **Step 5: Inspect repository quality**

```powershell
git diff --check
git status --short
git log --oneline -6
```

Expected: no whitespace errors; only intended documentation state remains; the
implementation is split into reviewable commits.

- [ ] **Step 6: Commit documentation**

```powershell
git add README.md
git commit -m "docs(architecture): clarify PE32 and x64 migration boundary"
```
