Import-Module (Join-Path $PSScriptRoot "DirectiveParser.psm1") -Force

Describe "Directive Parser" -Tag 'unit', 'conformance-harness' {
    It "parses the full EXPECT directive set" {
        $content = @(
            'REM EXPECT: COMPILE_SUCCESS'
            'REM EXPECT: RUNTIME_OUTPUT "Sum: 15"'
            'REM EXPECT: EXIT_CODE 0'
            'REM EXPECT: TIMEOUT_SECONDS 5'
            'REM EXPECT: COMPILER_ERROR "Syntax error"'
            'a = 5'
        )
        $parsed = ConvertFrom-TestDirective -FileContent $content

        $parsed.CompileSuccess | Should -BeTrue
        $parsed.RuntimeOutputs | Should -Contain "Sum: 15"
        $parsed.ExitCode | Should -Be 0
        $parsed.TimeoutSeconds | Should -Be 5
        $parsed.CompilerError | Should -Be "Syntax error"
    }

    It "defaults to compile-success fixtures with no directives" {
        $parsed = ConvertFrom-TestDirective -FileContent @('a = 5', 'print a')

        $parsed.CompileSuccess | Should -BeTrue
        $parsed.ExitCode | Should -Be 0
        $parsed.TimeoutSeconds | Should -Be 5
        $parsed.RuntimeOutputs.Count | Should -Be 0
        $parsed.CompilerError | Should -BeNullOrEmpty
    }

    It "honors COMPILE_FAIL and multiple RUNTIME_OUTPUT directives" {
        $parsed = ConvertFrom-TestDirective -FileContent @(
            'REM EXPECT: COMPILE_FAIL'
            'REM EXPECT: RUNTIME_OUTPUT "first line"'
            'REM EXPECT: RUNTIME_OUTPUT "second line"'
        )

        $parsed.CompileSuccess | Should -BeFalse
        $parsed.RuntimeOutputs | Should -Contain "first line"
        $parsed.RuntimeOutputs | Should -Contain "second line"
        $parsed.RuntimeOutputs.Count | Should -Be 2
    }

    It "ignores plain comments and preserves expectation defaults" {
        $parsed = ConvertFrom-TestDirective -FileContent @(
            'REM this is just a comment'
            'REM EXPECT: is not a directive line'
            'x = 1'
        )

        $parsed.CompileSuccess | Should -BeTrue
        $parsed.ExitCode | Should -Be 0
        $parsed.RuntimeOutputs.Count | Should -Be 0
    }

    It "overrides exit code and timeout from directives" {
        $parsed = ConvertFrom-TestDirective -FileContent @(
            'REM EXPECT: EXIT_CODE 3'
            'REM EXPECT: TIMEOUT_SECONDS 12'
        )

        $parsed.ExitCode | Should -Be 3
        $parsed.TimeoutSeconds | Should -Be 12
    }
}
