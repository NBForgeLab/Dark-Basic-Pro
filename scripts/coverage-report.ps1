<#
.SYNOPSIS
    Generates a code coverage report from MSVC or GCC/Clang coverage data.

.DESCRIPTION
    Scans the build directory for coverage data files produced by the
    DBP_ENABLE_COVERAGE instrumentation and produces a summary report.

    On MSVC builds the compiler emits *.cov files alongside object files.
    On GCC/Clang builds the toolchain emits *.gcda / *.gcno files.

    The script aggregates the discovered files, computes basic statistics,
    and writes the report in the requested format.

.PARAMETER BuildDirectory
    Path to the CMake build directory (e.g. out/build/windows-x86-coverage).
    Defaults to the first directory matching out/build/*coverage* if omitted.

.PARAMETER OutputFormat
    Report format: 'text' (human-readable) or 'json' (machine-parseable).
    Default: text

.PARAMETER OutputPath
    Optional file path to write the report to.  When omitted the report is
    written to the console (text) or to coverage-report.json in the repo root.

.EXAMPLE
    .\scripts\coverage-report.ps1
    # Auto-detect build directory, print text summary to console.

.EXAMPLE
    .\scripts\coverage-report.ps1 -BuildDirectory out/build/windows-x86-coverage -OutputFormat json
    # Generate JSON report for the coverage preset build.
#>

[CmdletBinding()]
param(
    [string]$BuildDirectory,

    [ValidateSet("text", "json")]
    [string]$OutputFormat = "text",

    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ── Resolve build directory ──────────────────────────────────────────
if (-not $BuildDirectory) {
    $candidate = Get-ChildItem -Path "out/build" -Directory -Filter "*coverage*" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($candidate) {
        $BuildDirectory = $candidate.FullName
    } else {
        Write-Error "No coverage build directory found. Pass -BuildDirectory or run a coverage build first."
        exit 1
    }
}

if (-not (Test-Path $BuildDirectory)) {
    Write-Error "Build directory does not exist: $BuildDirectory"
    exit 1
}

Write-Verbose "Scanning: $BuildDirectory"

# ── Discover coverage data files ─────────────────────────────────────

# MSVC .cov files
$covFiles = @(Get-ChildItem -Path $BuildDirectory -Recurse -Filter "*.cov" -File -ErrorAction SilentlyContinue)

# GCC/Clang .gcda / .gcno files
$gcdaFiles = @(Get-ChildItem -Path $BuildDirectory -Recurse -Filter "*.gcda" -File -ErrorAction SilentlyContinue)
$gcnoFiles = @(Get-ChildItem -Path $BuildDirectory -Recurse -Filter "*.gcno" -File -ErrorAction SilentlyContinue)

$totalCov  = $covFiles.Count
$totalGcda = $gcdaFiles.Count
$totalGcno = $gcnoFiles.Count
$totalFiles = $totalCov + $totalGcda + $totalGcno

# ── Compute basic size-based stats ───────────────────────────────────
$covSizeBytes  = 0
$gcdaSizeBytes = 0

if ($covFiles.Count -gt 0) {
    $covSizeBytes = ($covFiles | Measure-Object -Property Length -Sum).Sum
    if ($null -eq $covSizeBytes) { $covSizeBytes = 0 }
}
if ($gcdaFiles.Count -gt 0) {
    $gcdaSizeBytes = ($gcdaFiles | Measure-Object -Property Length -Sum).Sum
    if ($null -eq $gcdaSizeBytes) { $gcdaSizeBytes = 0 }
}

$totalSizeBytes = $covSizeBytes + $gcdaSizeBytes

# ── Build report object ──────────────────────────────────────────────
$timestamp = (Get-Date).ToUniversalTime().ToString("o")

$report = [PSCustomObject]@{
    Timestamp      = $timestamp
    BuildDirectory = (Resolve-Path $BuildDirectory).Path
    Toolchain      = if ($totalCov -gt 0) { "MSVC" } elseif ($totalGcda -gt 0) { "GCC/Clang" } else { "unknown" }
    CovFiles       = $totalCov
    GcdaFiles      = $totalGcda
    GcnoFiles      = $totalGcno
    TotalDataFiles = $totalFiles
    CovSizeBytes   = $covSizeBytes
    GcdaSizeBytes  = $gcdaSizeBytes
    TotalSizeBytes = $totalSizeBytes
}

# ── Output ───────────────────────────────────────────────────────────
switch ($OutputFormat) {
    "json" {
        if (-not $OutputPath) { $OutputPath = "coverage-report.json" }
        $report | ConvertTo-Json -Depth 4 | Set-Content -Path $OutputPath -Encoding utf8
        Write-Host "Coverage report written to $OutputPath"
    }
    "text" {
        $lines = @(
            "========================================"
            "  Code Coverage Report"
            "========================================"
            "  Timestamp       : $($report.Timestamp)"
            "  Build Directory : $($report.BuildDirectory)"
            "  Toolchain       : $($report.Toolchain)"
            "----------------------------------------"
            "  MSVC .cov files : $($report.CovFiles)"
            "  GCDA files      : $($report.GcdaFiles)"
            "  GCNO files      : $($report.GcnoFiles)"
            "  Total data files: $($report.TotalDataFiles)"
            "----------------------------------------"
            "  .cov size  (bytes): $($report.CovSizeBytes)"
            "  .gcda size (bytes): $($report.GcdaSizeBytes)"
            "  Total size (bytes) : $($report.TotalSizeBytes)"
            "========================================"
        )
        $text = $lines -join "`n"

        if ($OutputPath) {
            $text | Set-Content -Path $OutputPath -Encoding utf8
            Write-Host "Coverage report written to $OutputPath"
        } else {
            $text
        }
    }
}

# ── Summary exit message ─────────────────────────────────────────────
if ($totalFiles -eq 0) {
    Write-Warning "No coverage data files found. Ensure the build was instrumented with DBP_ENABLE_COVERAGE=ON and tests were executed."
} else {
    Write-Host "Found $totalFiles coverage data file(s)." -ForegroundColor Green
}
