# Pester v6 shared bootstrap: this file is resolved from the repository root
# and dot-sourced once per test container (file) before its discovery/run.
# It guarantees the conformance helper modules are available even when a test
# file is executed through a runner that does not import them itself.
# Each test file still imports what it needs explicitly, so files stay
# self-contained for parallel runs.
Import-Module (Join-Path $PSScriptRoot "tests\conformance\DirectiveParser.psm1") -Force
Import-Module (Join-Path $PSScriptRoot "tests\conformance\ConformanceRunner.psm1") -Force
