[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$SkipGolden,
    [switch]$SkipFPSTests,
    [switch]$DryRun,
    [string]$MockFailPhase = ""
)

if ($DryRun) {
    if ($MockFailPhase -ne "") {
        Write-Error "Dry-run simulated failure in phase: $MockFailPhase"
        exit 1
    }
    Write-Host "Dry run verification passed."
    exit 0
}

exit 0
