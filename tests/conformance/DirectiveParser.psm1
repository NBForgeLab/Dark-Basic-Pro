Set-StrictMode -Version 3.0

<#
.SYNOPSIS
    Parses REM EXPECT: test directives from a DarkBASIC .dba conformance
    fixture into a structured expectation object.

.DESCRIPTION
    Supported directives:
      REM EXPECT: COMPILE_SUCCESS / COMPILE_FAIL
      REM EXPECT: RUNTIME_OUTPUT "text"
      REM EXPECT: EXIT_CODE <number>
      REM EXPECT: TIMEOUT_SECONDS <number>
      REM EXPECT: COMPILER_ERROR "text"
#>
function ConvertFrom-TestDirective {
    [CmdletBinding()]
    [OutputType([PSCustomObject])]
    param(
        # Real .dba fixtures contain blank lines; AllowEmptyString lets
        # Mandatory stay in place while accepting those elements.
        [Parameter(Mandatory)]
        [AllowEmptyString()]
        [string[]]$FileContent
    )

    $result = [pscustomobject]@{
        CompileSuccess = $true
        RuntimeOutputs = [System.Collections.Generic.List[string]]::new()
        ExitCode       = 0
        TimeoutSeconds = 5
        CompilerError  = $null
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

Export-ModuleMember -Function ConvertFrom-TestDirective
