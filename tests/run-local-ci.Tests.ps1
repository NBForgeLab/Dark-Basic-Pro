Describe "Unified Local CI/CD Orchestrator Contract" -Tag 'ci-contract', 'pipeline' {
    BeforeAll {
        # Pester v6 separates discovery from run per file, so file-scope state
        # must be established at run time to be visible inside It blocks.
        $script:CIPath = Join-Path $PSScriptRoot "..\scripts\run-local-ci.ps1"
    }

    Context "Release and hardening gates" {
        It "Defines Debug, Release, and ASan configure/build/test presets" {
            $presetPath = Join-Path $PSScriptRoot "..\CMakePresets.json"
            $presets = Get-Content -LiteralPath $presetPath -Raw |
                ConvertFrom-Json
            (@($presets.configurePresets.name) -contains
                "windows-x64-debug") | Should -BeTrue -Because "a debug preset must exist"
            (@($presets.configurePresets.name) -contains
                "windows-x64-release") | Should -BeTrue -Because "a release preset must exist"
            (@($presets.configurePresets.name) -contains
                "windows-x64-asan") | Should -BeTrue -Because "an ASan preset must exist"
            (@($presets.buildPresets.name) -contains
                "windows-x64-release") | Should -BeTrue -Because "a release build preset must exist"
            (@($presets.testPresets.name) -contains
                "windows-x64-release") | Should -BeTrue -Because "a release test preset must exist"
        }

        It "Runs hosted conformance and has no developer-specific drive path" {
            $workflow = Get-Content -LiteralPath (
                Join-Path $PSScriptRoot `
                    "..\.github\workflows\windows-x64.yml") -Raw
            $localCI = Get-Content -LiteralPath $script:CIPath -Raw
            $workflow | Should -Match "Run language conformance" -Because "CI must run the conformance suite"
            $workflow | Should -Match "windows-x64-release" -Because "conformance must run on the release preset"
            $workflow | Should -Match "FailedCount" -Because "CI must gate on conformance failures"
            $localCI | Should -Match "FailedCount" -Because "the local orchestrator must gate on failures"
            $localCI | Should -Not -Match "[A-Za-z]:\\GitHub-repo" -Because "no developer-specific drive path may leak into the orchestrator"
        }

        It "Keeps Clang libFuzzer targets optional for normal MSVC builds" {
            $rootCMake = Get-Content -LiteralPath (
                Join-Path $PSScriptRoot "..\CMakeLists.txt") -Raw
            $options = Get-Content -LiteralPath (
                Join-Path $PSScriptRoot `
                    "..\cmake\ProjectOptions.cmake") -Raw
            $fuzzCMake = Get-Content -LiteralPath (
                Join-Path $PSScriptRoot "..\fuzz\CMakeLists.txt") -Raw
            $workflow = Get-Content -LiteralPath (
                Join-Path $PSScriptRoot `
                    "..\.github\workflows\windows-x64.yml") -Raw
            $smoke = Get-Content -LiteralPath (
                Join-Path $PSScriptRoot `
                    "..\fuzz\run-corpus-smoke.ps1") -Raw
            $options | Should -Match "option\(DBP_BUILD_FUZZERS" -Because "fuzzing must be opt-in"
            # The directory is entered when tests OR fuzzers are enabled, because
            # it also hosts the toolchain-independent corpus seed generator. The
            # libFuzzer targets themselves must still be gated on the option.
            $rootCMake | Should -Match "DBP_BUILD_FUZZERS" `
                -Because "entering the fuzz directory must depend on the fuzzer option"
            $rootCMake | Should -Match "add_subdirectory\(fuzz\)" `
                -Because "the fuzz directory must be reachable"
            $fuzzCMake | Should -Match "if\(DBP_BUILD_FUZZERS\)" `
                -Because "libFuzzer targets must stay opt-in"
            $fuzzCMake | Should -Match "fsanitize=fuzzer" -Because "fuzz targets must use libFuzzer"
            $fuzzCMake | Should -Match "requires Clang" -Because "fuzzing must require Clang"
            $workflow | Should -Match "fuzz-corpus-smoke" -Because "CI must smoke the fuzz corpus"
            $workflow | Should -Match "Run bounded valid and malformed corpus smoke" -Because "CI must run bounded corpus smoke"
            $smoke | Should -Match "dbp_fuzz_seed_generator" -Because "the smoke script must generate a seed corpus"
            $smoke | Should -Match "-runs=128" -Because "the smoke run must stay bounded"
        }

        It "Codegen fuzzing layer is wired into the contract" {
            $codegenSmoke = Get-Content -LiteralPath (
                Join-Path $PSScriptRoot "..\fuzz\run-codegen-corpus-smoke.ps1") -Raw
            $workflow = Get-Content -LiteralPath (
                Join-Path $PSScriptRoot `
                    "..\.github\workflows\windows-x64.yml") -Raw
            $codegenSmoke | Should -Match "dbp_codegen_seed_generator" `
                -Because "the codegen smoke script must generate a seed corpus"
            $codegenSmoke | Should -Match "-runs=128" `
                -Because "the codegen smoke run must stay bounded"
            $workflow | Should -Match "Run bounded codegen corpus smoke" `
                -Because "CI must run the codegen corpus smoke"
        }
    }

    # -----------------------------------------------------------------------
    # Behavioural contract for the codegen fuzzing layer.
    #
    # The checks above only prove that certain strings exist in certain files,
    # which stays green even when the tooling is completely broken (renamed
    # binary, generator writing nothing, corpus silently non-reproducible).
    # These run the real tools and assert on their output instead. They skip
    # when the binaries have not been built on this machine.
    # -----------------------------------------------------------------------
    Context "Codegen fuzzing layer behaviour" {
        BeforeAll {
            $script:SeedGenerator = $null
            $script:CorpusRunner = $null
            $configs = @(
                @{ Cfg = "Debug";   Dir = "windows-x64-debug" },
                @{ Cfg = "Release"; Dir = "windows-x64-release" }
            )
            foreach ($c in $configs) {
                $seed = Join-Path $PSScriptRoot (
                    "..\out\build\$($c.Dir)\bin\$($c.Cfg)\dbp_codegen_seed_generator.exe")
                $runner = Join-Path $PSScriptRoot (
                    "..\out\build\$($c.Dir)\bin\$($c.Cfg)\dbp_codegen_corpus_runner.exe")
                if ((Test-Path -LiteralPath $seed) -and (-not $script:SeedGenerator)) {
                    $script:SeedGenerator = (Resolve-Path -LiteralPath $seed).Path
                }
                if ((Test-Path -LiteralPath $runner) -and (-not $script:CorpusRunner)) {
                    $script:CorpusRunner = (Resolve-Path -LiteralPath $runner).Path
                }
            }
        }

        It "Seed generator produces a reproducible corpus (fixed seed)" {
            if (-not $script:SeedGenerator) {
                Set-ItResult -Skipped -Because "dbp_codegen_seed_generator is not built"
                return
            }
            $root = Join-Path ([System.IO.Path]::GetTempPath()) ("dbpgen-" + [guid]::NewGuid().ToString("N"))
            $dirA = Join-Path $root "a"
            $dirB = Join-Path $root "b"
            try {
                & $script:SeedGenerator $dirA | Out-Null
                $LASTEXITCODE | Should -Be 0 -Because "the generator must succeed"
                & $script:SeedGenerator $dirB | Out-Null
                $LASTEXITCODE | Should -Be 0 -Because "the generator must succeed"

                $filesA = @(Get-ChildItem -LiteralPath $dirA -Filter *.dba)
                $filesB = @(Get-ChildItem -LiteralPath $dirB -Filter *.dba)
                $filesA.Count | Should -BeGreaterThan 0 `
                    -Because "the generator must actually write corpus files"

                # Reproducibility is the whole point of the fixed seed: without it
                # a failing input cannot be replayed and CI diffs never stabilise.
                ($filesA.Name -join ',') | Should -Be ($filesB.Name -join ',') `
                    -Because "the same seed must yield the same file set"
                foreach ($f in $filesA) {
                    $other = Join-Path $dirB $f.Name
                    Test-Path -LiteralPath $other | Should -BeTrue
                    (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash |
                        Should -Be (Get-FileHash -LiteralPath $other -Algorithm SHA256).Hash `
                        -Because "$($f.Name) must be byte-identical across runs"
                }
            }
            finally {
                Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
            }
        }

        It "Seed corpus manifest describes every generated file" {
            if (-not $script:SeedGenerator) {
                Set-ItResult -Skipped -Because "dbp_codegen_seed_generator is not built"
                return
            }
            $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("dbpgen-" + [guid]::NewGuid().ToString("N"))
            try {
                & $script:SeedGenerator $dir | Out-Null
                $manifestPath = Join-Path $dir "manifest.json"
                Test-Path -LiteralPath $manifestPath | Should -BeTrue `
                    -Because "the generator must emit a manifest for triage"

                $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
                $manifest.files.Count | Should -BeGreaterThan 0
                $manifest.files.Count | Should -Be @(Get-ChildItem -LiteralPath $dir -Filter *.dba).Count `
                    -Because "every generated file must be described exactly once"

                # The manifest kinds are what let the harness judge a verdict; both
                # kinds must be present or the corpus cannot detect regressions.
                $kinds = $manifest.files | ForEach-Object { $_.kind } | Select-Object -Unique
                $kinds | Should -Contain "valid" `
                    -Because "well-formed programs are the regression baseline"
                $kinds | Should -Contain "mutant" `
                    -Because "malformed inputs are the robustness probe"

                foreach ($entry in $manifest.files) {
                    Test-Path -LiteralPath (Join-Path $dir $entry.file) | Should -BeTrue `
                        -Because "manifest entry '$($entry.file)' must exist on disk"
                }
            }
            finally {
                Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
            }
        }

        It "Corpus runner reports an explicit outcome, never a bare PASS/FAIL" {
            if (-not $script:CorpusRunner) {
                Set-ItResult -Skipped -Because "dbp_codegen_corpus_runner is not built"
                return
            }
            $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("dbprun-" + [guid]::NewGuid().ToString("N"))
            try {
                New-Item -ItemType Directory -Path $dir -Force | Out-Null
                # A single comment: parsed and emitted without exercising the
                # constructs that currently fault inside the compiler.
                $src = Join-Path $dir "harmless.dba"
                Set-Content -LiteralPath $src -Value "`r`n" -NoNewline

                $raw = & $script:CorpusRunner $src 2>$null
                $line = $raw | Where-Object { $_ -like '{*outcome*' } | Select-Object -First 1
                $line | Should -Not -BeNullOrEmpty `
                    -Because "the runner must print one machine-readable result line"
                ($line | ConvertFrom-Json).outcome |
                    Should -BeIn @("CLEAN", "REJECTED", "VIOLATION") `
                    -Because "the verdict depends on the input kind, so the runner must report the raw outcome"
            }
            finally {
                Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
            }
        }
    }

    Context "Dry Run & Parameter Validation" {
        It "Accepts standard parameters in dry-run mode and returns exit code 0" {
            $global:LASTEXITCODE = 0
            & pwsh -NoProfile -ExecutionPolicy Bypass `
                -File $script:CIPath `
                -DryRun `
                -Configuration Release `
                -SkipGolden `
                -SkipFPSTests
            $global:LASTEXITCODE | Should -Be 0 -Because "a valid dry run must succeed"
        }

        It "Rejects invalid configuration parameter values" {
            $global:LASTEXITCODE = 0
            & pwsh -NoProfile -ExecutionPolicy Bypass `
                -File $script:CIPath `
                -DryRun `
                -Configuration InvalidConfig
            $global:LASTEXITCODE | Should -Not -Be 0 -Because "an invalid configuration must fail validation"
        }
    }

    Context "Mock Failure Injection" {
        It "Fails with exit code 1 if mock compilation phase fails" {
            $global:LASTEXITCODE = 0
            & pwsh -NoProfile -ExecutionPolicy Bypass `
                -File $script:CIPath `
                -DryRun `
                -MockFailPhase "Compile"
            $global:LASTEXITCODE | Should -Be 1 -Because "a failed compile phase must abort the pipeline"
        }

        It "Fails with exit code 1 if mock conformance phase fails" {
            $global:LASTEXITCODE = 0
            & pwsh -NoProfile -ExecutionPolicy Bypass `
                -File $script:CIPath `
                -DryRun `
                -MockFailPhase "Conformance"
            $global:LASTEXITCODE | Should -Be 1 -Because "a failed conformance phase must abort the pipeline"
        }
    }

    Context "Real Execution (Fast Local Pipeline)" {
        It "Successfully compiles compiler and runs C++ unit tests" -Tag 'real-execution' {
            $global:LASTEXITCODE = 0
            # Run local CI skipping the integration components
            & pwsh -NoProfile -ExecutionPolicy Bypass `
                -File $script:CIPath `
                -Configuration Release `
                -SkipGolden `
                -SkipFPSTests
            $global:LASTEXITCODE | Should -Be 0 -Because "the fast local pipeline must build, test, and pass conformance"
        }
    }
}
