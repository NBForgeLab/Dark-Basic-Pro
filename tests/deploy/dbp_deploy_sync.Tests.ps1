Describe "FPSC package deployment layout" -Tag 'deploy', 'package' {
    BeforeAll {
        # Pester v6 separates discovery from run per file, so file-scope state
        # must be established at run time to be visible inside It blocks.
        $script:SyncScript = (Resolve-Path -LiteralPath (
            Join-Path $PSScriptRoot "..\..\cmake\dbp_deploy_sync.cmake")).Path
        $script:ArtifactName = "FPSC-MapEditor"
        $script:DbpakName = "data-0123456789abcdef0123456789abcdef.dbpak"

        # DBPREF2 layout, matching the blob the packager really writes:
        #   0x00 "DBPREF2\0", 0x08 u64 entry count, 0x10 32 identifier bytes,
        #   0x30 u32 name length, then the NUL-terminated dbpak name.
        function script:New-DbpakRefBytes([string]$dbpakName) {
            $nameBytes = [System.Text.Encoding]::ASCII.GetBytes($dbpakName)
            $blob = New-Object System.Collections.Generic.List[byte]
            $blob.AddRange([System.Text.Encoding]::ASCII.GetBytes("DBPREF2"))
            $blob.Add([byte]0)
            $blob.AddRange([BitConverter]::GetBytes([uint64]1))
            for ($i = 0; $i -lt 32; $i++) { $blob.Add([byte]$i) }
            $blob.AddRange([BitConverter]::GetBytes([uint32]$nameBytes.Length))
            $blob.AddRange($nameBytes)
            $blob.Add([byte]0)
            return $blob.ToArray()
        }

        # Stands in for Projects/FPSCREATOR: the project directory holds the
        # trio the compiler emits, and Files/ is the game-data root beside it.
        function script:New-PackageFixture([string]$dbpakName = $script:DbpakName) {
            $root = Join-Path ([System.IO.Path]::GetTempPath()) (
                "dbppkg-" + [guid]::NewGuid().ToString("N"))
            New-Item -ItemType Directory -Path $root -Force | Out-Null
            Set-Content -LiteralPath (Join-Path $root "$($script:ArtifactName).exe") `
                -Value "exe" -NoNewline
            $refBytes = [byte[]](script:New-DbpakRefBytes $dbpakName)
            [System.IO.File]::WriteAllBytes(
                (Join-Path $root "$($script:ArtifactName).dbpakref"), $refBytes)
            Set-Content -LiteralPath (Join-Path $root $dbpakName) -Value "pak" -NoNewline

            $files = Join-Path $root "Files"
            New-Item -ItemType Directory -Path $files -Force | Out-Null
            Set-Content -LiteralPath (Join-Path $files "DBProCore.dll") `
                -Value "core" -NoNewline
            return $root
        }

        function script:Invoke-PackageMode {
            param([string]$ProjectDir, [switch]$WithFilesDir, [string]$DbpakName)
            $arguments = @(
                "-DMODE=package"
                "-DFPSC_PROJECT_DIR=$ProjectDir"
            )
            if ($WithFilesDir) {
                $arguments += "-DFPSC_FILES_DIR=$(Join-Path $ProjectDir 'Files')"
            }
            $arguments += "-DFPSC_ARTIFACT_NAME=$($script:ArtifactName)"
            $arguments += "-P"
            $arguments += $script:SyncScript
            & cmake @arguments 2>&1 | Out-Null
            return $LASTEXITCODE
        }
    }

    # String-matching checks on the cmake sources would stay green even while
    # the script keeps writing duplicates into the game-data directory, so these
    # run the real script against a fixture and assert on the filesystem.
    It "Leaves the game-data Files directory untouched" {
        $root = script:New-PackageFixture
        try {
            script:Invoke-PackageMode -ProjectDir $root -WithFilesDir |
                Should -Be 0 -Because "a complete package must pass the gate"

            $files = Join-Path $root "Files"
            @(Get-ChildItem -LiteralPath $files).Name |
                Should -Be @("DBProCore.dll") -Because `
                    "the parent resolves the child against the project directory (EditorDoc.cpp:298-300), so a copy under Files/ is never read and only drifts"
        }
        finally {
            Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    It "Does not require a Files directory" {
        $root = script:New-PackageFixture
        try {
            script:Invoke-PackageMode -ProjectDir $root |
                Should -Be 0 -Because `
                    "the compiler already emits the trio into its working directory, which is the project directory"
        }
        finally {
            Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    It "Fails when the dbpakref names a dbpak that is missing" {
        $root = script:New-PackageFixture
        try {
            Remove-Item -LiteralPath (Join-Path $root $script:DbpakName) -Force
            script:Invoke-PackageMode -ProjectDir $root |
                Should -Not -Be 0 -Because `
                    "an exe whose referenced package is absent cannot launch"
        }
        finally {
            Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}
