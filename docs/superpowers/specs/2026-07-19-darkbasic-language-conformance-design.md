# DarkBASIC Language Conformance Test Suite Design Spec

## 🎯 Goal
Implement a robust, deterministic, and highly maintainable test suite for verifying the DarkBASIC language compiler and runtime. This conformance suite acts as a safety gate to prevent regressions when modernizing the compiler architecture, parser, and transition to x64.

---

## 🏗️ Architectural Pattern: Self-Contained Tests (LLVM-lit Style)
To avoid the maintenance overhead of central test manifests, we adopt the modern compiler industry standard (similar to LLVM's `lit` tool). 

Each test case (`.dba` file) is fully self-contained, declaring its own compiler expectations and runtime assertions in header comments using the `REM` prefix.

### Supported Directives
*   `REM EXPECT: COMPILE_SUCCESS`
    Indicates that the compiler must compile this file with exit code 0 and produce a target executable.
*   `REM EXPECT: COMPILE_FAIL`
    Indicates that the compiler must reject this file (e.g., syntax/semantic error) and exit with a non-zero code.
*   `REM EXPECT: COMPILER_ERROR "<substring>"`
    (Optional, used with `COMPILE_FAIL`) Verifies that the compiler diagnostics contains the specified error message.
*   `REM EXPECT: EXIT_CODE <number>`
    Specifies the expected exit code of the compiled application when run (default is `0`).
*   `REM EXPECT: RUNTIME_OUTPUT "<substring>"`
    Specifies a substring that must appear in the console output when the compiled application runs. Multiple `RUNTIME_OUTPUT` directives are checked in order.
*   `REM EXPECT: TIMEOUT_SECONDS <number>`
    Sets the maximum runtime execution limit (default is `5` seconds).

---

## 📁 Directory Structure
All conformance tests reside under `tests/conformance/` in the `Dark-Basic-Pro` repository:
```text
Dark-Basic-Pro/
└── tests/
    └── conformance/
        ├── run-conformance.Tests.ps1   # Pester Test Runner
        ├── expressions/                # Operators, precedence, math
        ├── variables/                  # Types, suffix rules, casting
        ├── arrays/                     # Dimensioning, multi-dimensional array access
        ├── control_flow/               # If-Then-Else, For, While loops, Repeat-Until
        ├── functions/                  # Parameter passing, local scopes, return types
        ├── includes/                   # #include behavior and expansion
        └── errors/                     # Language-rejected syntax cases
```

---

## 🛠️ Test Runner Architecture (run-conformance.Tests.ps1)
The test runner is built in **PowerShell** using the industry-standard **Pester** BDD testing framework.

### Execution Workflow
For each `.dba` file discovered in the `conformance` directory:
1.  **Parse Directives**: Scan the file's top lines to extract `REM EXPECT:` definitions.
2.  **Isolated Staging (Sandboxing)**: 
    *   Create a clean, isolated temporary workspace directory.
    *   Copy the target `.dba` file (and any required includes) into this workspace.
3.  **Compile Execution**:
    *   Invoke `DBPCompiler.exe` with `--runtime-root` targeting the active compiler configuration, and `--output` targeting the temporary workspace.
    *   Capture compiler stdout, stderr, and exit code.
4.  **Compilation Validation**:
    *   If `COMPILE_SUCCESS` is expected, verify the exit code is 0 and the `.exe` file exists.
    *   If `COMPILE_FAIL` is expected, verify the exit code is non-zero. If `COMPILER_ERROR` is defined, verify the compiler output contains the expected message.
5.  **Runtime Validation (if compilation succeeded)**:
    *   Execute the compiled binary from the workspace.
    *   Enforce the specified timeout. If exceeded, terminate the process and fail the test.
    *   Capture output and exit code.
    *   Verify the application exit code matches `expectedExitCode`.
    *   Verify stdout/stderr contains all expected `RUNTIME_OUTPUT` substrings.

---

## 🧪 Verification and Quality Gates
*   All test cases are fully isolated to prevent workspace pollution.
*   Tests run headlessly without manual interaction.
*   The test suite runs automatically during full maintenance loops.
