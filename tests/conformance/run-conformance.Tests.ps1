Import-Module (Join-Path $PSScriptRoot "DirectiveParser.psm1") -Force

Describe "DarkBASIC Language Conformance Tests" {
    # Find built compiler
    $compilerCandidates = @(
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
    $script:RuntimeRoot = [IO.Path]::GetDirectoryName($script:CompilerPath)

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

                # Execute compiler using deadlock-proof files redirection and --json to suppress GUI dialogs
                $compilerArgs = @("--json", "--runtime-root", $script:RuntimeRoot, "--output", $outputExe, $dbproFile)
                
                $stdoutFile = [IO.Path]::GetTempFileName()
                $stderrFile = [IO.Path]::GetTempFileName()
                try {
                    $p = Start-Process -FilePath $script:CompilerPath -ArgumentList $compilerArgs `
                        -NoNewWindow -PassThru -RedirectStandardOutput $stdoutFile -RedirectStandardError $stderrFile
                    $hasExited = $p.WaitForExit(30000) # 30s compile timeout
                    if (-not $hasExited) {
                        $p.Kill()
                        throw "Compiler execution timed out after 30 seconds."
                    }
                    $compilerExitCode = $p.ExitCode
                    $stdout = Get-Content -LiteralPath $stdoutFile -Raw -ErrorAction SilentlyContinue
                    $stderr = Get-Content -LiteralPath $stderrFile -Raw -ErrorAction SilentlyContinue
                }
                finally {
                    Remove-Item -LiteralPath $stdoutFile -Force -ErrorAction SilentlyContinue
                    Remove-Item -LiteralPath $stderrFile -Force -ErrorAction SilentlyContinue
                }

                $compileSucceeded = ($compilerExitCode -eq 0) -and (Test-Path -LiteralPath $outputExe -PathType Leaf)

                if ($expected.CompileSuccess) {
                    if (-not $compileSucceeded) {
                        throw "Compilation failed: Output code: $compilerExitCode`nStdout: $stdout`nStderr: $stderr"
                    }

                    # Run compiled application using deadlock-proof files redirection
                    $appStdoutFile = [IO.Path]::GetTempFileName()
                    $appStderrFile = [IO.Path]::GetTempFileName()
                    try {
                        $appProcess = Start-Process -FilePath $outputExe -WorkingDirectory $workspace -NoNewWindow -PassThru `
                            -RedirectStandardOutput $appStdoutFile -RedirectStandardError $appStderrFile
                        $hasExited = $appProcess.WaitForExit([int]($expected.TimeoutSeconds * 1000))

                        if (-not $hasExited) {
                            $appProcess.Kill()
                            throw "Application execution timed out after $($expected.TimeoutSeconds) seconds."
                        }

                        $appExitCode = $appProcess.ExitCode
                        $appExitCode | Should Be $expected.ExitCode

                        $appStdout = Get-Content -LiteralPath $appStdoutFile -Raw -ErrorAction SilentlyContinue
                        $outputTxtFile = Join-Path $workspace "output.txt"
                        if (Test-Path -LiteralPath $outputTxtFile -PathType Leaf) {
                            $appStdout += "`n" + (Get-Content -LiteralPath $outputTxtFile -Raw)
                        }

                        foreach ($outSub in $expected.RuntimeOutputs) {
                            ($appStdout -like "*$outSub*") | Should Be $true
                        }
                    }
                    finally {
                        Remove-Item -LiteralPath $appStdoutFile -Force -ErrorAction SilentlyContinue
                        Remove-Item -LiteralPath $appStderrFile -Force -ErrorAction SilentlyContinue
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
}
