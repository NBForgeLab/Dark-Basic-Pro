# DarkBASIC Language Conformance Test Suite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a robust and deterministic automated language conformance test runner for DarkBASIC using Pester, verifying syntax, compiler diagnostics, program exit codes, and runtime stdout.

**Architecture:** Build a self-contained test runner (`run-conformance.Tests.ps1`) that scans `.dba` files, parses inline `REM EXPECT:` directives, staging compiling tasks to isolated sandboxes, and verifies execution results against compiler/runtime criteria.

**Tech Stack:** PowerShell, Pester (Testing framework), DBPCompiler.exe.

---

### Task 1: Create initial conformance test files (Fixtures)

**Files:**
- Create: `tests/conformance/expressions/int_addition.dba`
- Create: `tests/conformance/errors/syntax_error.dba`

- [ ] **Step 1: Create the expressions addition test case**
  Create the file `tests/conformance/expressions/int_addition.dba` with self-contained success comments.
  ```darkbasic
  REM EXPECT: COMPILE_SUCCESS
  REM EXPECT: RUNTIME_OUTPUT "Sum: 15"
  REM EXPECT: EXIT_CODE 0
  REM EXPECT: TIMEOUT_SECONDS 5

  a = 5
  b = 10
  c = a + b
  print "Sum: " + c
  end
  ```

- [ ] **Step 2: Create the syntax error compile failure test case**
  Create the file `tests/conformance/errors/syntax_error.dba` with self-contained rejection comments.
  ```darkbasic
  REM EXPECT: COMPILE_FAIL
  REM EXPECT: COMPILER_ERROR "Syntax error"

  if a == 5
     print "Missing Then"
  endif
  ```

- [ ] **Step 3: Commit**
  ```bash
  git add tests/conformance/expressions/int_addition.dba tests/conformance/errors/syntax_error.dba
  git commit -m "test: add initial conformance test dba fixtures"
  ```

---

### Task 2: Implement Directive Parser logic

**Files:**
- Create: `tests/conformance/DirectiveParser.psm1`
- Create: `tests/conformance/DirectiveParser.Tests.ps1`

- [ ] **Step 1: Write the failing Pester test for parsing directives**
  Create `tests/conformance/DirectiveParser.Tests.ps1`.
  ```powershell
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

          $parsed.CompileSuccess | Should -Be $true
          $parsed.RuntimeOutputs | Should -Contain "Sum: 15"
          $parsed.ExitCode | Should -Be 0
          $parsed.TimeoutSeconds | Should -Be 5
          $parsed.CompilerError | Should -Be "Syntax error"
      }
  }
  ```

- [ ] **Step 2: Run test to verify it fails**
  Run: `Invoke-Pester -Path tests/conformance/DirectiveParser.Tests.ps1`
  Expected: FAIL (DirectiveParser.psm1 not found or function Parse-TestDirectives not defined)

- [ ] **Step 3: Write minimal implementation**
  Create `tests/conformance/DirectiveParser.psm1`.
  ```powershell
  function Parse-TestDirectives {
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

  Export-ModuleMember -Function Parse-TestDirectives
  ```

- [ ] **Step 4: Run test to verify it passes**
  Run: `Invoke-Pester -Path tests/conformance/DirectiveParser.Tests.ps1`
  Expected: PASS

- [ ] **Step 5: Commit**
  ```bash
  git add tests/conformance/DirectiveParser.psm1 tests/conformance/DirectiveParser.Tests.ps1
  git commit -m "feat: implement self-contained directive parser with tests"
  ```

---

### Task 3: Write Conformance Runner and compile execution logic

**Files:**
- Create: `tests/conformance/run-conformance.Tests.ps1`

- [ ] **Step 1: Write initial run-conformance.Tests.ps1 script skeleton**
  Write `tests/conformance/run-conformance.Tests.ps1` declaring the Pester framework skeleton.
  ```powershell
  BeforeAll {
      Import-Module (Join-Path $PSScriptRoot "DirectiveParser.psm1") -Force

      # Find built compiler
      $compilerCandidates = @(
          (Join-Path $PSScriptRoot "..\..\bin\Release\DBPCompiler.exe"),
          (Join-Path $PSScriptRoot "..\..\bin\Debug\DBPCompiler.exe"),
          (Join-Path $PSScriptRoot "..\..\build\bin\Release\DBPCompiler.exe"),
          (Join-Path $PSScriptRoot "..\..\build\bin\Debug\DBPCompiler.exe")
      )
      $script:CompilerPath = $null
      foreach ($candidate in $compilerCandidates) {
          if (Test-Path -LiteralPath $candidate -PathType Leaf) {
              $script:CompilerPath = [IO.Path]::GetFullPath($candidate)
              break
          }
      }
      if ($null -eq $script:CompilerPath) {
          throw "DBPCompiler.exe not found in build outputs. Build project first."
      }
      $script:RuntimeRoot = [IO.Path]::GetDirectoryName($script:CompilerPath)
  }

  Describe "DarkBASIC Language Conformance Tests" {
      $testFiles = Get-ChildItem -Path $PSScriptRoot -Filter "*.dba" -Recurse
      foreach ($file in $testFiles) {
          $relativeName = Resolve-Path $file.FullName -Relative

          It "Processes test case: $relativeName" {
              $lines = Get-Content -LiteralPath $file.FullName
              $expected = Parse-TestDirectives -FileContent $lines

              # Isolated staging
              $workspace = Join-Path [IO.Path]::GetTempPath() ("dbp-conformance-" + [Guid]::NewGuid().ToString("N"))
              $null = New-Item -ItemType Directory -Path $workspace -Force
              try {
                  $stagedSource = Join-Path $workspace $file.Name
                  Copy-Item -LiteralPath $file.FullName -Destination $stagedSource -Force
                  $outputExe = Join-Path $workspace "app.exe"

                  # Reset LASTEXITCODE
                  $global:LASTEXITCODE = 0

                  # Execute compiler
                  # Note: DBPCompiler might output logs asynchronously. We capture stdout/stderr.
                  $compilerArgs = @("--runtime-root", $script:RuntimeRoot, "--output", $outputExe, $stagedSource)
                  $processStartInfo = [System.Diagnostics.ProcessStartInfo]@{
                      FileName               = $script:CompilerPath
                      Arguments              = $compilerArgs -join " "
                      RedirectStandardOutput = $true
                      RedirectStandardError  = $true
                      UseShellExecute        = $false
                      CreateNoWindow         = $true
                      WorkingDirectory       = $workspace
                  }
                  $p = [System.Diagnostics.Process]::Start($processStartInfo)
                  $stdout = $p.StandardOutput.ReadToEnd()
                  $stderr = $p.StandardError.ReadToEnd()
                  $null = $p.WaitForExit(30000) # 30s compile timeout
                  $compilerExitCode = $p.ExitCode

                  $compileSucceeded = ($compilerExitCode -eq 0) -and (Test-Path -LiteralPath $outputExe -PathType Leaf)

                  if ($expected.CompileSuccess) {
                      if (-not $compileSucceeded) {
                          fail "Compilation failed: Output code: $compilerExitCode`nStdout: $stdout`nStderr: $stderr"
                      }

                      # Run compiled application
                      $appProcessStart = [System.Diagnostics.ProcessStartInfo]@{
                          FileName               = $outputExe
                          RedirectStandardOutput = $true
                          RedirectStandardError  = $true
                          UseShellExecute        = $false
                          CreateNoWindow         = $true
                          WorkingDirectory       = $workspace
                      }
                      $appProcess = [System.Diagnostics.Process]::Start($appProcessStart)
                      $appStdout = $appProcess.StandardOutput.ReadToEnd()
                      $appStderr = $appProcess.StandardError.ReadToEnd()
                      $hasExited = $appProcess.WaitForExit([int]($expected.TimeoutSeconds * 1000))

                      if (-not $hasExited) {
                          $appProcess.Kill()
                          fail "Application execution timed out after $($expected.TimeoutSeconds) seconds."
                      }

                      $appExitCode = $appProcess.ExitCode
                      $appExitCode | Should -Be $expected.ExitCode

                      foreach ($outSub in $expected.RuntimeOutputs) {
                          $appStdout | Should -Contain $outSub
                      }
                  }
                  else {
                      if ($compileSucceeded) {
                          fail "Compilation succeeded but expected failure.`nStdout: $stdout"
                      }
                      if ($null -ne $expected.CompilerError) {
                          $stdout + $stderr | Should -Contain $expected.CompilerError
                      }
                  }
              }
              finally {
                  if (Test-Path -LiteralPath $workspace) {
                      Remove-Item -LiteralPath $workspace -Recurse -Force -ErrorAction SilentlyContinue
                  }
              }
          }
      }
  }
  ```

- [ ] **Step 2: Run conformance tests to verify they pass**
  Run: `Invoke-Pester -Path tests/conformance/run-conformance.Tests.ps1`
  Expected: PASS (2 tests pass: `int_addition.dba` compiles & runs outputting `Sum: 15`, `syntax_error.dba` rejected with "Syntax error")

- [ ] **Step 3: Commit**
  ```bash
  git add tests/conformance/run-conformance.Tests.ps1
  git commit -m "feat: implement test runner and compile execution logic"
  ```
