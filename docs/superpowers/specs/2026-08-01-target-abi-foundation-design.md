# Target ABI Foundation and Compiler Core Modernization

## Status

Approved for implementation on 2026-08-01. The project is modernizing toward
C++17/C++20 while preserving the currently supported PE32 runtime and preparing
for a future native x64 runtime.

## Problem

The compiler currently mixes three distinct concepts:

1. the address width of the compiler process;
2. the address width of the generated Dark Basic Professional program; and
3. the serialized width of addresses in compiler/runtime data structures.

Using `uintptr_t` at an isolated read site therefore changes the number of bytes
read according to the compiler host, even though the variable-space layout still
stores four-byte PE32 addresses. The code also performs typed dereferences on
byte buffers, which can violate alignment and aliasing rules. Pointer-sized
layout constants are duplicated as literal `4` values, so future x64 work can
silently update one layer without updating the others.

The recent attribute modernization also exposed an ignored return value and
made several `const` member functions return mutable pointers to owned state.
Those interfaces need real contracts rather than mechanical annotations.

## Goals

- Make the generated target ABI independent from the compiler host ABI.
- Preserve the current PE32 ABI without relying on host pointer width.
- Model both 32-bit and 64-bit target address storage in testable C++17 code.
- Make byte-buffer reads bounds-checked, alignment-safe, and aliasing-safe.
- Centralize pointer-slot sizes used by variable and structure layout.
- Make affected getters genuinely const-correct.
- Use `[[nodiscard]]` and `noexcept` only where their contracts are correct.
- Add regression tests that exercise both 32-bit and 64-bit serialized slots on
  the existing Win32 test runner.
- Leave an explicit, reviewable boundary for the later PE64/code-generator and
  runtime migration.

## Non-goals for this phase

- Emitting PE32+ images or x64 machine code.
- Changing the public plug-in ABI.
- Converting every Win32 type or raw pointer in the repository in one change.
- Claiming that the runtime supports x64 before PE64 code generation, calling
  conventions, loader behavior, and plug-in compatibility are implemented.

These are later modernization phases that build on this foundation.

## Architecture

### Target ABI traits

Introduce a small header-only target ABI module. It defines traits for 32-bit
and 64-bit serialized addresses without consulting `sizeof(void*)`:

- `TargetAbiTraits<32>::address_type` is `std::uint32_t`.
- `TargetAbiTraits<64>::address_type` is `std::uint64_t`.
- Each trait exposes `address_size` derived from its address type.
- `ActiveTargetAbi` explicitly aliases the supported PE32 target.

The active alias is intentionally not selected from the compiler platform.
That permits a future x64 compiler host to continue targeting PE32 correctly
and permits a future target selector to choose PE32 or PE32+ deliberately.

### Safe serialized reads and writes

Provide reusable C++17 helpers operating on byte ranges:

- validate `offset <= size` and `sizeof(T) <= size - offset` without overflow;
- transfer trivially-copyable values with `std::memcpy`;
- return `std::optional<T>` for reads that can fail;
- never form a typed pointer into an arbitrary byte buffer;
- keep address decoding separate from conversion to a host pointer.

The generic helper is tested with both ABI traits. Production code uses the
active PE32 trait until the x64 runtime is genuinely available.

### Variable-space layout

All runtime pointer-like built-in types use `ActiveTargetAbi::address_size`:

- strings;
- all array handles;
- user-defined variable and array pointers;
- dynamically allocated variable slots.

Four-byte numeric values such as `integer`, `float`, and `dword` remain four
bytes because their language representation is not pointer-sized.

`CVarTable::EstablishVarOffsets` uses the same target ABI constant for array
slots. This creates one source of truth and prevents host-width drift.

### ASM/debugger transfer

`CASMWriter::MakeVarValuesForTransfer` will decode string addresses through the
safe target ABI reader. Invalid offsets or truncated pointer slots fail the
transfer deterministically instead of reading adjacent or out-of-range bytes.
The transfer format itself retains its documented fixed-width fields for PE32.

Pointer decoding will be factored into a small independently tested function;
the large legacy writer will only coordinate iteration and serialization.

### API contracts

Owned or referenced object getters receive paired overloads:

```cpp
T* GetValue() noexcept;
const T* GetValue() const noexcept;
```

Scalar getters remain `const noexcept`. `[[nodiscard]]` is retained where
discarding the result is likely a defect. Existing ignored results are audited
and either consumed or explicitly documented and cast to `void` when intentional.

The ignored `GetTokenLineNumber` result is assigned to the local diagnostic line
number so the annotation produces no warning and the intended behavior works.

## Error handling

- Low-level buffer decoding returns `std::nullopt` for invalid bounds.
- The ASM writer treats an invalid variable-space pointer slot as a failed
  debugger snapshot and returns a null buffer with size zero.
- No exception is required for malformed runtime memory.
- Functions marked `noexcept` perform only non-throwing operations or contain
  their error paths explicitly.

## Testing strategy

Tests are written before production changes and must demonstrate the original
failure or missing contract.

1. Decode an unaligned 32-bit address slot correctly.
2. Ignore non-slot bytes immediately following a 32-bit address.
3. Reject truncated and out-of-range address slots.
4. Decode a full 64-bit address through the 64-bit trait, even in a Win32 test
   process, without converting it to a host pointer.
5. Assert the active target remains PE32 and pointer-like language types use its
   address size.
6. Compile-time assertions verify mutable and const getter return types.
7. Error-report tests verify the token line number is consumed where practical;
   otherwise the clean compiler build is the contract check for the warning.

After focused tests pass, run the complete Debug build and test suite. A clean
rebuild is required to detect newly introduced `[[nodiscard]]` warnings.

## Quality gates

- `git diff --check` passes.
- Win32 Debug compiler, runtime, and tests build successfully.
- Focused ABI and API contract tests pass.
- The full GoogleTest suite passes.
- No warnings introduced by changed files.
- No production path derives target pointer width from host `uintptr_t`.
- Documentation describes PE32 as the active ABI and x64 as a future target.

## Follow-on phases

1. Replace remaining typed byte-buffer dereferences in debugger, machine-code,
   PE inspection, and runtime cleanup paths with the safe byte codec.
2. Introduce strong semantic types for byte offsets, target addresses, indexes,
   and Win32 handles instead of using `DWORD` for unrelated concepts.
3. Modernize string boundaries with `std::string_view`, explicit encodings, and
   narrow legacy adapters.
4. Add a PE32+/x64 code-generation backend, x64 runtime calling conventions, and
   a versioned plug-in ABI.
5. Enable native x64 build and execution presets only when the end-to-end path is
   covered by integration tests.
