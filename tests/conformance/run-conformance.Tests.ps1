Import-Module (Join-Path $PSScriptRoot "DirectiveParser.psm1") -Force
Import-Module (Join-Path $PSScriptRoot "ConformanceRunner.psm1") -Force

# Pester v6 performs discovery and run as separate passes per file. Discovery
# state (the .dba corpus and its EXPECT expectations) therefore lives in
# BeforeDiscovery and reaches the data-driven tests through -ForEach.
# Items must be hashtables: Pester only expands -ForEach keys into variables
# for dictionaries, so PSCustomObject case objects would arrive with no
# accessible properties.
BeforeDiscovery {
    Import-Module (Join-Path $PSScriptRoot "DirectiveParser.psm1") -Force

    $conformanceCases = @()
    foreach ($file in (Get-ChildItem -Path $PSScriptRoot -Filter "*.dba" -Recurse)) {
        $lines = Get-Content -LiteralPath $file.FullName
        $hasExpectations = @($lines | Where-Object { $_ -match '^\s*REM\s+EXPECT:' }).Count -gt 0
        if (-not $hasExpectations) {
            continue
        }

        $conformanceCases += @{
            Name     = [string](Resolve-Path $file.FullName -Relative)
            File     = $file.FullName
            Expected = ConvertFrom-TestDirective -FileContent $lines
        }
    }
}

Describe "DarkBASIC Language Conformance" -Tag 'conformance', 'compiler-e2e' {
    BeforeAll {
        $script:CompilerPath = Get-ConformanceCompilerPath
        if ($null -eq $script:CompilerPath) {
            throw "DBPCompiler.exe not found in build outputs. Build project first."
        }

        if (-not [string]::IsNullOrWhiteSpace($env:DBP_CONFORMANCE_RUNTIME_ROOT)) {
            $script:RuntimeRoot = [IO.Path]::GetFullPath($env:DBP_CONFORMANCE_RUNTIME_ROOT)
        } else {
            $script:RuntimeRoot = [IO.Path]::GetDirectoryName($script:CompilerPath)
        }
    }

    It "Processes conformance case <Name>" -ForEach $conformanceCases {
        $result = Invoke-ConformanceCase `
            -SourceFile $File `
            -CompilerPath $script:CompilerPath `
            -RuntimeRoot $script:RuntimeRoot `
            -Expected $Expected

        if ($Expected.CompileSuccess) {
            $result.Compiled | Should -BeTrue -Because "the compiler must accept '$Name'"

            $result.AppExited | Should -BeTrue -Because "the headless runtime must self-exit for '$Name'"
            $result.AppExitCode | Should -Be $Expected.ExitCode -Because "the exit code of '$Name' must match its EXPECT directive"

            foreach ($outSub in $Expected.RuntimeOutputs) {
                $result.AppOutput |
                    Should -Match ([regex]::Escape($outSub)) -Because "'$outSub' must appear in the output of '$Name'"
            }
        } else {
            $result.Compiled | Should -BeFalse -Because "the compiler must reject '$Name'"

            if ($null -ne $Expected.CompilerError) {
                ($result.CompilerStdout + $result.CompilerStderr) |
                    Should -Match ([regex]::Escape($Expected.CompilerError)) -Because "the expected diagnostic must be reported for '$Name'"
            }
        }
    }

    It "Keeps the previous application runnable across an interrupted rebuild" {
        $workspace = Join-Path ([IO.Path]::GetTempPath()) (
            "dbp-publication-" + [Guid]::NewGuid().ToString("N"))
        $null = New-Item -ItemType Directory -Path $workspace
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
            $initial.ExitCode | Should -Be 0 -Because "the first build must succeed"
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
                Should -Match "DBP3190" -Because "the after-package interruption must report DBP3190"
            $packageInterrupted.HasExited | Should -BeTrue
            (Get-FileHash -LiteralPath $outputExe -Algorithm SHA256).Hash |
                Should -Be $executableHash -Because "the executable must not change after a packaging interruption"
            (Get-FileHash -LiteralPath $descriptor -Algorithm SHA256).Hash |
                Should -Be $descriptorHash -Because "the descriptor must not change after a packaging interruption"

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
                Should -Match "DBP3191" -Because "the post-executable interruption must report DBP3191"
            $interrupted.HasExited | Should -BeTrue
            (Get-FileHash -LiteralPath $descriptor -Algorithm SHA256).Hash |
                Should -Be $descriptorHash -Because "the descriptor must survive an interrupted rebuild"

            $application = Invoke-ProcessWithTimeout `
                -FileName $outputExe `
                -WorkingDirectory $workspace
            $application.ExitCode | Should -Be 0 -Because "the previously published application must still run"

            $completed = Invoke-ProcessWithTimeout `
                -FileName $script:CompilerPath `
                -Arguments $arguments `
                -WorkingDirectory $workspace
            $completed.ExitCode | Should -Be 0 -Because "the final build must succeed"
            @(Get-ChildItem -LiteralPath $workspace -Force |
                Where-Object Name -Like "*.dbp-backup-*").Count |
                Should -Be 0 -Because "no backup artifacts may remain after a clean rebuild"
        }
        finally {
            Remove-Item -LiteralPath $workspace -Recurse -Force `
                -ErrorAction SilentlyContinue
        }
    }
}
