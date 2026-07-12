# Standalone DBPro Project Compilation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `DBPCompiler.exe <project.dbpro>` assemble and compile declared sources without Synergy Editor, and prevent parser-time machine-code emission.

**Architecture:** Add a typed manifest reader, deterministic source assembler, and owned `CompilationInput` before the legacy parser. Keep final-source output explicit and atomic, and enforce parse-before-initialize-before-emit ordering through a code-generation session boundary.

**Tech Stack:** C++17, `std::filesystem`, RAII byte buffers, GoogleTest, CMake/CTest, MSVC Win32, JSON Lines diagnostics.

---

## File Structure

- `ProjectManifest.*`: parse and validate legacy `.dbpro` manifests.
- `SourceAssembler.*`: assemble source bytes and optionally publish final source atomically.
- `CompilationInput.*`: owned input shared by direct DBA and DBPro builds.
- `CodeGenerationSession.*`: enforce backend initialization state.
- `tests/test_project_manifest.cpp`: manifest behavior.
- `tests/test_source_assembler.cpp`: assembly and artifact behavior.
- `tests/test_compilation_input.cpp`: input convergence.
- `tests/test_codegen_session.cpp`: backend ordering.
- `tests/test_cli_project.cpp`: standalone CLI behavior.
- `tests/test_fpsc_projects.cpp`: FPS Creator compatibility gate.
- `DBPCompiler.*`, `Main.cpp`, `Statement.*`, `DBMWriter.cpp`: narrow integration points.

### Task 1: Typed DBPro Manifest Reader

**Files:**
- Create: `DBProCompiler/DBPCompiler/ProjectManifest.h`
- Create: `DBProCompiler/DBPCompiler/ProjectManifest.cpp`
- Create: `tests/test_project_manifest.cpp`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

Test case-insensitive keys, `main` first, numeric `include1..N` ordering,
unknown-field tolerance, missing main, include gaps, malformed include keys,
duplicates, spaces, and absolute paths. Core assertions:

```cpp
const auto result = ProjectManifestReader::Read(project.path());
ASSERT_TRUE(result);
EXPECT_EQ(result.value().sources[0].manifestKey, "main");
EXPECT_EQ(result.value().sources[2].manifestKey, "include2");
```

- [ ] **Step 2: Register and run RED**

```powershell
cmake --build --preset windows-x86-debug --target dbp_tests
& .\out\build\windows-x86-debug\bin\Debug\dbp_tests.exe --gtest_filter=ProjectManifestReaderTest.*
```

Expected: build fails because the reader API does not exist.

- [ ] **Step 3: Implement the minimal owned model**

```cpp
enum class ProjectErrorCode {
    FileNotFound, FileUnreadable, MissingMain, MalformedIncludeKey,
    DuplicateIncludeIndex, NonContiguousIncludes
};
struct ProjectSourceEntry {
    std::string manifestKey;
    std::filesystem::path declaredPath;
    std::filesystem::path resolvedPath;
};
struct ProjectManifest {
    std::filesystem::path projectPath;
    std::filesystem::path projectDirectory;
    std::vector<ProjectSourceEntry> sources;
    std::optional<std::filesystem::path> finalSourcePath;
    std::optional<std::filesystem::path> executablePath;
};
```

Use owned values, checked index parsing, ASCII case folding for legacy keys, and
paths resolved relative to the project directory without changing CWD.

- [ ] **Step 4: Run GREEN and refactor**

Run the focused filter; expected all tests pass. Remove duplication while tests
remain green.

- [ ] **Step 5: Commit**

```powershell
git add DBProCompiler/DBPCompiler/ProjectManifest.* DBProCompiler/DBPCompiler/CMakeLists.txt tests/test_project_manifest.cpp tests/CMakeLists.txt
git commit -m "feat: parse DBPro project manifests"
```

### Task 2: Deterministic Source Assembly

**Files:**
- Create: `DBProCompiler/DBPCompiler/SourceAssembler.h`
- Create: `DBProCompiler/DBPCompiler/SourceAssembler.cpp`
- Create: `tests/test_source_assembler.cpp`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

Test exact order, Synergy-compatible CR/LF/CRLF normalization, missing or
unreadable files, checked maximum size, ANSI byte preservation, and source-map
offsets.

```cpp
const auto result = SourceAssembler::Assemble(manifest, {.maxBytes = 1024});
ASSERT_TRUE(result);
EXPECT_EQ(result.value().bytes, Bytes("alpha\r\nbeta\r\n"));
```

- [ ] **Step 2: Run RED**

Run `dbp_tests --gtest_filter=SourceAssemblerTest.*`; expected missing symbols.

- [ ] **Step 3: Implement checked binary assembly**

```cpp
struct SourceAssemblyOptions { std::uint64_t maxBytes = 256ull * 1024 * 1024; };
struct SourceMapEntry {
    std::filesystem::path path;
    std::uint64_t combinedByteStart;
    std::uint64_t combinedLineStart;
};
struct AssembledSource {
    std::vector<std::byte> bytes;
    std::vector<SourceMapEntry> sourceMap;
};
```

Preflight sizes, reject overflow before addition, reserve once, read binary,
preserve non-line-ending bytes, normalize all line endings to `\r\n`, and
terminate each non-empty source file exactly as Synergy's line writer did.

- [ ] **Step 4: Run GREEN and refactor**

Expected all assembly tests pass.

- [ ] **Step 5: Commit**

```powershell
git add DBProCompiler/DBPCompiler/SourceAssembler.* DBProCompiler/DBPCompiler/CMakeLists.txt tests/test_source_assembler.cpp tests/CMakeLists.txt
git commit -m "feat: assemble ordered DBPro project sources"
```

### Task 3: Atomic Final-Source Artifact and Owned Input

**Files:**
- Modify: `DBProCompiler/DBPCompiler/SourceAssembler.h`
- Modify: `DBProCompiler/DBPCompiler/SourceAssembler.cpp`
- Create: `DBProCompiler/DBPCompiler/CompilationInput.h`
- Create: `DBProCompiler/DBPCompiler/CompilationInput.cpp`
- Create: `tests/test_compilation_input.cpp`
- Modify: `tests/test_source_assembler.cpp`
- Modify: both CMake lists

- [ ] **Step 1: Write failing artifact and input tests**

Prove atomic replacement, temp cleanup, preservation of an old destination on
failure, stable buffer lifetime, and convergence of direct DBA/project inputs.

```cpp
const auto result = FinalSourceArtifactWriter::WriteAtomically(path, bytes);
ASSERT_TRUE(result);
EXPECT_EQ(ReadBytes(path), bytes);
EXPECT_FALSE(std::filesystem::exists(path.string() + ".tmp"));
```

- [ ] **Step 2: Run RED**

Run `SourceAssemblerTest.*:CompilationInputTest.*`; expected missing APIs.

- [ ] **Step 3: Implement atomic writer and input factories**

Write a unique sibling temp file, flush/close, then `MoveFileExW` with
`MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`; clean up via RAII.

```cpp
class CompilationInput {
public:
    static CompilationInputResult FromSourceFile(const std::filesystem::path&);
    static CompilationInputResult FromProject(const ProjectManifest&, SourceAssemblyOptions);
    const std::vector<std::byte>& bytes() const noexcept;
    const std::filesystem::path& baseDirectory() const noexcept;
};
```

- [ ] **Step 4: Run GREEN and refactor**

Expected all focused tests pass with no raw owning pointers in new code.

- [ ] **Step 5: Commit**

```powershell
git add DBProCompiler/DBPCompiler/SourceAssembler.* DBProCompiler/DBPCompiler/CompilationInput.* DBProCompiler/DBPCompiler/CMakeLists.txt tests/test_source_assembler.cpp tests/test_compilation_input.cpp tests/CMakeLists.txt
git commit -m "feat: add owned standalone compilation input"
```

### Task 4: Standalone CLI Integration

**Files:**
- Modify: `DBProCompiler/DBPCompiler/DBPCompiler.h`
- Modify: `DBProCompiler/DBPCompiler/DBPCompiler.cpp`
- Modify: `DBProCompiler/DBPCompiler/Main.cpp`
- Create: `tests/test_cli_project.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing subprocess test**

Create a multi-file project whose `final source` is absent. Invoke compiler with
`--json`; require `project_manifest` and `source_assembly` status events and no
missing `_Temp.dbsource` error.

- [ ] **Step 2: Run RED**

Expected current compiler fails because it loads `final source` directly.

- [ ] **Step 3: Add explicit preparation options**

```cpp
struct CompilationPreparationOptions {
    bool emitFinalSource = false;
    bool legacyFinalSource = false;
    std::uint64_t maxSourceBytes = 256ull * 1024 * 1024;
};
```

Implement `PrepareCompilationInput`; normal DBPro uses manifest assembly,
direct DBA uses its file, and only `--legacy-final-source` consumes the old
artifact. Never silently fall back.

- [ ] **Step 4: Adapt legacy loading at one boundary**

Make `LoadSource(const CompilationInput&)` provide the exact mutable,
NUL-terminated legacy buffer needed downstream while ownership remains explicit.

- [ ] **Step 5: Add CLI flags and JSON stage events**

Parse `--emit-final-source` and `--legacy-final-source`, reject conflicts, keep
logs on stderr, and return 1 for preparation failures.

- [ ] **Step 6: Run GREEN and commit**

```powershell
& .\out\build\windows-x86-debug\bin\Debug\dbp_tests.exe --gtest_filter=CompilationInputTest.*:CliProjectTest.*
git add DBProCompiler/DBPCompiler/DBPCompiler.* DBProCompiler/DBPCompiler/Main.cpp tests/test_cli_project.cpp tests/CMakeLists.txt
git commit -m "feat: compile DBPro projects without the editor"
```

### Task 5: Parser/Backend Stage Separation

**Files:**
- Create: `DBProCompiler/DBPCompiler/CodeGenerationSession.h`
- Create: `DBProCompiler/DBPCompiler/CodeGenerationSession.cpp`
- Create: `tests/test_codegen_session.cpp`
- Modify: `DBProCompiler/DBPCompiler/Statement.cpp`
- Modify: `DBProCompiler/DBPCompiler/Statement.h`
- Modify: `DBProCompiler/DBPCompiler/DBMWriter.cpp`
- Modify: `tests/test_ast.cpp`
- Modify: both CMake lists

- [ ] **Step 1: Write failing invariant tests**

With a fake `ICodeGenerator`, prove emit-before-begin returns `DBP2001`, Begin
calls `CreateASMHeader()` exactly once, and parsing `gloadreportstate=0` emits no
machine code.

- [ ] **Step 2: Run RED**

Expected parser invokes `CodeGenVisitor` early or regression subprocess crashes
with `0xc0000005`.

- [ ] **Step 3: Implement the session state machine**

```cpp
enum class CodeGenerationState { Created, Initialized, Finished, Failed };
class CodeGenerationSession {
public:
    explicit CodeGenerationSession(ICodeGenerator& generator);
    CodegenResult Begin();
    CodegenResult RequireInitialized(std::string_view operation) const;
    CodegenResult Finish();
};
```

Reject illegal transitions deterministically before touching machine memory.

- [ ] **Step 4: Remove parser-time emission**

Remove `CodeGenVisitor` construction from `CStatement::DoAssignment`. Until a
complete AST program survives into the emission stage, route simple assignments
through the existing legacy statement representation so exactly one production
path owns them.

- [ ] **Step 5: Begin the session at the backend boundary**

Use `CodeGenerationSession` in `CDBMWriter` at the existing header creation
point, then require initialized state before statement emission.

- [ ] **Step 6: Run GREEN and commit**

```powershell
& .\out\build\windows-x86-debug\bin\Debug\dbp_tests.exe --gtest_filter=AST*:CodeGeneration*:AssignmentRegressionTest.*
git add DBProCompiler/DBPCompiler/CodeGenerationSession.* DBProCompiler/DBPCompiler/Statement.* DBProCompiler/DBPCompiler/DBMWriter.cpp DBProCompiler/DBPCompiler/CMakeLists.txt tests/test_codegen_session.cpp tests/test_ast.cpp tests/CMakeLists.txt
git commit -m "fix: defer assignment emission until backend initialization"
```

### Task 6: Structured Diagnostics and FPSC Gate

**Files:**
- Modify: `DBProCompiler/DBPCompiler/DiagnosticEngine.*`
- Modify: `DBProCompiler/DBPCompiler/Main.cpp`
- Modify: `tests/test_diagnostics.cpp`
- Create: `tests/test_fpsc_projects.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing diagnostic tests**

Require stable codes `DBP1001..DBP1005` and `DBP2001`, valid JSON Lines,
escaped Windows paths, stderr-only logs, and nonzero failure exits.

- [ ] **Step 2: Run RED**

Expected existing output lacks stable structured fields.

- [ ] **Step 3: Centralize structured serialization**

Add code, stage, severity, message, project, source, line, and column to a typed
diagnostic. Serialize only through one JSON escaping function.

- [ ] **Step 4: Write FPSC integration tests before final fixes**

Resolve `DBP_FPSC_ROOT`, serialize the three builds, invoke direct CLI, require
exit zero, a `success` stage, and a freshly produced executable. Label these
tests `integration;fpsc` and use a CTest resource lock.

- [ ] **Step 5: Run GREEN and commit**

```powershell
$env:DBP_FPSC_ROOT='D:\GitHub-repo\FPS-Creator-Classic'
ctest --preset windows-x86-debug -L fpsc --output-on-failure
git add DBProCompiler/DBPCompiler/DiagnosticEngine.* DBProCompiler/DBPCompiler/Main.cpp tests/test_diagnostics.cpp tests/test_fpsc_projects.cpp tests/CMakeLists.txt
git commit -m "test: gate standalone compiler with FPS Creator"
```

### Task 7: Full Quality Gate and Documentation

**Files:**
- Modify: `README.md`
- Modify: `docs/incremental_modernization_compatibility_strategy.md`

- [ ] **Step 1: Document authoritative CLI behavior**

Document direct `.dbpro`, `--emit-final-source`, explicit legacy mode, stage
diagnostics, path rules, and exit codes.

- [ ] **Step 2: Run fresh debug verification**

```powershell
cmake --preset windows-x86-debug
cmake --build --preset windows-x86-debug
$env:DBP_FPSC_ROOT='D:\GitHub-repo\FPS-Creator-Classic'
ctest --preset windows-x86-debug --output-on-failure
```

Expected: build exit 0, zero failed tests, and no new warnings from new files.

- [ ] **Step 3: Run ASan verification**

```powershell
cmake --preset windows-x86-asan
cmake --build --preset windows-x86-asan
ctest --preset windows-x86-asan --output-on-failure
```

Expected: zero failed tests and no sanitizer report.

- [ ] **Step 4: Verify FPSC outputs**

Record size, timestamp, and SHA-256 for Screens, Map Editor, and Game; verify
each log ends in `success` with no error diagnostic.

- [ ] **Step 5: Commit docs and inspect scope**

```powershell
git add README.md docs/incremental_modernization_compatibility_strategy.md
git commit -m "docs: document standalone DBPro builds"
git status --short
git diff --check HEAD~7..HEAD
```

Confirm pre-existing user modifications were not staged or rewritten.
