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
        exit 1
    }
}

if ($DryRun) {
    if ($MockFailPhase -eq "Compile") {
        Write-Error "Dry-run simulated compilation failure."
        exit 1
    }
    if ($MockFailPhase -eq "Conformance") {
        Write-Error "Dry-run simulated conformance failure."
        exit 1
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
    Write-Error "Could not find dbp_tests.exe at $testExe"
    exit 1
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

Write-Host "`nPhase 1 & 2 succeeded." -ForegroundColor Green
exit 0
