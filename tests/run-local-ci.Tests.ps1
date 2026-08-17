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
            $rootCMake | Should -Match "if\(DBP_BUILD_FUZZERS\)" -Because "the root CMake must gate fuzz targets"
            $fuzzCMake | Should -Match "fsanitize=fuzzer" -Because "fuzz targets must use libFuzzer"
            $fuzzCMake | Should -Match "requires Clang" -Because "fuzzing must require Clang"
            $workflow | Should -Match "fuzz-corpus-smoke" -Because "CI must smoke the fuzz corpus"
            $workflow | Should -Match "Run bounded valid and malformed corpus smoke" -Because "CI must run bounded corpus smoke"
            $smoke | Should -Match "dbp_fuzz_seed_generator" -Because "the smoke script must generate a seed corpus"
            $smoke | Should -Match "-runs=128" -Because "the smoke run must stay bounded"
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
