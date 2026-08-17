Set-StrictMode -Version 3.0

<#
.SYNOPSIS
    Runs a child process with a hard timeout, polling for the DarkBASIC
    headless-runner "output.txt" completion marker.

.DESCRIPTION
    Every child process gets its own private TEMP/TMP so concurrent
    DBPCompiler runs cannot clash over %TEMP% (the historical DBP3105
    "package source changed/unavailable" race) or cross-read each other's
    output.txt.

    In Pester v6 the helper must live in a module: functions defined at
    the top level of a test file are not visible inside It blocks because
    discovery and run are separate passes per file.
#>
function Invoke-ProcessWithTimeout {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$FileName,
        [string]$Arguments = "",
        [string]$WorkingDirectory,
        [int]$TimeoutMs = 15000,
        [switch]$NoRedirect
    )

    $p = $null
    try {
        $psi = [System.Diagnostics.ProcessStartInfo]::new()
        $psi.FileName = $FileName
        $psi.Arguments = $Arguments
        $psi.WorkingDirectory = $WorkingDirectory
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true
        $psi.EnvironmentVariables["PATH"] = $env:PATH

        $privateTempRoot = Join-Path ([IO.Path]::GetTempPath()) (
            "dbp-isotemp-" + [Guid]::NewGuid().ToString("N"))
        $null = New-Item -ItemType Directory -Path $privateTempRoot -Force
        try {
            $psi.EnvironmentVariables["TEMP"] = $privateTempRoot
            $psi.EnvironmentVariables["TMP"]  = $privateTempRoot
            $psi.EnvironmentVariables["DBP_TEST_TEMPROOT"] = $privateTempRoot

            $stdoutStr = ""
            $stderrStr = ""

            if (-not $NoRedirect) {
                $psi.RedirectStandardOutput = $true
                $psi.RedirectStandardError = $true
            }

            # Pre-clean any existing output.txt files to ensure clean detection
            Get-ChildItem -Path $privateTempRoot -Filter "output.txt" -Recurse -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
            Get-ChildItem -Path $WorkingDirectory -Filter "output.txt" -Recurse -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

            $p = [System.Diagnostics.Process]::Start($psi)

            $captureOutput = -not $NoRedirect
            if ($captureOutput) {
                $stdoutTask = $p.StandardOutput.ReadToEndAsync()
                $stderrTask = $p.StandardError.ReadToEndAsync()
            }

            $pollIntervalMs = 50
            $elapsedMs = 0
            $hasExitedBool = $false

            while ($elapsedMs -lt $TimeoutMs) {
                if ($p.WaitForExit($pollIntervalMs)) {
                    $hasExitedBool = $true
                    break
                }
                $elapsedMs += $pollIntervalMs

                # DarkBASIC Pro GUI apps in headless test-runner mode signal
                # completion by writing output.txt; stop polling when it appears.
                $outTxt1 = Join-Path $WorkingDirectory "output.txt"
                $outTxt2 = Join-Path $privateTempRoot "output.txt"
                $outTxt3 = Get-ChildItem -Path $privateTempRoot -Filter "output.txt" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1

                if (Test-Path -LiteralPath $outTxt1 -PathType Leaf) {
                    Start-Sleep -Milliseconds 100
                    $hasExitedBool = $true
                    try { $p.Kill() } catch {}
                    break
                }
                if (Test-Path -LiteralPath $outTxt2 -PathType Leaf) {
                    Start-Sleep -Milliseconds 100
                    Copy-Item -LiteralPath $outTxt2 -Destination $outTxt1 -Force -ErrorAction SilentlyContinue
                    $hasExitedBool = $true
                    try { $p.Kill() } catch {}
                    break
                }
                if ($null -ne $outTxt3) {
                    Start-Sleep -Milliseconds 100
                    Copy-Item -LiteralPath $outTxt3.FullName -Destination $outTxt1 -Force -ErrorAction SilentlyContinue
                    $hasExitedBool = $true
                    try { $p.Kill() } catch {}
                    break
                }
            }

            if (-not $hasExitedBool) {
                try { $p.Kill() } catch {}
            }

            # Always drain redirected pipes after the process has finished.
            if ($captureOutput -and $hasExitedBool) {
                try { $p.WaitForExit() } catch {}
                try {
                    [System.Threading.Tasks.Task]::WaitAll(
                        @($stdoutTask, $stderrTask), 5000) | Out-Null
                    if ($stdoutTask.IsCompleted) {
                        $stdoutStr = $stdoutTask.Result
                    }
                    if ($stderrTask.IsCompleted) {
                        $stderrStr = $stderrTask.Result
                    }
                } catch {}
            }

            if (-not $hasExitedBool) {
                $exitCode = -1
                try { $p.WaitForExit() } catch {}
            } else {
                try { $exitCode = $p.ExitCode } catch { $exitCode = 0 }
            }

            return [pscustomobject]@{
                HasExited = $hasExitedBool
                ExitCode  = $exitCode
                Stdout    = $stdoutStr
                Stderr    = $stderrStr
            }
        }
        finally {
            Remove-Item -Path $privateTempRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    finally {
        if ($null -ne $p) {
            try { $p.Dispose() } catch {}
        }
    }
}

<#
.SYNOPSIS
    Locates a built DBPCompiler.exe from $env:DBP_CONFORMANCE_COMPILER or
    the standard build output layouts relative to the module directory.
#>
function Get-ConformanceCompilerPath {
    [CmdletBinding()]
    [OutputType([string])]
    param()

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($env:DBP_CONFORMANCE_COMPILER)) {
        $candidates += $env:DBP_CONFORMANCE_COMPILER
    }
    $candidates += @(
        (Join-Path $PSScriptRoot "..\..\out\build\windows-x64-release\bin\Release\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\..\out\build\windows-x64-release\bin\Debug\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\..\out\build\windows-x64-debug\bin\Debug\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\..\build\bin\Release\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\..\build\bin\Debug\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\..\bin\Release\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\..\bin\Debug\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\bin\Release\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\bin\Debug\DBPCompiler.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return [IO.Path]::GetFullPath($candidate)
        }
    }
    return $null
}

<#
.SYNOPSIS
    Stages a .dba conformance case into an isolated temp workspace, compiles
    it with the real compiler, and (for expected-success cases) runs the
    produced executable against the headless runtime.

.DESCRIPTION
    Returns a result object with Compiled / CompilerExitCode /
    CompilerStdout / CompilerStderr / AppExited / AppExitCode / AppOutput so
    the Pester It block can assert expectations cleanly. Hard infrastructure
    failures (compiler binary missing, setup.ini absent, headless app hang)
    throw so they surface as distinct test failures with the full context.
#>
function Invoke-ConformanceCase {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$SourceFile,
        [Parameter(Mandatory)]
        [string]$CompilerPath,
        [Parameter(Mandatory)]
        [string]$RuntimeRoot,
        [Parameter(Mandatory)]
        [PSCustomObject]$Expected
    )

    $tempDir = [IO.Path]::GetTempPath()
    $guid = [Guid]::NewGuid().ToString("N")
    $workspace = Join-Path $tempDir "dbp-conformance-$guid"
    $null = New-Item -ItemType Directory -Path $workspace -Force

    try {
        # Stage sources in a clean temp build dir to prevent DBP3105
        # packaging snapshot conflicts between concurrent compiler runs.
        $tempCompilerDir = Join-Path $tempDir ("dbp-build-" + [Guid]::NewGuid().ToString("N"))
        $null = New-Item -ItemType Directory -Path $tempCompilerDir -Force

        $sourceName = Split-Path -Leaf $SourceFile
        Copy-Item -LiteralPath $SourceFile -Destination (Join-Path $tempCompilerDir $sourceName) -Force
        $helperFile = Join-Path (Split-Path -Parent $SourceFile) "helper.dba"
        if (Test-Path -LiteralPath $helperFile) {
            Copy-Item -LiteralPath $helperFile -Destination (Join-Path $tempCompilerDir "helper.dba") -Force
        }

        # Normalize line endings to CRLF for the legacy compiler.
        Get-ChildItem -Path $tempCompilerDir -Filter "*.dba" -Recurse | ForEach-Object {
            $c = [System.IO.File]::ReadAllText($_.FullName)
            $c = $c -replace "`r`n", "`n" -replace "`n", "`r`n"
            [System.IO.File]::WriteAllText($_.FullName, $c, [System.Text.Encoding]::ASCII)
        }

        $outputExe = Join-Path $workspace "app.exe"
        $dbproContent = @(
            "main=$sourceName"
            "executable=app.exe"
            "final source=_Temp.dbsource"
        ) -join "`r`n"
        $dbproFile = Join-Path $tempCompilerDir "project.dbpro"
        $dbproContent | Set-Content -LiteralPath $dbproFile -Encoding ASCII

        $global:LASTEXITCODE = 0
        $compilerResult = $null
        try {
            $compilerResult = Invoke-ProcessWithTimeout -FileName $CompilerPath `
                -Arguments "--json --runtime-root `"$RuntimeRoot`" --output `"$outputExe`" `"$dbproFile`"" `
                -WorkingDirectory $tempCompilerDir -TimeoutMs 30000
        } finally {
            Remove-Item -Path $tempCompilerDir -Recurse -Force -ErrorAction SilentlyContinue
        }

        $compiled = ($compilerResult.HasExited) -and
                    ($compilerResult.ExitCode -eq 0) -and
                    (Test-Path -LiteralPath $outputExe -PathType Leaf)

        $appExited = $false
        $appExitCode = $null
        $appOutput = ""

        if ($compiled) {
            # Copy plugin DLLs, root DLLs, setup.ini, and DarkEXE.exe into the workspace.
            $runtimePlugins = Join-Path $RuntimeRoot "plugins"
            if (Test-Path -LiteralPath $runtimePlugins) {
                $targetPlugins = Join-Path $workspace "plugins"
                $null = New-Item -ItemType Directory -Path $targetPlugins -Force -ErrorAction SilentlyContinue
                Copy-Item -Path (Join-Path $runtimePlugins "*") -Destination $targetPlugins -Recurse -Force -ErrorAction SilentlyContinue
                Copy-Item -Path (Join-Path $runtimePlugins "*.dll") -Destination $workspace -Force -ErrorAction SilentlyContinue
            }
            # setup.ini is mandatory for the headless run: without it the app
            # spins up a window and never self-exits.
            $setupIni = Join-Path $RuntimeRoot "setup.ini"
            if (-not (Test-Path -LiteralPath $setupIni -PathType Leaf)) {
                throw "setup.ini not found in runtime root '$RuntimeRoot'. Build or Deploy the runtime first."
            }
            Copy-Item -LiteralPath $setupIni -Destination $workspace -Force -ErrorAction SilentlyContinue
            Copy-Item -Path (Join-Path $RuntimeRoot "*.dll") -Destination $workspace -Force -ErrorAction SilentlyContinue
            $darkExe = Join-Path $RuntimeRoot "DarkEXE.exe"
            if (Test-Path -LiteralPath $darkExe) {
                Copy-Item -LiteralPath $darkExe -Destination $workspace -Force -ErrorAction SilentlyContinue
            }

            # Give the app the runtime on PATH for plugin discovery.
            $oldPath = $env:PATH
            $env:PATH = "$workspace;$(Join-Path $workspace 'plugins');$RuntimeRoot;$(Join-Path $RuntimeRoot 'plugins');" + $env:PATH
            $appResult = $null
            try {
                $appResult = Invoke-ProcessWithTimeout `
                    -FileName $outputExe `
                    -WorkingDirectory $workspace `
                    -TimeoutMs ($Expected.TimeoutSeconds * 1000) `
                    -NoRedirect
            } finally {
                $env:PATH = $oldPath
            }

            if ($null -eq $appResult -or -not $appResult.HasExited) {
                throw "Application did not exit within $($Expected.TimeoutSeconds)s (headless runtime misconfigured)"
            }

            $appExited = $appResult.HasExited
            $appExitCode = $appResult.ExitCode
            $appOutput = $appResult.Stdout

            $outputTxtFile = Join-Path $workspace "output.txt"
            if (-not (Test-Path -LiteralPath $outputTxtFile -PathType Leaf)) {
                $outputTxtFile = Join-Path ([IO.Path]::GetFullPath((Join-Path $workspace ".."))) "output.txt"
            }
            if (Test-Path -LiteralPath $outputTxtFile -PathType Leaf) {
                $appOutput += "`n" + (Get-Content -LiteralPath $outputTxtFile -Raw)
            }
        }

        return [pscustomobject]@{
            Compiled         = $compiled
            CompilerExitCode = $compilerResult.ExitCode
            CompilerStdout   = $compilerResult.Stdout
            CompilerStderr   = $compilerResult.Stderr
            AppExited        = $appExited
            AppExitCode      = $appExitCode
            AppOutput        = $appOutput
        }
    }
    finally {
        if (Test-Path -LiteralPath $workspace) {
            Remove-Item -LiteralPath $workspace -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

Export-ModuleMember -Function Invoke-ProcessWithTimeout, Get-ConformanceCompilerPath, Invoke-ConformanceCase
