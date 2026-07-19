Import-Module (Join-Path $PSScriptRoot "DirectiveParser.psm1") -Force

Describe "Directive Parser" {
    It "correctly parses EXPECT directives from file content" {
        $content = @(
            'REM EXPECT: COMPILE_SUCCESS'
            'REM EXPECT: RUNTIME_OUTPUT "Sum: 15"'
            'REM EXPECT: EXIT_CODE 0'
            'REM EXPECT: TIMEOUT_SECONDS 5'
            'REM EXPECT: COMPILER_ERROR "Syntax error"'
            'a = 5'
        )
        $parsed = Parse-TestDirectives -FileContent $content

        $parsed.CompileSuccess | Should Be $true
        ($parsed.RuntimeOutputs -contains "Sum: 15") | Should Be $true
        $parsed.ExitCode | Should Be 0
        $parsed.TimeoutSeconds | Should Be 5
        $parsed.CompilerError | Should Be "Syntax error"
    }
}
