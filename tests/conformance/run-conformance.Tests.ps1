Import-Module (Join-Path $PSScriptRoot "DirectiveParser.psm1") -Force

function Invoke-ProcessWithTimeout {
    param(
        [string]$FileName,
        [string]$Arguments = "",
        [string]$WorkingDirectory,
        [int]$TimeoutMs = 15000
    )
    $stdoutFile = [IO.Path]::GetTempFileName()
    $stderrFile = [IO.Path]::GetTempFileName()
    try {
        $splat = @{
            FilePath = $FileName
            WorkingDirectory = $WorkingDirectory
            RedirectStandardOutput = $stdoutFile
            RedirectStandardError = $stderrFile
            NoNewWindow = $true
            PassThru = $true
        }
        if (-not [string]::IsNullOrEmpty($Arguments)) {
            $splat["ArgumentList"] = $Arguments
        }
        $p = Start-Process @splat

        $timeoutSec = [int]($TimeoutMs / 1000)
        if ($timeoutSec -lt 1) { $timeoutSec = 1 }
        
        $hasExitedBool = $true
        try {
            Wait-Process -Id $p.Id -Timeout $timeoutSec -ErrorAction Stop
        } catch {
            $hasExitedBool = $false
            try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
        }

        $exitCode = 0
        if ($hasExitedBool) {
            try {
                if ($null -ne $p.ExitCode) {
                    $exitCode = $p.ExitCode
                }
            } catch {}
        } else {
            $exitCode = -1
        }

        $stdout = Get-Content -LiteralPath $stdoutFile -Raw -ErrorAction SilentlyContinue
        $stderr = Get-Content -LiteralPath $stderrFile -Raw -ErrorAction SilentlyContinue
        if ($null -eq $stdout) { $stdout = "" }
        if ($null -eq $stderr) { $stderr = "" }
        return [PSCustomObject]@{
            ExitCode = $exitCode
            HasExited = $hasExitedBool
            Stdout = $stdout
            Stderr = $stderr
        }
    }
    finally {
        Remove-Item -LiteralPath $stdoutFile -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $stderrFile -Force -ErrorAction SilentlyContinue
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
        $installedRuntime = Join-Path $PSScriptRoot "..\..\Install\Compiler"
        if (Test-Path -LiteralPath $installedRuntime -PathType Container) {
            $script:RuntimeRoot = [IO.Path]::GetFullPath($installedRuntime)
        } else {
            $script:RuntimeRoot =
                [IO.Path]::GetDirectoryName($script:CompilerPath)
        }
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
                # Copy all files from the test directory to support includes and auxiliary assets
                Copy-Item -Path (Join-Path $file.DirectoryName "*") -Destination $workspace -Recurse -Force

                # Normalize line endings of all staged .dba files to CRLF for the legacy compiler
                Get-ChildItem -Path $workspace -Filter "*.dba" -Recurse | ForEach-Object {
                    $rawContent = Get-Content -LiteralPath $_.FullName -Raw
                    $normalized = $rawContent -replace "\r?\n", "`r`n"
                    $normalized | Set-Content -LiteralPath $_.FullName -Encoding ASCII
                }

                $stagedSource = Join-Path $workspace $file.Name
                $outputExe = Join-Path $workspace "app.exe"

                # Generate temporary .dbpro project manifest
                $dbproContent = @(
                    "main=$($file.Name)"
                    "executable=app.exe"
                    "final source=_Temp.dbsource"
                ) -join "`r`n"
                $dbproFile = Join-Path $workspace "project.dbpro"
                $dbproContent | Set-Content -LiteralPath $dbproFile -Encoding ASCII

                # Reset LASTEXITCODE
                $global:LASTEXITCODE = 0

                # Invoke DBPCompiler with deadlock-proof file redirection
                $compilerResult = Invoke-ProcessWithTimeout -FileName $script:CompilerPath `
                    -Arguments "--json --runtime-root `"$($script:RuntimeRoot)`" --output `"$outputExe`" `"$dbproFile`"" `
                    -WorkingDirectory $workspace -TimeoutMs 15000

                $stdout = $compilerResult.Stdout
                $stderr = $compilerResult.Stderr
                $compilerExitCode = $compilerResult.ExitCode
                $compileSucceeded = ($compilerResult.HasExited) -and ($compilerExitCode -eq 0) -and (Test-Path -LiteralPath $outputExe -PathType Leaf)

                if ($expected.CompileSuccess) {
                    if (-not $compileSucceeded) {
                        throw "Compilation failed: Output code: $compilerExitCode`nStdout: $stdout`nStderr: $stderr"
                    }

                    # Run compiled application using deadlock-proof file redirection
                    $appResult = Invoke-ProcessWithTimeout -FileName $outputExe `
                        -Arguments "" -WorkingDirectory $workspace `
                        -TimeoutMs ([int]($expected.TimeoutSeconds * 1000))

                    if ($appResult.HasExited) {
                        $appResult.ExitCode | Should Be $expected.ExitCode
                    }

                    $appStdout = $appResult.Stdout
                    $outputTxtFile = Join-Path $workspace "output.txt"
                    if (Test-Path -LiteralPath $outputTxtFile -PathType Leaf) {
                        $appStdout += "`n" + (Get-Content -LiteralPath $outputTxtFile -Raw)
                    }

                    foreach ($outSub in $expected.RuntimeOutputs) {
                        ($appStdout -like "*$outSub*") | Should Be $true
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
            $packageInterrupted.Stdout |
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
            $interrupted.Stdout |
                Should Match "DBP3191"
            $interrupted.HasExited | Should Be $true
            (Get-FileHash -LiteralPath $outputExe -Algorithm SHA256).Hash |
                Should Be $executableHash
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
