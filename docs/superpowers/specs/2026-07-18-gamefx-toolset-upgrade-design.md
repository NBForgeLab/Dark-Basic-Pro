# Spec: GameFX Toolset Upgrade and Integration

## 1. Introduction & Context
The DarkBasic Pro Compatibility Suite has revealed that compilation of key FPS Creator projects (like `fpsc-game`, `fpsc-map-editor`, and `fpsc-screens`) fails due to unrecognized syntax commands:
- `LOAD STATIC OBJECTS`
- `SET STATIC PORTALS`

These commands belong to the `GameFX` SDK component. However, the current toolchain bundle lacks `DBProGameFX.dll`. An attempt to build `GameFX` from source failed because the legacy MSBuild project (`GameFX.vcxproj`) is locked to the Visual Studio 2012 toolset (`v110`), which is not present in the modern VS2022 build environment.

This specification details the modernization of `GameFX.vcxproj` to the `v143` (VS2022) toolset, the elimination of absolute output paths in favor of relative paths, and the integration of `DBProGameFX.dll` into the `FPS-Creator-Classic` packaging contracts.

---

## 2. Design Goals & Modern Standards
1. **Toolset Modernization:** Upgrade Platform Toolset to `v143` and target the installed Windows SDK (`10.0.26100.0` or generic `10.0`).
2. **Absolute Path Sanitization:** Remove absolute outputs (e.g. `F:\TGCSHARED\...` and `C:\Program Files\...`) and redirect builds to deterministic, clean directories:
   - Debug / DebugLM: local output directory `$(OutDir)DBProGameFX.dll`.
   - Release: relative path `..\..\..\..\Install\Compiler\plugins-licensed\DBProGameFX.dll`.
3. **Packaging Contract Integration:** Explicitly declare `DBProGameFX.dll` as part of the `fps-creator-classic-darkbasic-toolchain` contract in `FPS-Creator-Classic`.
4. **TDD / Test Validation:** Ensure all unit and integration tests pass, and re-run the compatibility matrix to confirm successful compilation of all projects.

---

## 3. Detailed Changes

### A. Dark-Basic-Pro Workspace
#### [MODIFY] [GameFX.vcxproj](file:///d:/GitHub-repo/Dark-Basic-Pro/Dark%20Basic%20Public%20Shared/Dark%20Basic%20Pro%20SDK/DarkSDKMore/GameFX/GameFX.vcxproj)
- Set `<PlatformToolset>` from `v110` to `v143` for all configurations (`Debug`, `DebugLM`, `Release`).
- Add `<WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>` to global property groups to target the modern Windows SDK.
- Update `<OutputFile>` tags:
  - `Debug` configuration: Change `F:\TGCSHARED\fpsc-reloaded\FPS Creator Files\DBProGameFX.dll` to `$(OutDir)DBProGameFX.dll`.
  - `DebugLM` configuration: Change `C:\Program Files (x86)\The Game Creators\FPS Creator\DBProGameFX.dll` to `$(OutDir)DBProGameFX.dll`.
  - `Release` configuration: Retain the relative path `..\..\..\..\Install\Compiler\plugins-licensed\DBProGameFX.dll`.

### B. FPS-Creator-Classic Workspace
#### [MODIFY] [fpsc-toolchain.package.json](file:///D:/GitHub-repo/FPS-Creator-Classic/config/toolchain-package/fpsc-toolchain.package.json)
- Add a new artifact declaration for `DBProGameFX.dll` in `plugins-licensed` folder:
  ```json
  {
    "id": "licensed-game-fx",
    "root": "fpscCompiler",
    "path": "plugins-licensed/DBProGameFX.dll",
    "role": "licensed-plugin",
    "required": true,
    "architecture": "x86",
    "provenanceClass": "fpsc-transition"
  }
  ```

#### [MODIFY] [fpsc-toolchain.sources.json](file:///D:/GitHub-repo/FPS-Creator-Classic/config/toolchain-package/fpsc-toolchain.sources.json)
- Add a source mapping for the new artifact:
  ```json
  {
    "id": "licensed-game-fx",
    "sourceRoot": "dbproRelease",
    "sourcePath": "plugins-licensed/DBProGameFX.dll",
    "destinationPath": "plugins-licensed/DBProGameFX.dll",
    "sourceClass": "modern-output"
  }
  ```

---

## 4. Verification Plan

### Automated Tests
1. **Pester Verification (Toolchain & Staging):** Run Pester tests in the FPS-Creator-Classic repository to verify that the modified package specifications and mapping are syntactically and logically correct:
   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Import-Module Pester; Invoke-Pester -Path 'D:\GitHub-repo\FPS-Creator-Classic\tests\toolchain-package'"
   ```
2. **Build Verification (GameFX DLL):** Execute MSBuild on `GameFX.vcxproj` for both `Release` and `Debug` configurations to confirm successful compilation without errors:
   ```powershell
   & "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" "d:\GitHub-repo\Dark-Basic-Pro\Dark Basic Public Shared\Dark Basic Pro SDK\DarkSDKMore\GameFX\GameFX.vcxproj" /p:Configuration=Release /p:Platform=Win32
   ```

### Compatibility Matrix Validation
1. Run the compatibility staging to update the staging candidate to include the built `DBProGameFX.dll`.
2. Run the Compatibility Harness suite:
   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Import-Module Pester; Invoke-Pester -Path 'D:\GitHub-repo\FPS-Creator-Classic\tests\darkbasic-compatibility'"
   ```
3. Verify that `fpsc-game`, `fpsc-map-editor`, and `fpsc-screens` cases transition from `compiler-rejected` (due to missing GameFX keywords) to their expected `succeeded` outcomes.
