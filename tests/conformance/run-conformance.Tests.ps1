Import-Module (Join-Path $PSScriptRoot "DirectiveParser.psm1") -Force

function Invoke-ProcessWithTimeout {
    param(
        [string]$FileName,
        [string]$Arguments = "",
        [string]$WorkingDirectory,
        [int]$TimeoutMs = 15000,
        [switch]$NoRedirect
    )
    try {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $FileName
        $psi.Arguments = $Arguments
        $psi.WorkingDirectory = $WorkingDirectory
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true
        $psi.EnvironmentVariables["PATH"] = $env:PATH

        $stdoutStr = ""
        $stderrStr = ""

        if (-not $NoRedirect) {
            $psi.RedirectStandardOutput = $true
            $psi.RedirectStandardError = $true
        }

        $p = [System.Diagnostics.Process]::Start($psi)

        $captureOutput = -not $NoRedirect
        if ($captureOutput) {
            $stdoutTask = $p.StandardOutput.ReadToEndAsync()
            $stderrTask = $p.StandardError.ReadToEndAsync()
        }

        $hasExitedBool = $p.WaitForExit($TimeoutMs)

        # Safety net: a well-formed staged runtime is headless (setup.ini is
        # required below), so a healthy app always exits by itself. If we still
        # have to kill it here, the run is already a failure — surfaced by the
        # hard HasExited assertion in the test body. Give the runtime a brief
        # grace window first so a healthy run that only just missed the timeout
        # still finishes its self-exit and flushes output.txt before we reap it.
        if (-not $hasExitedBool) {
            $hasExitedBool = $p.WaitForExit(2000)
            if (-not $hasExitedBool) {
                try { $p.Kill() } catch {}
            }
        }

        # Always drain redirected pipes after the process has finished.
        # An exited process guarantees the async reads will complete, but the
        # pipe pumps can lag process exit on fast runs, so wait here with a
        # bounded timeout and only then read the results. Killed processes also
        # reach this point with HasExited true.
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

        return [PSCustomObject]@{
            HasExited = $hasExitedBool
            ExitCode  = $exitCode
            Stdout    = $stdoutStr
            Stderr    = $stderrStr
        }
    }
    finally {
        if ($null -ne $p) {
            try { $p.Dispose() } catch {}
        }
    }
}

Describe "DarkBASIC Language Conformance Tests" {
    # Find built compiler
    $compilerCandidates = @()
    if (-not [string]::IsNullOrWhiteSpace(
            $env:DBP_CONFORMANCE_COMPILER)) {
        $compilerCandidates += $env:DBP_CONFORMANCE_COMPILER
    }
    $compilerCandidates += @(
        (Join-Path $PSScriptRoot "..\..\out\build\windows-x86-release\bin\Release\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\..\out\build\windows-x86-debug\bin\Debug\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\..\build\bin\Release\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\..\build\bin\Debug\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\..\bin\Release\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\..\bin\Debug\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\bin\Release\DBPCompiler.exe"),
        (Join-Path $PSScriptRoot "..\bin\Debug\DBPCompiler.exe")
    )
    $script:CompilerPath = $null
    foreach ($candidate in $compilerCandidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $script:CompilerPath = [IO.Path]::GetFullPath($candidate)
            break
        }
    }
    if ($null -eq $script:CompilerPath) {
        throw "DBPCompiler.exe not found in build outputs. Build project first."
    }
    if (-not [string]::IsNullOrWhiteSpace(
            $env:DBP_CONFORMANCE_RUNTIME_ROOT)) {
        $script:RuntimeRoot = [IO.Path]::GetFullPath(
            $env:DBP_CONFORMANCE_RUNTIME_ROOT)
    } else {
        $script:RuntimeRoot = [IO.Path]::GetDirectoryName($script:CompilerPath)
    }

    $testFiles = Get-ChildItem -Path $PSScriptRoot -Filter "*.dba" -Recurse
    foreach ($file in $testFiles) {
        $lines = Get-Content -LiteralPath $file.FullName
        $hasExpectations = $lines | Where-Object { $_ -match '^\s*REM\s+EXPECT:' }
        if (-not $hasExpectations) {
            continue
        }

        $relativeName = Resolve-Path $file.FullName -Relative

        It "Processes test case: $relativeName" {
            $expected = Parse-TestDirectives -FileContent $lines

            # Isolated staging
            $tempDir = [IO.Path]::GetTempPath()
            $guid = [Guid]::NewGuid().ToString("N")
            $workspaceName = "dbp-conformance-$guid"
            $workspace = Join-Path $tempDir $workspaceName
            $null = New-Item -ItemType Directory -Path $workspace -Force
            try {
                # Stage files into clean temp build directory to prevent DBP3105 packaging snapshot conflicts
                $tempCompilerDir = Join-Path ([IO.Path]::GetTempPath()) ("dbp-build-" + [Guid]::NewGuid().ToString("N"))
                $null = New-Item -ItemType Directory -Path $tempCompilerDir -Force

                Copy-Item -Path $file.FullName -Destination (Join-Path $tempCompilerDir $file.Name) -Force
                $helperFile = Join-Path $file.DirectoryName "helper.dba"
                if (Test-Path $helperFile) {
                    Copy-Item -Path $helperFile -Destination (Join-Path $tempCompilerDir "helper.dba") -Force
                }

                # Normalize line endings to CRLF for legacy compiler
                Get-ChildItem -Path $tempCompilerDir -Filter "*.dba" -Recurse | ForEach-Object {
                    $c = [System.IO.File]::ReadAllText($_.FullName)
                    $c = $c -replace "`r`n", "`n" -replace "`n", "`r`n"
                    [System.IO.File]::WriteAllText($_.FullName, $c, [System.Text.Encoding]::ASCII)
                }

                $outputExe = Join-Path $workspace "app.exe"

                # Generate temporary .dbpro project manifest
                $dbproContent = @(
                    "main=$($file.Name)"
                    "executable=app.exe"
                    "final source=_Temp.dbsource"
                ) -join "`r`n"
                $dbproFile = Join-Path $tempCompilerDir "project.dbpro"
                $dbproContent | Set-Content -LiteralPath $dbproFile -Encoding ASCII

                # Reset LASTEXITCODE
                $global:LASTEXITCODE = 0

                $compilerResult = $null
                try {
                    $compilerResult = Invoke-ProcessWithTimeout -FileName $script:CompilerPath `
                        -Arguments "--json --runtime-root `"$($script:RuntimeRoot)`" --output `"$outputExe`" `"$dbproFile`"" `
                        -WorkingDirectory $tempCompilerDir -TimeoutMs 30000
                } finally {
                    Remove-Item -Path $tempCompilerDir -Recurse -Force -ErrorAction SilentlyContinue
                }

                $stdout = $compilerResult.Stdout
                $stderr = $compilerResult.Stderr
                $compilerExitCode = $compilerResult.ExitCode
                $compileSucceeded = ($compilerResult.HasExited) -and ($compilerExitCode -eq 0) -and (Test-Path -LiteralPath $outputExe -PathType Leaf)

                if ($expected.CompileSuccess) {
                    if (-not $compileSucceeded) {
                        throw "Compilation failed: Output code: $compilerExitCode`nStdout: $stdout`nStderr: $stderr"
                    }

                    # Copy all plugin DLLs, root DLLs, setup.ini, and DarkEXE.exe into workspace
                    $runtimePlugins = Join-Path $script:RuntimeRoot "plugins"
                    if (Test-Path -LiteralPath $runtimePlugins) {
                        $targetPlugins = Join-Path $workspace "plugins"
                        $null = New-Item -ItemType Directory -Path $targetPlugins -Force -ErrorAction SilentlyContinue
                        Copy-Item -Path (Join-Path $runtimePlugins "*") -Destination $targetPlugins -Recurse -Force -ErrorAction SilentlyContinue
                        Copy-Item -Path (Join-Path $runtimePlugins "*.dll") -Destination $workspace -Force -ErrorAction SilentlyContinue
                    }
                    # setup.ini is mandatory for the headless run (without it
                    # the app would spin a window and never self-exit).
                    $setupIni = Join-Path $script:RuntimeRoot "setup.ini"
                    if (-not (Test-Path -LiteralPath $setupIni -PathType Leaf)) {
                        throw "setup.ini not found in runtime root '$script:RuntimeRoot'. Build or Deploy the runtime first."
                    }
                    Copy-Item -Path $setupIni -Destination $workspace -Force -ErrorAction SilentlyContinue
                    Copy-Item -Path (Join-Path $script:RuntimeRoot "*.dll") -Destination $workspace -Force -ErrorAction SilentlyContinue
                    $darkExe = Join-Path $script:RuntimeRoot "DarkEXE.exe"
                    if (Test-Path -LiteralPath $darkExe) {
                        Copy-Item -Path $darkExe -Destination $workspace -Force -ErrorAction SilentlyContinue
                    }

                    # Set up runtime PATH for execution
                    $oldPath = $env:PATH
                    $env:PATH = "$workspace;$(Join-Path $workspace 'plugins');$script:RuntimeRoot;$(Join-Path $script:RuntimeRoot 'plugins');" + $env:PATH
                    $appResult = $null
                    try {
                        $appResult = Invoke-ProcessWithTimeout `
                            -FileName $outputExe `
                            -Arguments "" `
                            -WorkingDirectory $workspace `
                            -TimeoutMs ($expected.TimeoutSeconds * 1000) `
                            -NoRedirect
                    } finally {
                        $env:PATH = $oldPath
                    }

                    if ($appResult.HasExited) {
                        $appResult.ExitCode | Should Be $expected.ExitCode
                    } else {
                        throw "Application did not exit within $($expected.TimeoutSeconds)s (headless runtime misconfigured)"
                    }

                    $appStdout = $appResult.Stdout
                    $outputTxtFile = Join-Path $workspace "output.txt"
                    if (-not (Test-Path -LiteralPath $outputTxtFile -PathType Leaf)) {
                        $outputTxtFile = Join-Path ([IO.Path]::GetFullPath((Join-Path $workspace ".."))) "output.txt"
                    }
                    if (Test-Path -LiteralPath $outputTxtFile -PathType Leaf) {
                        $appStdout += "`n" + (Get-Content -LiteralPath $outputTxtFile -Raw)
                    }

                    foreach ($outSub in $expected.RuntimeOutputs) {
                        if (-not ($appStdout -like "*$outSub*")) {
                            $dirFiles = (Get-ChildItem -LiteralPath $workspace | Select-Object -ExpandProperty Name) -join ", "
                            $dbpLogContent = ""
                            $dbpLogPath = Join-Path $workspace "dbp.log"
                            if (Test-Path $dbpLogPath) { $dbpLogContent = Get-Content $dbpLogPath -Raw }
                            throw "Runtime output expectation failed.`nExpected substring: '$outSub'`nActual output: '$appStdout'`nHasExited: $($appResult.HasExited)`nExitCode: $($appResult.ExitCode)`nWorkspace files: $dirFiles`ndbp.log: $dbpLogContent"
                        }
                    }
                }
                else {
                    if ($compileSucceeded) {
                        throw "Compilation succeeded but expected failure.`nStdout: $stdout"
                    }
                    if ($null -ne $expected.CompilerError) {
                        $allCompilerOutputs = $stdout + $stderr
                        ($allCompilerOutputs -like "*$($expected.CompilerError)*") | Should Be $true
                    }
                }
            }
            finally {
                if (Test-Path -LiteralPath $workspace) {
                    Remove-Item -LiteralPath $workspace -Recurse -Force -ErrorAction SilentlyContinue
                }
            }
        }
    }

    It "Keeps the previous application runnable across an interrupted rebuild" {
        $workspace = Join-Path ([IO.Path]::GetTempPath()) (
            "dbp-publication-" + [Guid]::NewGuid().ToString("N"))
        New-Item -ItemType Directory -Path $workspace | Out-Null
        try {
            "end`r`n" | Set-Content -LiteralPath (
                Join-Path $workspace "main.dba") -Encoding ASCII
            @(
                "main=main.dba"
                "executable=app.exe"
                "final source=_Temp.dbsource"
            ) -join "`r`n" | Set-Content -LiteralPath (
                Join-Path $workspace "project.dbpro") -Encoding ASCII
            $outputExe = Join-Path $workspace "app.exe"
            $descriptor = Join-Path $workspace "app.dbpakref"
            $arguments =
                "--json --runtime-root `"$($script:RuntimeRoot)`" " +
                "--output `"$outputExe`" " +
                "`"$(Join-Path $workspace 'project.dbpro')`""

            $initial = Invoke-ProcessWithTimeout `
                -FileName $script:CompilerPath `
                -Arguments $arguments `
                -WorkingDirectory $workspace
            $initial.ExitCode | Should Be 0
            $descriptorHash =
                (Get-FileHash -LiteralPath $descriptor -Algorithm SHA256).Hash
            $executableHash =
                (Get-FileHash -LiteralPath $outputExe -Algorithm SHA256).Hash

            @(
                "main=main.dba"
                "executable=app.exe"
                "final source=_Temp.dbsource"
                "app title=Interrupted rebuild"
            ) -join "`r`n" | Set-Content -LiteralPath (
                Join-Path $workspace "project.dbpro") -Encoding ASCII

            $packageInterruptWrapper =
                Join-Path $workspace "interrupt-after-package.cmd"
            @(
                "@echo off"
                "set DBP_TEST_FAIL_PUBLICATION_STAGE=after-package"
                "`"$($script:CompilerPath)`" $arguments"
                "if not errorlevel 1 exit /b 0"
                "exit /b 23"
            ) -join "`r`n" | Set-Content -LiteralPath (
                $packageInterruptWrapper) -Encoding ASCII
            $packageInterrupted = Invoke-ProcessWithTimeout `
                -FileName $env:ComSpec `
                -Arguments "/d /s /c `"`"$packageInterruptWrapper`"`"" `
                -WorkingDirectory $workspace
            ($packageInterrupted.Stdout + $packageInterrupted.Stderr) |
                Should Match "DBP3190"
            $packageInterrupted.HasExited | Should Be $true
            (Get-FileHash -LiteralPath $outputExe -Algorithm SHA256).Hash |
                Should Be $executableHash
            (Get-FileHash -LiteralPath $descriptor -Algorithm SHA256).Hash |
                Should Be $descriptorHash

            $interruptWrapper =
                Join-Path $workspace "interrupt-build.cmd"
            @(
                "@echo off"
                "set DBP_TEST_FAIL_PUBLICATION_STAGE=after-executable"
                "`"$($script:CompilerPath)`" $arguments"
                "if not errorlevel 1 exit /b 0"
                "exit /b 23"
            ) -join "`r`n" | Set-Content -LiteralPath (
                $interruptWrapper) -Encoding ASCII
            $interrupted = Invoke-ProcessWithTimeout `
                -FileName $env:ComSpec `
                -Arguments "/d /s /c `"`"$interruptWrapper`"`"" `
                -WorkingDirectory $workspace
            ($interrupted.Stdout + $interrupted.Stderr) |
                Should Match "DBP3191"
            $interrupted.HasExited | Should Be $true
            (Get-FileHash -LiteralPath $descriptor -Algorithm SHA256).Hash |
                Should Be $descriptorHash

            $application = Invoke-ProcessWithTimeout `
                -FileName $outputExe `
                -WorkingDirectory $workspace
            $application.ExitCode | Should Be 0

            $completed = Invoke-ProcessWithTimeout `
                -FileName $script:CompilerPath `
                -Arguments $arguments `
                -WorkingDirectory $workspace
            $completed.ExitCode | Should Be 0
            @(Get-ChildItem -LiteralPath $workspace -Force |
                Where-Object Name -Like "*.dbp-backup-*").Count |
                Should Be 0
        }
        finally {
            Remove-Item -LiteralPath $workspace -Recurse -Force `
                -ErrorAction SilentlyContinue
        }
    }
}
