# Language Conformance Matrix & Stress Testing Expansion Design Spec

## 🎯 Goal
Expand the DarkBASIC Pro language conformance test suite (`tests/conformance/`) with comprehensive, deterministic, and self-contained test coverage focusing on multi-dimensional array indexing (`CalcArrayOffset`), boundary validation, operator precedence (`AND`, `OR`, `NOT`, `XOR`, `DIV`, `MOD`), and User-Defined Types (UDT) array members. Optimize the test runner (`run-conformance.Tests.ps1`) for fast execution.

---

## 🏗️ Architectural Design

### 1. Test Suite Expansion Categories
The conformance suite will be expanded under `tests/conformance/` with new LLVM-lit style `.dba` test files:

#### A. Arrays & Memory Bounds (`tests/conformance/arrays/`)
- `multidimensional_3d.dba`: Verifies 3D array element calculation `array(x, y, z)` and offset correctness across boundaries.
- `multidimensional_4d.dba`: Verifies 4D array element indexing `array(w, x, y, z)` with boundary value checks.
- `array_bounds_check.dba`: Verifies that invalid array indexing produces deterministic error or safe rejection.
- `dim_undim_lifecycle.dba`: Verifies dynamic array allocation (`DIM`) and deallocation (`UNDIM`) lifecycle without leaks or stale pointers.
- `udt_array_members.dba`: Verifies arrays of User-Defined Types (`TYPE...ENDTYPE`) with nested fields and string members.

#### B. Complex Expressions & Logical Precedence (`tests/conformance/expressions/`)
- `mixed_operator_case.dba`: Verifies case-insensitivity across `and`, `And`, `AND`, `or`, `Xor`, `Not`, `div`, `mod` in compound expressions.
- `logical_precedence_chain.dba`: Verifies complex operator precedence ordering between arithmetic (`+`, `-`, `*`, `/`, `DIV`, `MOD`) and logical (`AND`, `OR`, `XOR`, `NOT`).
- `float_int_expression_mix.dba`: Verifies mixed integer/float type coercion and precision retention in compound expressions.

---

## 🛠️ Conformance Test Runner Optimization (`run-conformance.Tests.ps1`)
- Support parallel execution of sandboxed test cases in Pester.
- Enforce strict 5-second per-test timeout and clean up temporary workspaces atomically.
- Capture stdout/stderr cleanly and validate `REM EXPECT: COMPILE_SUCCESS`, `REM EXPECT: RUNTIME_OUTPUT`, and `REM EXPECT: EXIT_CODE 0`.

---

## 🧪 Verification Plan
1. Run all expanded conformance tests via Pester runner:
   `Invoke-Pester -Path tests/conformance/run-conformance.Tests.ps1 -PassThru`
2. Run full local CI loop:
   `powershell -ExecutionPolicy Bypass -File scripts/run-local-ci.ps1 -SkipGolden -SkipFPSTests`
3. Verify 100% pass rate across all new and existing conformance test cases.
