# GameFX Toolset Upgrade and Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Modernize the build configuration of `GameFX.vcxproj` to Visual Studio 2022 (`v143`), redirect absolute output paths to relative directories, build the DLL, and integrate it into the `FPS-Creator-Classic` toolchain package contract to successfully compile all projects in the compatibility matrix.

**Architecture:** We use MSBuild to upgrade the platform toolset and output directory. We then update the package manifest/sources schemas in FPS-Creator-Classic to declare the new target DLL, copy the built file, and run validation/matrix checks to confirm completion.

**Tech Stack:** MSBuild (v17), Pester (PowerShell testing framework), Git.

---

### Task 1: Declare the new GameFX artifact in FPS-Creator-Classic packaging manifests

**Files:**
- Modify: `D:/GitHub-repo/FPS-Creator-Classic/config/toolchain-package/fpsc-toolchain.package.json`
- Modify: `D:/GitHub-repo/FPS-Creator-Classic/config/toolchain-package/fpsc-toolchain.sources.json`

- [ ] **Step 1: Write the manifest declarations**
  Add the artifact to `fpsc-toolchain.package.json` under `artifacts` (around line 71):
  ```json
      { "id": "licensed-game-fx", "root": "fpscCompiler", "path": "plugins-licensed/DBProGameFX.dll", "role": "licensed-plugin", "required": true, "architecture": "x86", "provenanceClass": "fpsc-transition" }
  ```
  Add the mapping to `fpsc-toolchain.sources.json` under `mappings` (around line 125):
  ```json
      { "id": "licensed-game-fx", "sourceRoot": "dbproRelease", "sourcePath": "plugins-licensed/DBProGameFX.dll", "destinationPath": "plugins-licensed/DBProGameFX.dll", "sourceClass": "modern-output" }
  ```

- [ ] **Step 2: Run validation test to verify it fails**
  Run:
  ```powershell
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Import-Module Pester; Invoke-Pester -Path 'D:\GitHub-repo\FPS-Creator-Classic\tests\toolchain-package\ToolchainStaging.Tests.ps1'"
  ```
  Expected: FAIL with `TPKG_SOURCE_MISSING: Source for artifact 'licensed-game-fx' does not exist as a file.` (since the DLL does not exist in `dbproRelease\plugins-licensed\`).

- [ ] **Step 3: Commit the manifest declarations**
  ```bash
  git -C "D:\GitHub-repo\FPS-Creator-Classic" add config/toolchain-package/fpsc-toolchain.package.json config/toolchain-package/fpsc-toolchain.sources.json
  git -C "D:\GitHub-repo\FPS-Creator-Classic" commit -m "chore: declare licensed-game-fx artifact in toolchain manifests"
  ```

---

### Task 2: Upgrade GameFX project file to modern toolset and clean output paths

**Files:**
- Modify: `d:/GitHub-repo/Dark-Basic-Pro/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/GameFX/GameFX.vcxproj`

- [ ] **Step 1: Update PlatformToolset, WindowsTargetPlatformVersion, and OutputFile**
  Modify `GameFX.vcxproj` to set:
  - `<PlatformToolset>` to `v143` for configurations: `Release|Win32`, `Debug|Win32`, `DebugLM|Win32`.
  - Add `<WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>` to global `<PropertyGroup Label="Globals">` (around line 17).
  - Update output paths:
    - In `Debug|Win32` `<Link>` section, change `<OutputFile>` to `$(OutDir)DBProGameFX.dll`.
    - In `DebugLM|Win32` `<Link>` section, change `<OutputFile>` to `$(OutDir)DBProGameFX.dll`.
    - In `Release|Win32` `<Link>` section, keep `<OutputFile>` as `..\..\..\..\Install\Compiler\plugins-licensed\DBProGameFX.dll`.

- [ ] **Step 2: Run MSBuild to compile the project**
  Run:
  ```powershell
  & "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" "d:\GitHub-repo\Dark-Basic-Pro\Dark Basic Public Shared\Dark Basic Pro SDK\DarkSDKMore\GameFX\GameFX.vcxproj" /p:Configuration=Release /p:Platform=Win32 /p:WindowsTargetPlatformVersion=10.0
  ```
  Expected: PASS, resulting in the file `d:\GitHub-repo\Dark-Basic-Pro\Install\Compiler\plugins-licensed\DBProGameFX.dll`.

- [ ] **Step 3: Commit the GameFX project changes**
  ```bash
  git -C "d:\GitHub-repo\Dark-Basic-Pro" add "Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/GameFX/GameFX.vcxproj"
  git -C "d:\GitHub-repo\Dark-Basic-Pro" commit -m "build: upgrade GameFX platform toolset to v143 and clean output paths"
  ```

---

### Task 3: Stage toolchain package and verify validation tests pass

**Files:**
- Modify: None (operational verification)

- [ ] **Step 1: Run staging script**
  Run:
  ```powershell
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\GitHub-repo\FPS-Creator-Classic\scripts\toolchain-package\Stage-ToolchainPackage.ps1" -PackageSpecificationPath "D:\GitHub-repo\FPS-Creator-Classic\config\toolchain-package\fpsc-toolchain.package.json" -SourceSpecificationPath "D:\GitHub-repo\FPS-Creator-Classic\config\toolchain-package\fpsc-toolchain.sources.json" -RootMapping @("fpscCompiler=D:\GitHub-repo\FPS-Creator-Classic\FPS Creator Editor\dxsdk", "dbproRelease=d:\GitHub-repo\Dark-Basic-Pro\Install\Compiler", "fpscLive=D:\GitHub-repo\FPS-Creator-Classic\FPS Creator Editor\dxsdk", "dbproEffects=d:\GitHub-repo\Dark-Basic-Pro\Install\Compiler\effects") -StagingRoot "D:\GitHub-repo\FPS-Creator-Classic\artifacts\toolchain-package\staging" -CandidateId "fpsc-x86-output-contract-20260718" -OutputPath "D:\GitHub-repo\FPS-Creator-Classic\artifacts\toolchain-package\current-candidate-report.json"
  ```
  Expected: Successfully stages the candidate containing the modern `DBProGameFX.dll`.

- [ ] **Step 2: Run validation tests to verify they pass**
  Run:
  ```powershell
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Import-Module Pester; Invoke-Pester -Path 'D:\GitHub-repo\FPS-Creator-Classic\tests\toolchain-package'"
  ```
  Expected: PASS. All toolchain-package verification tests pass.

- [ ] **Step 3: Commit updated stage reports**
  Add and commit any updated JSON files or manifests in `D:\GitHub-repo\FPS-Creator-Classic\artifacts\toolchain-package\`.

---

### Task 4: Re-run Compatibility Matrix and verify expected outcome success

**Files:**
- Modify: `D:/GitHub-repo/FPS-Creator-Classic/artifacts/darkbasic-compatibility/fpsc-x86-output-contract-20260718.json` (report file)

- [ ] **Step 1: Execute Compatibility Harness for the matrix**
  Run:
  ```powershell
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\GitHub-repo\FPS-Creator-Classic\scripts\darkbasic-compatibility\Invoke-DarkBasicCompatibility.ps1" -SuitePath "D:\GitHub-repo\FPS-Creator-Classic\config\darkbasic-compatibility\fpsc-golden-projects.json" -CandidateRoot "D:\GitHub-repo\FPS-Creator-Classic\artifacts\toolchain-package\staging\fpsc-x86-output-contract-20260718" -RootMapping @("projects=D:\GitHub-repo\FPS-Creator-Classic\Dark Basic Pro Shared\Dark Basic Pro\Projects") -WorkspaceRoot "D:\GitHub-repo\FPS-Creator-Classic\artifacts\darkbasic-compatibility\workspaces" -OutputPath "D:\GitHub-repo\FPS-Creator-Classic\artifacts\darkbasic-compatibility\fpsc-x86-output-contract-20260718.json"
  ```
  Expected: Successful compilation of the other projects (`fpsc-game`, `fpsc-map-editor`, and `fpsc-screens`), matching their expected outcome `succeeded`. The script exits with code 0.

- [ ] **Step 2: Run DarkBasic compatibility Pester tests**
  Run:
  ```powershell
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Import-Module Pester; Invoke-Pester -Path 'D:\GitHub-repo\FPS-Creator-Classic\tests\darkbasic-compatibility'"
  ```
  Expected: PASS (22/22 tests pass).

- [ ] **Step 3: Commit the new matrix run result**
  ```bash
  git -C "D:\GitHub-repo\FPS-Creator-Classic" add artifacts/darkbasic-compatibility/fpsc-x86-output-contract-20260718.json
  git -C "D:\GitHub-repo\FPS-Creator-Classic" commit -m "test: update compatibility run contract JSON indicating 4/4 passing"
  ```
