[CmdletBinding()]
function ConvertFrom-TestDirective {
    [CmdletBinding()]
    param(
        [string[]]$FileContent
    )

    $result = [pscustomobject]@{
        CompileSuccess  = $true
        RuntimeOutputs  = [System.Collections.Generic.List[string]]::new()
        ExitCode        = 0
        TimeoutSeconds  = 5
        CompilerError   = $null
    }

    foreach ($line in $FileContent) {
        if ($line -match '^\s*REM\s+EXPECT:\s+(.*)$') {
            $directive = $Matches[1].Trim()
            if ($directive -eq 'COMPILE_SUCCESS') {
                $result.CompileSuccess = $true
            }
            elseif ($directive -eq 'COMPILE_FAIL') {
                $result.CompileSuccess = $false
            }
            elseif ($directive -match '^RUNTIME_OUTPUT\s+"(.*)"$') {
                $result.RuntimeOutputs.Add($Matches[1])
            }
            elseif ($directive -match '^EXIT_CODE\s+(\d+)$') {
                $result.ExitCode = [int]$Matches[1]
            }
            elseif ($directive -match '^TIMEOUT_SECONDS\s+(\d+)$') {
                $result.TimeoutSeconds = [int]$Matches[1]
            }
            elseif ($directive -match '^COMPILER_ERROR\s+"(.*)"$') {
                $result.CompilerError = $Matches[1]
            }
        }
    }

    return $result
}

Set-Alias -Name Parse-TestDirectives -Value ConvertFrom-TestDirective -ErrorAction SilentlyContinue

Export-ModuleMember -Function ConvertFrom-TestDirective, Parse-TestDirectives -Alias Parse-TestDirectives
