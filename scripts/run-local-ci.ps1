[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$SkipGolden,
    [switch]$SkipFPSTests,
    [switch]$DryRun,
    [string]$MockFailPhase = ""
)

# Helper function to check last exit code and abort
function Assert-Step($message, $phaseName) {
    if ($global:LASTEXITCODE -ne 0) {
        Write-Error "Error: $message"
        Write-Host "CI FAILED in phase: $phaseName" -ForegroundColor Red
        # Track status
        $script:PhaseStatuses[$phaseName] = "FAILED"
        Print-Dashboard -ExitOnFailure
    } else {
        $script:PhaseStatuses[$phaseName] = "PASSED"
    }
}

$script:PhaseStatuses = [ordered]@{
    "Compile"             = "PENDING"
    "C++Tests"            = "PENDING"
    "Conformance"         = "PENDING"
    "CompatibilityMatrix" = "PENDING"
    "FPSTests"            = "PENDING"
}

function Print-Dashboard([switch]$ExitOnFailure) {
    Write-Host "`n=========================================" -ForegroundColor Cyan
    Write-Host "      LOCAL CI/CD RUN DASHBOARD          " -ForegroundColor Cyan
    Write-Host "=========================================" -ForegroundColor Cyan
    foreach ($phase in $script:PhaseStatuses.Keys) {
        $status = $script:PhaseStatuses[$phase]
        $color = "Yellow"
        if ($status -eq "PASSED") { $color = "Green" }
        if ($status -eq "FAILED") { $color = "Red" }
        if ($status -eq "SKIPPED") { $color = "Gray" }
        Write-Host ("  {0,-20} : {1}" -f $phase, $status) -ForegroundColor $color
    }
    Write-Host "=========================================" -ForegroundColor Cyan
    if ($ExitOnFailure) {
        exit 1
    }
}

if ($DryRun) {
    if ($MockFailPhase -eq "Compile") {
        $script:PhaseStatuses["Compile"] = "FAILED"
        Print-Dashboard -ExitOnFailure
    }
    if ($MockFailPhase -eq "Conformance") {
        $script:PhaseStatuses["Compile"] = "PASSED"
        $script:PhaseStatuses["C++Tests"] = "PASSED"
        $script:PhaseStatuses["Conformance"] = "FAILED"
        Print-Dashboard -ExitOnFailure
    }
    Write-Host "Dry run verification passed."
    exit 0
}

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "   Unified DarkBASIC Pro Local CI/CD     " -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan

# Phase 1: Toolchain Build
Write-Host "`n[1/5] Building Toolchain ($Configuration)..." -ForegroundColor Yellow
$rootPath = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildPath = Join-Path $rootPath "build"

if (-not (Test-Path $buildPath)) {
    $null = New-Item -ItemType Directory -Path $buildPath -Force
}

# Run CMake Configuration
Write-Host "Configuring CMake project..."
$global:LASTEXITCODE = 0
& cmake -B $buildPath -S $rootPath -DCMAKE_BUILD_TYPE=$Configuration
Assert-Step "CMake configuration failed." "Compile"

# Run CMake Build
Write-Host "Building CMake targets..."
$global:LASTEXITCODE = 0
& cmake --build $buildPath --config $Configuration --parallel
Assert-Step "CMake build failed." "Compile"

# Phase 2: C++ Unit Tests
Write-Host "`n[2/5] Running C++ Unit Tests (dbp_tests)..." -ForegroundColor Yellow
$testExe = Join-Path $buildPath "bin\$Configuration\dbp_tests.exe"
if (-not (Test-Path $testExe)) {
    $testExe = Join-Path $buildPath "bin\dbp_tests.exe"
}

if (-not (Test-Path $testExe)) {
    $global:LASTEXITCODE = 1
    Assert-Step "Could not find dbp_tests.exe at $testExe" "C++Tests"
}

# Execute tests in its directory to resolve dependencies correctly
$testDir = [System.IO.Path]::GetDirectoryName($testExe)
Push-Location $testDir
try {
    $global:LASTEXITCODE = 0
    & $testExe
    Assert-Step "C++ unit tests failed." "C++Tests"
}
finally {
    Pop-Location
}

# Phase 3: Conformance Tests
Write-Host "`n[3/5] Running Language Conformance Tests..." -ForegroundColor Yellow
$conformancePath = Join-Path $rootPath "tests\conformance\run-conformance.Tests.ps1"
if (-not (Test-Path $conformancePath)) {
    $global:LASTEXITCODE = 1
    Assert-Step "Could not find conformance tests at $conformancePath" "Conformance"
}

$global:LASTEXITCODE = 0
Invoke-Pester -Path $conformancePath -ErrorAction SilentlyContinue
Assert-Step "Language conformance tests failed." "Conformance"

# Phase 4: Golden Compatibility Matrix
if ($SkipGolden) {
    Write-Host "`n[4/5] Skipping Golden Compatibility Matrix Phase." -ForegroundColor Gray
    $script:PhaseStatuses["CompatibilityMatrix"] = "SKIPPED"
} else {
    Write-Host "`n[4/5] Running Golden Compatibility Matrix Tests..." -ForegroundColor Yellow
    $compatScript = "D:\GitHub-repo\FPS-Creator-Classic\scripts\darkbasic-compatibility\Invoke-DarkBasicCompatibility.ps1"
    $compatSuite = "D:\GitHub-repo\FPS-Creator-Classic\config\darkbasic-compatibility\fpsc-golden-projects.json"
    $projectsRoot = "D:\GitHub-repo\FPS-Creator-Classic\Dark Basic Pro Shared\Dark Basic Pro\Projects"
    $compatWorkspace = Join-Path $env:TEMP "dbp-compat-workspace"
    $compatReport = Join-Path $env:TEMP "dbp-compat-report.json"
    $compatCandidate = Join-Path $env:TEMP "dbp-compat-candidate"

    if (-not (Test-Path $compatWorkspace)) {
        $null = New-Item -ItemType Directory -Path $compatWorkspace -Force
    }

    if (-not (Test-Path $compatScript)) {
        $global:LASTEXITCODE = 1
        Assert-Step "Could not find compatibility script at $compatScript. Ensure FPS-Creator-Classic is checked out sibling to Dark-Basic-Pro." "CompatibilityMatrix"
    }

    # 1. Clean and recreate candidate directory
    if (Test-Path $compatCandidate) {
        Remove-Item -Path $compatCandidate -Recurse -Force -ErrorAction SilentlyContinue
    }
    $null = New-Item -ItemType Directory -Path $compatCandidate -Force

    # 2. Copy active compiler template from FPS-Creator-Classic
    $templateCompilerDir = "D:\GitHub-repo\FPS-Creator-Classic\Dark Basic Pro Shared\Dark Basic Pro\Compiler"
    if (Test-Path $templateCompilerDir) {
        Copy-Item -Path (Join-Path $templateCompilerDir "*") -Destination $compatCandidate -Recurse -Force
    } else {
        $global:LASTEXITCODE = 1
        Assert-Step "Could not find active compiler template directory at $templateCompilerDir" "CompatibilityMatrix"
    }

    # 3. Overwrite with newly compiled binaries
    $candidateBinDir = Join-Path $buildPath "bin\$Configuration"
    Copy-Item -Path (Join-Path $candidateBinDir "DBPCompiler.exe") -Destination $compatCandidate -Force
    if (Test-Path (Join-Path $candidateBinDir "DarkEXE.exe")) {
        Copy-Item -Path (Join-Path $candidateBinDir "DarkEXE.exe") -Destination $compatCandidate -Force
    }

    # 4. Generate candidate.manifest.json
    $manifestPath = Join-Path $compatCandidate "candidate.manifest.json"
    Set-Content -LiteralPath $manifestPath -Value '{"candidateId":"local-ci"}' -Encoding ASCII

    $global:LASTEXITCODE = 0
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $compatScript `
        -SuitePath $compatSuite `
        -CandidateRoot $compatCandidate `
        -RootMapping "fpscProjects=$projectsRoot" `
        -WorkspaceRoot $compatWorkspace `
        -OutputPath $compatReport
    Assert-Step "Golden project compatibility checks failed." "CompatibilityMatrix"
}

# Phase 5: FPS Creator Unit Tests
if ($SkipFPSTests) {
    Write-Host "`n[5/5] Skipping FPS Creator Unit Tests Phase." -ForegroundColor Gray
    $script:PhaseStatuses["FPSTests"] = "SKIPPED"
} else {
    Write-Host "`n[5/5] Running FPS Creator Unit Tests..." -ForegroundColor Yellow
    $fpsTestsExe = "D:\GitHub-repo\FPS-Creator-Classic\FPS Creator Editor\tests\FPSTests\$Configuration\FPSTests.exe"
    
    if (-not (Test-Path $fpsTestsExe)) {
        $global:LASTEXITCODE = 1
        Assert-Step "Could not find FPSTests.exe at $fpsTestsExe. Build FPS Creator tests target first." "FPSTests"
    }

    $fpsTestsDir = [System.IO.Path]::GetDirectoryName($fpsTestsExe)
    Push-Location $fpsTestsDir
    try {
        $global:LASTEXITCODE = 0
        & $fpsTestsExe
        Assert-Step "FPS Creator unit tests failed." "FPSTests"
    }
    finally {
        Pop-Location
    }
}

Print-Dashboard
exit 0
