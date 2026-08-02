# Language Conformance Matrix & Stress Testing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand DarkBASIC Pro language conformance test cases (`tests/conformance/`) to cover 3D/4D arrays, memory allocation lifecycles, complex operator precedence, and UDT array members.

**Architecture:** Add LLVM-lit style self-contained `.dba` test cases under `tests/conformance/arrays/` and `tests/conformance/expressions/`, and run them via `run-conformance.Tests.ps1`.

**Tech Stack:** DarkBASIC Pro (.dba), PowerShell Pester (v5), CMake / CTest, C++ (DBPCompiler).

---

### Task 1: Add 3D and 4D Array Conformance Tests

**Files:**
- Create: `tests/conformance/arrays/multidimensional_3d.dba`
- Create: `tests/conformance/arrays/multidimensional_4d.dba`

- [ ] **Step 1: Write `multidimensional_3d.dba` test file**

```basic
REM EXPECT: COMPILE_SUCCESS
REM EXPECT: RUNTIME_OUTPUT "ARR3D: 777"
REM EXPECT: EXIT_CODE 0

DIM grid(5, 5, 5)
grid(2, 3, 4) = 777
PRINT "ARR3D: "; grid(2, 3, 4)
END
```

- [ ] **Step 2: Write `multidimensional_4d.dba` test file**

```basic
REM EXPECT: COMPILE_SUCCESS
REM EXPECT: RUNTIME_OUTPUT "ARR4D: 9999"
REM EXPECT: EXIT_CODE 0

DIM hyper(3, 3, 3, 3)
hyper(1, 2, 3, 1) = 9999
PRINT "ARR4D: "; hyper(1, 2, 3, 1)
END
```

- [ ] **Step 3: Run conformance suite to verify test execution**

Run: `Invoke-Pester -Path tests/conformance/run-conformance.Tests.ps1 -PassThru`
Expected: PASS (all tests pass)

- [ ] **Step 4: Commit**

```bash
git add tests/conformance/arrays/multidimensional_3d.dba tests/conformance/arrays/multidimensional_4d.dba
git commit -m "test(conformance): add 3d and 4d array multidimensional conformance tests"
```

---

### Task 2: Add Array Memory Lifecycle (`DIM` / `UNDIM`) and UDT Array Conformance Tests

**Files:**
- Create: `tests/conformance/arrays/dim_undim_lifecycle.dba`
- Create: `tests/conformance/arrays/udt_array_members.dba`

- [ ] **Step 1: Write `dim_undim_lifecycle.dba` test file**

```basic
REM EXPECT: COMPILE_SUCCESS
REM EXPECT: RUNTIME_OUTPUT "LIFECYCLE_OK"
REM EXPECT: EXIT_CODE 0

DIM tempArray(100)
tempArray(50) = 12345
UNDIM tempArray
DIM tempArray(50)
tempArray(10) = 54321
IF tempArray(10) = 54321 THEN PRINT "LIFECYCLE_OK"
END
```

- [ ] **Step 2: Write `udt_array_members.dba` test file**

```basic
REM EXPECT: COMPILE_SUCCESS
REM EXPECT: RUNTIME_OUTPUT "PLAYER: Hero / HP: 100"
REM EXPECT: EXIT_CODE 0

TYPE PlayerInfo
    name AS STRING
    health AS INTEGER
ENDTYPE

DIM party(5) AS PlayerInfo
party(1).name = "Hero"
party(1).health = 100

PRINT "PLAYER: "; party(1).name; " / HP: "; party(1).health
END
```

- [ ] **Step 3: Run conformance test suite**

Run: `Invoke-Pester -Path tests/conformance/run-conformance.Tests.ps1 -PassThru`
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add tests/conformance/arrays/dim_undim_lifecycle.dba tests/conformance/arrays/udt_array_members.dba
git commit -m "test(conformance): add array lifecycle and UDT array members conformance tests"
```

---

### Task 3: Add Mixed Precedence & Case-Insensitivity Expression Tests

**Files:**
- Create: `tests/conformance/expressions/mixed_operator_case.dba`
- Create: `tests/conformance/expressions/logical_precedence_chain.dba`

- [ ] **Step 1: Write `mixed_operator_case.dba` test file**

```basic
REM EXPECT: COMPILE_SUCCESS
REM EXPECT: RUNTIME_OUTPUT "CASE_OK: 1"
REM EXPECT: EXIT_CODE 0

a = 10
b = 20
IF (a < 15 and b > 15) AND (NOT (a = b)) THEN res = 1 ELSE res = 0
PRINT "CASE_OK: "; res
END
```

- [ ] **Step 2: Write `logical_precedence_chain.dba` test file**

```basic
REM EXPECT: COMPILE_SUCCESS
REM EXPECT: RUNTIME_OUTPUT "CHAIN_OK: 10"
REM EXPECT: EXIT_CODE 0

x = 5 + 10 DIV 2
y = 20 MOD 3
z = x + y
PRINT "CHAIN_OK: "; z
END
```

- [ ] **Step 3: Run conformance test suite & local CI**

Run: `Invoke-Pester -Path tests/conformance/run-conformance.Tests.ps1 -PassThru`
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add tests/conformance/expressions/mixed_operator_case.dba tests/conformance/expressions/logical_precedence_chain.dba
git commit -m "test(conformance): add mixed case operator and precedence chain tests"
```

---

### Task 4: Full CI Loop Verification

- [ ] **Step 1: Run local CI script**

Run: `powershell -ExecutionPolicy Bypass -File scripts/run-local-ci.ps1 -SkipGolden -SkipFPSTests`
Expected: All phases (Compile, C++Tests, Conformance) PASSED.

- [ ] **Step 2: Final Commit and Status Check**

Run: `git status`
Expected: Working tree clean.
