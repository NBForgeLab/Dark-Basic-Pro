# Generic DBP Application Publisher Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a reusable transactional publisher and `dbp-publish.exe` CLI that turn a precompiled PE host plus an explicit asset manifest into an authenticated EXE + DBPAK v2 + descriptor tuple.

**Architecture:** Move executable/package/descriptor transaction ownership from compiler-specific `CFileBuilder` code into `DBProShared/Package/ApplicationPublisher`. Both the compiler and the new console tool call the same service. The CLI parses a strict JSON manifest, resolves a protected key file, publishes through the shared service, and emits stable NDJSON diagnostics.

**Tech Stack:** C++17, CMake 3.25+, Win32 file/resource APIs, CNG, Zstd 1.5.7, nlohmann/json 3.12.0 pinned release, GoogleTest, Pester.

---

## File Structure

- `DBProShared/Package/include/dbp/package/ApplicationPublisher.h`: public immutable request/result types and transaction interface.
- `DBProShared/Package/src/ApplicationPublisher.cpp`: staged host copy, authenticated package write, PE key injection, recovery backup, descriptor commit, and cleanup.
- `DBProShared/Package/include/dbp/package/PublicationCheckpoint.h`: injectable failure/cancellation boundary used by production and tests.
- `DBProShared/Package/src/PublicationCheckpoint.cpp`: no-op production checkpoint.
- `DBProTools/Publisher/PublisherManifest.h/.cpp`: strict JSON manifest model and parser.
- `DBProTools/Publisher/PublisherCli.h/.cpp`: typed CLI arguments, stable diagnostics, exit-code mapping.
- `DBProTools/Publisher/Main.cpp`: console entry point only.
- `DBProTools/Publisher/CMakeLists.txt`: `dbp-publish` target.
- `tests/test_application_publisher.cpp`: transaction and recovery tests.
- `tests/test_publisher_manifest.cpp`: JSON and path validation tests.
- `tests/test_publisher_cli.cpp`: argument and output contract tests.
- `tests/conformance/dbp-publish.Tests.ps1`: real-process publication and interruption tests.
- `DBProCompiler/DBPCompiler/FileBuilder.cpp`: delegate publication to the shared service and remove duplicate transaction logic.

### Task 1: Define the shared publication contract

**Files:**
- Create: `DBProShared/Package/include/dbp/package/PublicationCheckpoint.h`
- Create: `DBProShared/Package/include/dbp/package/ApplicationPublisher.h`
- Test: `tests/test_application_publisher.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing contract tests**

```cpp
TEST(ApplicationPublisherTest, RequestAndResultAreMoveSafe) {
    static_assert(std::is_move_constructible_v<ApplicationPublishRequest>);
    static_assert(std::is_move_constructible_v<ApplicationPublishResult>);
    static_assert(!std::is_copy_constructible_v<SecureBuffer>);
}

TEST(ApplicationPublisherTest, RejectsHostAndOutputPathAlias) {
    ApplicationPublishRequest request;
    request.hostExecutable = root_ / "host.exe";
    request.outputExecutable = request.hostExecutable;
    const auto result = publisher_.Publish(request, keys_);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, PackageErrorCode::PublicationFailed);
}
```

- [ ] **Step 2: Build the test target and verify RED**

Run:

```powershell
cmake --build --preset windows-x86-release --target dbp_tests
```

Expected: compilation fails because `ApplicationPublisher` does not exist.

- [ ] **Step 3: Add the minimal public types**

```cpp
enum class PublicationStage {
    PackagePublished,
    ExecutablePublished,
    DescriptorPublished,
};

class PublicationCheckpoint {
public:
    virtual ~PublicationCheckpoint() = default;
    virtual PackageResult<bool> Reach(PublicationStage stage) const = 0;
};

struct ApplicationPublishRequest {
    std::filesystem::path hostExecutable;
    std::filesystem::path outputExecutable;
    RuntimeMode mode = RuntimeMode::Application;
    KeyId keyId{};
    std::vector<PackageSourceEntry> entries;
    PackageLimits limits;
};

struct ApplicationPublishResult {
    std::filesystem::path executablePath;
    std::filesystem::path descriptorPath;
    PackageWriteResult package;
};
```

Declare `ApplicationPublisher::Publish(const ApplicationPublishRequest&,
const KeyProvider&)`.

- [ ] **Step 4: Run the focused tests and verify GREEN**

Run:

```powershell
cmake --build --preset windows-x86-release --target dbp_tests
.\out\build\windows-x86-release\bin\Release\dbp_tests.exe --gtest_filter=ApplicationPublisherTest.RequestAndResultAreMoveSafe:ApplicationPublisherTest.RejectsHostAndOutputPathAlias
```

Expected: 2 passed.

- [ ] **Step 5: Commit**

```powershell
git add DBProShared/Package tests
git commit -m "feat(package): define application publication contract"
```

### Task 2: Implement transactional application publication

**Files:**
- Create: `DBProShared/Package/src/ApplicationPublisher.cpp`
- Create: `DBProShared/Package/src/PublicationCheckpoint.cpp`
- Modify: `DBProShared/Package/CMakeLists.txt`
- Test: `tests/test_application_publisher.cpp`

- [ ] **Step 1: Add failing success and interruption tests**

Use a `RecordingCheckpoint` that returns failure at a selected
`PublicationStage`. Assert:

```cpp
ASSERT_TRUE(success);
EXPECT_TRUE(std::filesystem::is_regular_file(success.value().executablePath));
EXPECT_TRUE(std::filesystem::is_regular_file(success.value().descriptorPath));
EXPECT_TRUE(std::filesystem::is_regular_file(success.value().package.packagePath));

const auto beforeExe = Sha256File(outputExe);
const auto beforeDescriptor = Sha256File(descriptor);
checkpoint.FailAt(PublicationStage::PackagePublished);
ASSERT_FALSE(rebuild);
EXPECT_EQ(Sha256File(outputExe), beforeExe);
EXPECT_EQ(Sha256File(descriptor), beforeDescriptor);
```

Add a second failure after executable replacement and prove the old descriptor
still resolves a fallback key and the old package opens.

- [ ] **Step 2: Run focused tests and verify RED**

Run:

```powershell
cmake --build --preset windows-x86-release --target dbp_tests
.\out\build\windows-x86-release\bin\Release\dbp_tests.exe --gtest_filter=ApplicationPublisherTest.*
```

Expected: publication success/recovery tests fail.

- [ ] **Step 3: Implement the transaction**

Implement these ordered operations in `ApplicationPublisher.cpp`:

```cpp
ValidateRequest();
CopyHostToUniqueSiblingStageWithHandles();
writer_.Write({outputDirectory, request.keyId, request.entries, request.limits}, keys);
checkpoint_.Reach(PublicationStage::PackagePublished);
ReadPreviousDescriptorAndKeyIfValid();
InjectExecutablePackageKeys(stagedExe, request.keyId, masterKey, fallback);
FlushFileBuffers(stagedExeHandle);
ReplaceFileW(finalExe, stagedExe, backup, REPLACEFILE_WRITE_THROUGH, nullptr, nullptr);
checkpoint_.Reach(PublicationStage::ExecutablePublished);
WriteRuntimeDescriptorAtomically(descriptorPath, descriptor);
checkpoint_.Reach(PublicationStage::DescriptorPublished);
RemoveBackupAndPrivateStage();
```

On descriptor/checkpoint failure after replacement, restore with `ReplaceFileW`
or delete a first-build executable. Never overwrite immutable package names.

- [ ] **Step 4: Verify focused tests**

Expected: all `ApplicationPublisherTest.*` tests pass with no `.dbp-stage-*` or
`.dbp-backup-*` residue.

- [ ] **Step 5: Run ASAN**

```powershell
cmake --build --preset windows-x86-asan --target dbp_tests
ctest --preset windows-x86-asan --output-on-failure
```

Expected: 100% pass, no sanitizer diagnostics.

- [ ] **Step 6: Commit**

```powershell
git add DBProShared/Package tests
git commit -m "feat(package): publish application tuples transactionally"
```

### Task 3: Parse a strict publisher manifest

**Files:**
- Create: `DBProTools/Publisher/PublisherManifest.h`
- Create: `DBProTools/Publisher/PublisherManifest.cpp`
- Create: `tests/test_publisher_manifest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Pin nlohmann/json 3.12.0**

Add:

```cmake
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
)
FetchContent_MakeAvailable(nlohmann_json)
```

- [ ] **Step 2: Write failing manifest tests**

Cover a valid v1 document and rejection of:

```json
{"schemaVersion":2,"hostExecutable":"host.exe","outputExecutable":"game.exe","assets":[]}
```

```json
{"schemaVersion":1,"hostExecutable":"host.exe","outputExecutable":"game.exe","assets":[],"unknwon":true}
```

Also reject duplicate JSON keys, absolute package paths, traversal,
case-insensitive collisions, missing sources, more than configured limits,
and files larger than configured limits.

- [ ] **Step 3: Verify RED**

Expected: `PublisherManifestTest.*` cannot compile.

- [ ] **Step 4: Implement strict parsing**

Define:

```cpp
struct PublisherAsset {
    std::filesystem::path source;
    std::string destination;
    bool compress = true;
};

struct PublisherManifest {
    std::uint32_t schemaVersion = 1;
    std::filesystem::path hostExecutable;
    std::filesystem::path outputExecutable;
    RuntimeMode mode = RuntimeMode::Application;
    std::vector<PublisherAsset> assets;
};
```

Parse with `nlohmann::json::parse` using a callback that rejects duplicate
object keys. Compare every object key against an explicit allow-list. Resolve
file paths against the manifest parent and call `NormalizePackagePath` for
destinations.

- [ ] **Step 5: Verify GREEN and commit**

```powershell
.\out\build\windows-x86-release\bin\Release\dbp_tests.exe --gtest_filter=PublisherManifestTest.*
git add CMakeLists.txt DBProTools tests
git commit -m "feat(publisher): parse strict versioned manifests"
```

### Task 4: Add the `dbp-publish` console tool

**Files:**
- Create: `DBProTools/Publisher/PublisherCli.h`
- Create: `DBProTools/Publisher/PublisherCli.cpp`
- Create: `DBProTools/Publisher/Main.cpp`
- Create: `DBProTools/Publisher/CMakeLists.txt`
- Test: `tests/test_publisher_cli.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing CLI tests**

Assert:

```cpp
EXPECT_EQ(ParsePublisherArguments({L"dbp-publish", L"--help"}).kind,
          PublisherCommandKind::Help);
EXPECT_EQ(ParsePublisherArguments({
    L"dbp-publish", L"publish", L"manifest.json",
    L"--package-key-file", L"release.key", L"--json"}).value().json, true);
```

Reject missing values, duplicate options, raw key options, unknown options,
and manifest paths that are directories.

- [ ] **Step 2: Verify RED**

Expected: missing `PublisherCli` symbols.

- [ ] **Step 3: Implement the parser and runner**

Supported syntax:

```text
dbp-publish publish <manifest.json> --package-key-file <32-byte-file> [--json]
dbp-publish validate <manifest.json> [--json]
dbp-publish --help
dbp-publish --version
```

`Main.cpp` converts `CommandLineToArgvW`, calls `RunPublisher`, prints NDJSON
only to stdout in JSON mode, prints human diagnostics to stderr, and returns:
`0` success, `2` CLI/manifest, `3` input/key, `4` package, `5` publication,
`7` invariant.

- [ ] **Step 4: Verify tests and real help**

```powershell
cmake --build --preset windows-x86-release --target dbp-publish dbp_tests
.\out\build\windows-x86-release\bin\Release\dbp_tests.exe --gtest_filter=PublisherCliTest.*
.\out\build\windows-x86-release\bin\Release\dbp-publish.exe --help
```

Expected: tests pass; help exits 0 and contains no GUI prompt.

- [ ] **Step 5: Commit**

```powershell
git add CMakeLists.txt DBProTools tests
git commit -m "feat(publisher): add headless publication CLI"
```

### Task 5: Migrate `CFileBuilder` to the shared publisher

**Files:**
- Modify: `DBProCompiler/DBPCompiler/FileBuilder.cpp`
- Modify: `DBProCompiler/DBPCompiler/FileBuilder.h`
- Test: `tests/test_filebuilder.cpp`
- Test: `tests/conformance/run-conformance.Tests.ps1`

- [ ] **Step 1: Add failing source and process contracts**

Require `FileBuilder.cpp` to contain `ApplicationPublisher` and forbid direct
calls to `ReplaceFileW`, `MoveFileExW`, `InjectExecutablePackageKeys`, and
`WriteRuntimeDescriptorAtomically`.

Keep the existing interrupted-rebuild process test and assert unchanged EXE
SHA-256 after `after-package`.

- [ ] **Step 2: Verify RED**

Expected: source contract fails because transaction code remains in
`CFileBuilder`.

- [ ] **Step 3: Delegate finalization**

Replace the transaction body with:

```cpp
ApplicationPublishRequest request{
    m_stagedExecutablePath,
    m_finalExecutablePath,
    KindOfExecutable == 0 ? RuntimeMode::Application : RuntimeMode::Installer,
    m_packageKeyId,
    m_packageEntries,
    {},
};
EnvironmentPublicationCheckpoint checkpoint;
ApplicationPublisher publisher(crypto, compression, atomicPublisher, checkpoint);
const auto published = publisher.Publish(request, keys);
```

Map errors to existing `DBP3105`-`DBP3107` diagnostics and clear the compiler
stage only after success.

- [ ] **Step 4: Run unit and conformance tests**

```powershell
cmake --build --preset windows-x86-release
ctest --preset windows-x86-release --output-on-failure
$env:DBP_CONFORMANCE_COMPILER=(Resolve-Path out/build/windows-x86-release/bin/Release/DBPCompiler.exe)
$env:DBP_CONFORMANCE_RUNTIME_ROOT=(Resolve-Path Install/Compiler)
Invoke-Pester tests/conformance/run-conformance.Tests.ps1 -PassThru
```

Expected: all C++ tests and 12 conformance tests pass.

- [ ] **Step 5: Commit**

```powershell
git add DBProCompiler tests
git commit -m "refactor(compiler): share application publisher"
```

### Task 6: Add real-process security and recovery conformance

**Files:**
- Create: `tests/conformance/dbp-publish.Tests.ps1`
- Modify: `scripts/run-local-ci.ps1`
- Modify: `.github/workflows/windows-x86.yml`

- [ ] **Step 1: Write failing Pester scenarios**

Build a temporary copied host and assets, then cover:

- valid publication and launch from unrelated CWD;
- wrong-size and broad-DACL key rejection without path leakage;
- manifest traversal and case collision;
- missing asset and post-snapshot modification;
- failure after package and after executable;
- corrupt produced payload rejection;
- no PCK, loose `Files`, stage, or backup artifacts.

- [ ] **Step 2: Verify RED**

Expected: local CI does not yet invoke the suite.

- [ ] **Step 3: Wire the suite into local and hosted CI**

Pass exact publisher/compiler/runtime paths through environment variables.
Fail when Pester returns any failed or skipped security scenario.

- [ ] **Step 4: Run the full matrix**

```powershell
ctest --preset windows-x86-debug --output-on-failure
ctest --preset windows-x86-release --output-on-failure
ctest --preset windows-x86-asan --output-on-failure
.\scripts\run-local-ci.ps1 -Configuration Release
```

Expected: all dashboards green.

- [ ] **Step 5: Commit**

```powershell
git add tests scripts .github
git commit -m "test(publisher): gate secure process publication"
```

### Task 7: Deploy and document the tool contract

**Files:**
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `docs/README.md`
- Create: `docs/17_headless_application_publisher.md`
- Modify: `docs/baselines/2026-07-28-authenticated-package-v2.md`

- [ ] **Step 1: Add failing deployment contract**

Extend `RuntimeBundleIntegrationTest` to require `dbp-publish.exe` beside the
deployed compiler and verify `--version`.

- [ ] **Step 2: Verify RED**

Expected: deployment test cannot find the tool.

- [ ] **Step 3: Deploy and document**

Add a post-build copy of `$<TARGET_FILE:dbp-publish>` to the compiler host
bundle. Document the manifest schema, CLI, exit codes, NDJSON events, key ACL,
atomicity, and examples.

- [ ] **Step 4: Run final verification**

Run all Debug, Release, ASAN, conformance, compatibility, FPS smoke, and
`git diff --check`. Record exact fresh counts.

- [ ] **Step 5: Commit**

```powershell
git add DBProCompiler docs tests
git commit -m "docs(publisher): deploy and document headless publishing"
```
