# Requires -RunAsAdministrator (bootstrapper needs admin to install VS Build Tools)
[CmdletBinding()]
param(
    # Components required to build this repository (C++ desktop, MSBuild, CMake/ninja bits, Windows SDK)
    [string[]]$Component = @(
        'Microsoft.VisualStudio.Workload.VCTools',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        'Microsoft.VisualStudio.Component.Windows11SDK.26100',
        'Microsoft.Component.MSBuild'
    ),
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-VS2026InstallPath {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) { return $null }
    $path = & $vswhere -version '[18.0,19.0)' -products * -requires Microsoft.Component.MSBuild -property installationPath 2>$null
    if ($LASTEXITCODE -eq 0 -and $path) { return ($path | Select-Object -First 1) }
    return $null
}

if (Get-VS2026InstallPath) {
    Write-Host 'Visual Studio 2026 (Build Tools or full IDE) already present.'
    exit 0
}

$bootstrapper = Join-Path $env:TEMP 'vs_BuildTools_18.exe'
Write-Host 'Downloading Visual Studio 2026 Build Tools bootstrapper (channel release/18)...'
Invoke-WebRequest -Uri 'https://aka.ms/vs/18/release/vs_BuildTools.exe' -OutFile $bootstrapper

$arguments = @('--wait', '--norestart')
if ($Quiet) { $arguments += '--quiet' } else { $arguments += '--passive' }
foreach ($c in $Component) { $arguments += @('--add', $c) }

Write-Host "Installing Build Tools with: $($Component -join ', ')"
$proc = Start-Process -FilePath $bootstrapper -ArgumentList $arguments -Wait -PassThru
if ($proc.ExitCode -notin 0, 3010) {
    throw "vs_BuildTools.exe exited with code $($proc.ExitCode)"
}

if (-not (Get-VS2026InstallPath)) {
    throw 'Visual Studio 2026 Build Tools installation completed but vswhere cannot find it.'
}
Write-Host 'Visual Studio 2026 Build Tools installed successfully.'
exit 0
