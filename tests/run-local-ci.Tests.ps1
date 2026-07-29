$script:CIPath = Join-Path $PSScriptRoot "..\scripts\run-local-ci.ps1"

Describe "Unified Local CI/CD Orchestrator Contract" {
    Context "Release and hardening gates" {
        It "Defines Debug, Release, and ASan configure/build/test presets" {
            $presetPath = Join-Path $PSScriptRoot "..\CMakePresets.json"
            $presets = Get-Content -LiteralPath $presetPath -Raw |
                ConvertFrom-Json
            (@($presets.configurePresets.name) -contains
                "windows-x86-debug") | Should Be $true
            (@($presets.configurePresets.name) -contains
                "windows-x86-release") | Should Be $true
            (@($presets.configurePresets.name) -contains
                "windows-x86-asan") | Should Be $true
            (@($presets.buildPresets.name) -contains
                "windows-x86-release") | Should Be $true
            (@($presets.testPresets.name) -contains
                "windows-x86-release") | Should Be $true
        }

        It "Runs hosted conformance and has no developer-specific drive path" {
            $workflow = Get-Content -LiteralPath (
                Join-Path $PSScriptRoot `
                    "..\.github\workflows\windows-x86.yml") -Raw
            $localCI = Get-Content -LiteralPath $script:CIPath -Raw
            $workflow | Should Match "Run language conformance"
            $workflow | Should Match "windows-x86-release"
            $workflow | Should Match "FailedCount"
            $localCI | Should Match "FailedCount"
            $localCI | Should Not Match "[A-Za-z]:\\GitHub-repo"
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
                    "..\.github\workflows\windows-x86.yml") -Raw
            $smoke = Get-Content -LiteralPath (
                Join-Path $PSScriptRoot `
                    "..\fuzz\run-corpus-smoke.ps1") -Raw
            $options | Should Match "option\(DBP_BUILD_FUZZERS"
            $rootCMake | Should Match "if\(DBP_BUILD_FUZZERS\)"
            $fuzzCMake | Should Match "fsanitize=fuzzer"
            $fuzzCMake | Should Match "requires Clang"
            $workflow | Should Match "fuzz-corpus-smoke"
            $workflow | Should Match "Run bounded valid and malformed corpus smoke"
            $smoke | Should Match "dbp_fuzz_seed_generator"
            $smoke | Should Match "-runs=128"
        }
    }

    Context "Dry Run & Parameter Validation" {
        It "Accepts standard parameters in dry-run mode and returns exit code 0" {
            $global:LASTEXITCODE = 0
            & powershell.exe -NoProfile -ExecutionPolicy Bypass `
                -File $script:CIPath `
                -DryRun `
                -Configuration Release `
                -SkipGolden `
                -SkipFPSTests
            $global:LASTEXITCODE | Should Be 0
        }

        It "Rejects invalid configuration parameter values" {
            $global:LASTEXITCODE = 0
            & powershell.exe -NoProfile -ExecutionPolicy Bypass `
                -File $script:CIPath `
                -DryRun `
                -Configuration InvalidConfig
            $global:LASTEXITCODE | Should Not Be 0
        }
    }

    Context "Mock Failure Injection" {
        It "Fails with exit code 1 if mock compilation phase fails" {
            $global:LASTEXITCODE = 0
            & powershell.exe -NoProfile -ExecutionPolicy Bypass `
                -File $script:CIPath `
                -DryRun `
                -MockFailPhase "Compile"
            $global:LASTEXITCODE | Should Be 1
        }

        It "Fails with exit code 1 if mock conformance phase fails" {
            $global:LASTEXITCODE = 0
            & powershell.exe -NoProfile -ExecutionPolicy Bypass `
                -File $script:CIPath `
                -DryRun `
                -MockFailPhase "Conformance"
            $global:LASTEXITCODE | Should Be 1
        }
    }

    Context "Real Execution (Fast Local Pipeline)" {
        It "Successfully compiles compiler and runs C++ unit tests" {
            $global:LASTEXITCODE = 0
            # Run local CI skipping the integration components
            & powershell.exe -NoProfile -ExecutionPolicy Bypass `
                -File $script:CIPath `
                -Configuration Release `
                -SkipGolden `
                -SkipFPSTests
            $global:LASTEXITCODE | Should Be 0
        }
    }
}
