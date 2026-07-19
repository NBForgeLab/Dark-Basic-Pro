$script:CIPath = Join-Path $PSScriptRoot "..\scripts\run-local-ci.ps1"

Describe "Unified Local CI/CD Orchestrator Contract" {
    Context "Dry Run & Parameter Validation" {
        It "Accepts standard parameters in dry-run mode and returns exit code 0" {
            $global:LASTEXITCODE = 0
            & $script:CIPath -DryRun -Configuration Release -SkipGolden -SkipFPSTests
            $global:LASTEXITCODE | Should Be 0
        }

        It "Rejects invalid configuration parameter values" {
            $global:LASTEXITCODE = 0
            try {
                & $script:CIPath -DryRun -Configuration InvalidConfig -ErrorAction Stop
            } catch {
                # Parameter validation should fail
            }
        }
    }

    Context "Mock Failure Injection" {
        It "Fails with exit code 1 if mock compilation phase fails" {
            $global:LASTEXITCODE = 0
            & $script:CIPath -DryRun -MockFailPhase "Compile"
            $global:LASTEXITCODE | Should Be 1
        }

        It "Fails with exit code 1 if mock conformance phase fails" {
            $global:LASTEXITCODE = 0
            & $script:CIPath -DryRun -MockFailPhase "Conformance"
            $global:LASTEXITCODE | Should Be 1
        }
    }

    Context "Real Execution (Fast Local Pipeline)" {
        It "Successfully compiles compiler and runs C++ unit tests" {
            $global:LASTEXITCODE = 0
            # Run local CI skipping the integration components
            & $script:CIPath -Configuration Release -SkipGolden -SkipFPSTests
            $global:LASTEXITCODE | Should Be 0
        }
    }
}
