function Invoke-ProcessWithTimeout {
    param(
        [Parameter(Mandatory = $true)][string]$FileName,
        [string]$Arguments = "",
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [int]$TimeoutMs = 30000
    )
    $stdoutFile = [IO.Path]::GetTempFileName()
    $stderrFile = [IO.Path]::GetTempFileName()
    try {
        $start = @{
            FilePath = $FileName
            WorkingDirectory = $WorkingDirectory
            RedirectStandardOutput = $stdoutFile
            RedirectStandardError = $stderrFile
            NoNewWindow = $true
            PassThru = $true
        }
        if (-not [string]::IsNullOrEmpty($Arguments)) {
            $start.ArgumentList = $Arguments
        }
        $process = Start-Process @start
        try {
            Wait-Process -Id $process.Id `
                -Timeout ([Math]::Max(1, [int]($TimeoutMs / 1000))) `
                -ErrorAction Stop
            $hasExited = $true
        }
        catch {
            $hasExited = $false
            Stop-Process -Id $process.Id -Force `
                -ErrorAction SilentlyContinue
        }
        return [PSCustomObject]@{
            ExitCode = if ($hasExited) { $process.ExitCode } else { -1 }
            HasExited = $hasExited
            Stdout = [string](Get-Content -LiteralPath $stdoutFile `
                -Raw -ErrorAction SilentlyContinue)
            Stderr = [string](Get-Content -LiteralPath $stderrFile `
                -Raw -ErrorAction SilentlyContinue)
        }
    }
    finally {
        Remove-Item -LiteralPath $stdoutFile, $stderrFile `
            -Force -ErrorAction SilentlyContinue
    }
}

function Set-OwnerOnlyKeyAcl {
    param([Parameter(Mandatory = $true)][string]$Path)
    $owner = [Security.Principal.WindowsIdentity]::GetCurrent().User
    $acl = [Security.AccessControl.FileSecurity]::new()
    $acl.SetOwner($owner)
    $acl.SetAccessRuleProtection($true, $false)
    $rule = [Security.AccessControl.FileSystemAccessRule]::new(
        $owner,
        [Security.AccessControl.FileSystemRights]::FullControl,
        [Security.AccessControl.AccessControlType]::Allow)
    $acl.AddAccessRule($rule)
    Set-Acl -LiteralPath $Path -AclObject $acl
}

function Add-BroadReadAcl {
    param([Parameter(Mandatory = $true)][string]$Path)
    $icacls = Join-Path $env:SystemRoot "System32\icacls.exe"
    & $icacls $Path /grant "*S-1-1-0:(R)" /q
    if ($LASTEXITCODE -ne 0) {
        throw "Could not grant the broad-read ACL used by the test."
    }
}

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Contents
    )
    [IO.File]::WriteAllText(
        $Path,
        $Contents,
        [Text.UTF8Encoding]::new($false))
}

function Write-PublisherManifest {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [object[]]$Assets = @(
            @{
                source = "asset.bin"
                destination = "media/asset.bin"
                compress = $true
            })
    )
    $document = [ordered]@{
        schemaVersion = 1
        hostExecutable = "publisher host.exe"
        outputExecutable = "dist/game.exe"
        mode = "application"
        assets = $Assets
    } | ConvertTo-Json -Depth 8
    Write-Utf8NoBom -Path $Path -Contents $document
}

function New-PublisherFixture {
    param(
        [Parameter(Mandatory = $true)][string]$PublisherPath,
        [string]$HostSource = $PublisherPath
    )
    $root = Join-Path ([IO.Path]::GetTempPath()) (
        "dbp publish process " + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $root | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $root "dist") |
        Out-Null
    Copy-Item -LiteralPath $HostSource `
        -Destination (Join-Path $root "publisher host.exe")
    [IO.File]::WriteAllBytes(
        (Join-Path $root "asset.bin"),
        [byte[]](0x10, 0x20, 0x30, 0x40))
    $keyPath = Join-Path $root "private release key.bin"
    [IO.File]::WriteAllBytes($keyPath, [byte[]](0..31))
    Set-OwnerOnlyKeyAcl -Path $keyPath
    $manifestPath = Join-Path $root "publisher.json"
    Write-PublisherManifest -Path $manifestPath
    return [PSCustomObject]@{
        Root = $root
        Key = $keyPath
        Manifest = $manifestPath
        Executable = Join-Path $root "dist\game.exe"
        Descriptor = Join-Path $root "dist\game.dbpakref"
    }
}

function Invoke-Publish {
    param(
        [Parameter(Mandatory = $true)][string]$PublisherPath,
        [Parameter(Mandatory = $true)]$Fixture,
        [string]$KeyPath = $Fixture.Key
    )
    Invoke-ProcessWithTimeout `
        -FileName $PublisherPath `
        -Arguments (
            "publish `"$($Fixture.Manifest)`" " +
            "--package-key-file `"$KeyPath`" --json") `
        -WorkingDirectory $Fixture.Root
}

Describe "Headless DBP publisher process security" {
    BeforeAll {
        $script:OriginalPublicationFailureStage =
            $env:DBP_TEST_FAIL_PUBLICATION_STAGE
        $script:OriginalCompilerCleanupFailureStage =
            $env:DBP_TEST_FAIL_COMPILER_STAGE_CLEANUP
        $script:OriginalManifestSnapshotGate =
            $env:DBP_TEST_MANIFEST_SNAPSHOT_GATE

        if (-not [string]::IsNullOrWhiteSpace(
                $env:DBP_PUBLISHER_EXE)) {
            if (-not (Test-Path -LiteralPath $env:DBP_PUBLISHER_EXE `
                    -PathType Leaf)) {
                throw "Configured DBP_PUBLISHER_EXE does not name a file."
            }
            $script:PublisherPath =
                [IO.Path]::GetFullPath($env:DBP_PUBLISHER_EXE)
        } else {
            $script:PublisherPath = @(
                (Join-Path $PSScriptRoot "..\..\out\build\windows-x86-release\bin\Release\dbp-publish.exe"),
                (Join-Path $PSScriptRoot "..\..\out\build\windows-x86-debug\bin\Debug\dbp-publish.exe")
            ) |
                Where-Object {
                    Test-Path -LiteralPath $_ -PathType Leaf
                } |
                Select-Object -First 1
        }
        if ($null -eq $script:PublisherPath) {
            throw "dbp-publish.exe was not found. Build it before this suite."
        }
        $script:PublisherPath =
            [IO.Path]::GetFullPath($script:PublisherPath)

        if (-not [string]::IsNullOrWhiteSpace(
                $env:DBP_PACKAGE_PROBE_EXE)) {
            if (-not (Test-Path -LiteralPath $env:DBP_PACKAGE_PROBE_EXE `
                    -PathType Leaf)) {
                throw "Configured DBP_PACKAGE_PROBE_EXE does not name a file."
            }
            $script:PackageProbePath =
                [IO.Path]::GetFullPath($env:DBP_PACKAGE_PROBE_EXE)
        } else {
            $script:PackageProbePath = @(
                (Join-Path $PSScriptRoot "..\..\out\build\windows-x86-release\bin\Release\dbp-package-probe.exe"),
                (Join-Path $PSScriptRoot "..\..\out\build\windows-x86-debug\bin\Debug\dbp-package-probe.exe")
            ) |
                Where-Object {
                    Test-Path -LiteralPath $_ -PathType Leaf
                } |
                Select-Object -First 1
        }
        if ($null -eq $script:PackageProbePath) {
            throw "dbp-package-probe.exe was not found. Build it before this suite."
        }
        $script:PackageProbePath =
            [IO.Path]::GetFullPath($script:PackageProbePath)

        $env:DBP_TEST_FAIL_PUBLICATION_STAGE = $null
        $env:DBP_TEST_FAIL_COMPILER_STAGE_CLEANUP = $null
        $env:DBP_TEST_MANIFEST_SNAPSHOT_GATE = $null
    }

    AfterAll {
        $env:DBP_TEST_FAIL_PUBLICATION_STAGE =
            $script:OriginalPublicationFailureStage
        $env:DBP_TEST_FAIL_COMPILER_STAGE_CLEANUP =
            $script:OriginalCompilerCleanupFailureStage
        $env:DBP_TEST_MANIFEST_SNAPSHOT_GATE =
            $script:OriginalManifestSnapshotGate
    }

    It "publishes and launches a tuple from an unrelated working directory" {
        $fixture = New-PublisherFixture $script:PublisherPath
        try {
            $result = Invoke-Publish $script:PublisherPath $fixture
            $result.ExitCode | Should Be 0
            $result.Stderr | Should BeNullOrEmpty
            $event = $result.Stdout | ConvertFrom-Json
            $event.type | Should Be "result"
            $event.status | Should Be "ok"
            Test-Path -LiteralPath $fixture.Executable -PathType Leaf |
                Should Be $true
            Test-Path -LiteralPath $fixture.Descriptor -PathType Leaf |
                Should Be $true

            $launched = Invoke-ProcessWithTimeout `
                -FileName $fixture.Executable `
                -Arguments "--version" `
                -WorkingDirectory ([IO.Path]::GetTempPath())
            $launched.ExitCode | Should Be 0
            $launched.Stdout | Should Match "dbp-publish 1.0.0"

            @(Get-ChildItem -LiteralPath $fixture.Root -Recurse -Force |
                Where-Object {
                    $_.Name -like "*.pck" -or
                    $_.Name -like "*.dbp-stage-*" -or
                    $_.Name -like "*.dbp-backup-*"
                }).Count | Should Be 0
            Test-Path -LiteralPath (Join-Path $fixture.Root "Files") |
                Should Be $false
        }
        finally {
            Remove-Item -LiteralPath $fixture.Root -Recurse -Force `
                -ErrorAction SilentlyContinue
        }
    }

    It "rejects malformed and broadly readable keys without path leakage" {
        $fixture = New-PublisherFixture $script:PublisherPath
        try {
            $wrongSize = Join-Path $fixture.Root "wrong-size-secret.key"
            [IO.File]::WriteAllBytes($wrongSize, [byte[]](0..30))
            Set-OwnerOnlyKeyAcl $wrongSize
            $wrong = Invoke-Publish `
                $script:PublisherPath $fixture $wrongSize
            $wrong.ExitCode | Should Be 3
            $wrong.Stdout | Should Not Match "wrong-size-secret.key"
            $wrong.Stderr | Should BeNullOrEmpty

            $broad = Join-Path $fixture.Root "broad-secret.key"
            [IO.File]::WriteAllBytes($broad, [byte[]](0..31))
            Set-OwnerOnlyKeyAcl $broad
            Add-BroadReadAcl $broad
            $broadResult = Invoke-Publish `
                $script:PublisherPath $fixture $broad
            $broadResult.ExitCode | Should Be 3
            $broadResult.Stdout | Should Not Match "broad-secret.key"
            $broadResult.Stderr | Should BeNullOrEmpty
        }
        finally {
            Remove-Item -LiteralPath $fixture.Root -Recurse -Force `
                -ErrorAction SilentlyContinue
        }
    }

    It "rejects traversal, case collisions, and missing asset inputs" {
        $fixture = New-PublisherFixture $script:PublisherPath
        try {
            Write-PublisherManifest $fixture.Manifest @(
                @{source="asset.bin"; destination="../escape.bin"})
            (Invoke-Publish $script:PublisherPath $fixture).ExitCode |
                Should Be 2

            Write-PublisherManifest $fixture.Manifest @(
                @{source="asset.bin"; destination="Media/A.bin"},
                @{source="asset.bin"; destination="media/a.bin"})
            (Invoke-Publish $script:PublisherPath $fixture).ExitCode |
                Should Be 2

            Write-PublisherManifest $fixture.Manifest @(
                @{source="missing.bin"; destination="media/a.bin"})
            (Invoke-Publish $script:PublisherPath $fixture).ExitCode |
                Should Be 3
            Test-Path -LiteralPath (Join-Path $fixture.Root "escape.bin") |
                Should Be $false
        }
        finally {
            Remove-Item -LiteralPath $fixture.Root -Recurse -Force `
                -ErrorAction SilentlyContinue
        }
    }

    It "rolls back both pre-commit interruption boundaries" {
        $fixture = New-PublisherFixture $script:PublisherPath
        $previousFailureStage = $env:DBP_TEST_FAIL_PUBLICATION_STAGE
        try {
            (Invoke-Publish $script:PublisherPath $fixture).ExitCode |
                Should Be 0
            $exeHash = (Get-FileHash -LiteralPath $fixture.Executable `
                -Algorithm SHA256).Hash
            $descriptorHash = (Get-FileHash `
                -LiteralPath $fixture.Descriptor -Algorithm SHA256).Hash

            foreach ($stage in @("after-package", "after-executable")) {
                $env:DBP_TEST_FAIL_PUBLICATION_STAGE = $stage
                $result = Invoke-Publish $script:PublisherPath $fixture
                $result.ExitCode | Should Be 5
                ($result.Stdout | ConvertFrom-Json).committed |
                    Should Be $false
                (Get-FileHash -LiteralPath $fixture.Executable `
                    -Algorithm SHA256).Hash | Should Be $exeHash
                (Get-FileHash -LiteralPath $fixture.Descriptor `
                    -Algorithm SHA256).Hash | Should Be $descriptorHash
            }
            @(Get-ChildItem -LiteralPath (Join-Path $fixture.Root "dist") `
                -Force | Where-Object {
                    $_.Name -like "*.dbp-stage-*" -or
                    $_.Name -like "*.dbp-backup-*"
                }).Count | Should Be 0
        }
        finally {
            $env:DBP_TEST_FAIL_PUBLICATION_STAGE = $previousFailureStage
            Remove-Item -LiteralPath $fixture.Root -Recurse -Force `
                -ErrorAction SilentlyContinue
        }
    }

    It "reports a committed tuple distinctly when cleanup is interrupted" {
        $fixture = New-PublisherFixture $script:PublisherPath
        $previousFailureStage = $env:DBP_TEST_FAIL_PUBLICATION_STAGE
        try {
            (Invoke-Publish $script:PublisherPath $fixture).ExitCode |
                Should Be 0
            $env:DBP_TEST_FAIL_PUBLICATION_STAGE = "during-cleanup"
            $result = Invoke-Publish $script:PublisherPath $fixture
            $result.ExitCode | Should Be 5
            $event = $result.Stdout | ConvertFrom-Json
            $event.committed | Should Be $true
            $event.phase | Should Be "cleanup"
            Test-Path -LiteralPath $fixture.Executable -PathType Leaf |
                Should Be $true
            Test-Path -LiteralPath $fixture.Descriptor -PathType Leaf |
                Should Be $true
        }
        finally {
            $env:DBP_TEST_FAIL_PUBLICATION_STAGE = $previousFailureStage
            Remove-Item -LiteralPath $fixture.Root -Recurse -Force `
                -ErrorAction SilentlyContinue
        }
    }

    It "rejects an asset modified after the manifest snapshot" {
        $fixture = New-PublisherFixture $script:PublisherPath
        $gate = Join-Path $fixture.Root "manifest snapshot gate"
        $stdoutFile = Join-Path $fixture.Root "publisher stdout.json"
        $stderrFile = Join-Path $fixture.Root "publisher stderr.txt"
        $process = $null
        New-Item -ItemType Directory -Path $gate | Out-Null
        $env:DBP_TEST_MANIFEST_SNAPSHOT_GATE = $gate
        try {
            $process = Start-Process `
                -FilePath $script:PublisherPath `
                -ArgumentList (
                    "publish `"$($fixture.Manifest)`" " +
                    "--package-key-file `"$($fixture.Key)`" --json") `
                -WorkingDirectory $fixture.Root `
                -RedirectStandardOutput $stdoutFile `
                -RedirectStandardError $stderrFile `
                -NoNewWindow `
                -PassThru
            $ready = Join-Path $gate "ready"
            $deadline = [DateTime]::UtcNow.AddSeconds(10)
            while (-not (Test-Path -LiteralPath $ready -PathType Leaf) -and
                   -not $process.HasExited -and
                   [DateTime]::UtcNow -lt $deadline) {
                Start-Sleep -Milliseconds 25
                $process.Refresh()
            }
            Test-Path -LiteralPath $ready -PathType Leaf |
                Should Be $true

            [IO.File]::WriteAllBytes(
                (Join-Path $fixture.Root "asset.bin"),
                [byte[]](0x40, 0x30, 0x20, 0x10))
            [IO.File]::WriteAllText(
                (Join-Path $gate "continue"),
                "",
                [Text.UTF8Encoding]::new($false))
            Wait-Process -Id $process.Id -Timeout 15 -ErrorAction Stop
            $process.Refresh()

            $process.ExitCode | Should Be 4
            (Get-Content -LiteralPath $stdoutFile -Raw |
                ConvertFrom-Json).committed | Should Be $false
            Test-Path -LiteralPath $fixture.Executable |
                Should Be $false
            Test-Path -LiteralPath $fixture.Descriptor |
                Should Be $false
        }
        finally {
            $env:DBP_TEST_MANIFEST_SNAPSHOT_GATE = $null
            if ($null -ne $process -and -not $process.HasExited) {
                Stop-Process -Id $process.Id -Force `
                    -ErrorAction SilentlyContinue
            }
            Remove-Item -LiteralPath $fixture.Root -Recurse -Force `
                -ErrorAction SilentlyContinue
        }
    }

    It "rejects a corrupted payload through the production package reader" {
        $fixture = New-PublisherFixture $script:PublisherPath
        $launchDirectory = Join-Path $fixture.Root "unrelated launch cwd"
        New-Item -ItemType Directory -Path $launchDirectory | Out-Null
        try {
            $published = Invoke-Publish $script:PublisherPath $fixture
            $published.ExitCode | Should Be 0
            $event = $published.Stdout | ConvertFrom-Json
            $packagePath = [IO.Path]::GetFullPath([string]$event.package)

            $valid = Invoke-ProcessWithTimeout `
                -FileName $script:PackageProbePath `
                -Arguments (
                    "`"$packagePath`" `"$($fixture.Key)`"") `
                -WorkingDirectory $launchDirectory
            $valid.ExitCode | Should Be 0

            $stream = [IO.File]::Open(
                $packagePath,
                [IO.FileMode]::Open,
                [IO.FileAccess]::ReadWrite,
                [IO.FileShare]::None)
            try {
                $null = $stream.Seek(-1, [IO.SeekOrigin]::End)
                $original = $stream.ReadByte()
                $null = $stream.Seek(-1, [IO.SeekOrigin]::End)
                $stream.WriteByte([byte]($original -bxor 0xFF))
                $stream.Flush($true)
            }
            finally {
                $stream.Dispose()
            }

            $corrupt = Invoke-ProcessWithTimeout `
                -FileName $script:PackageProbePath `
                -Arguments (
                    "`"$packagePath`" `"$($fixture.Key)`"") `
                -WorkingDirectory $launchDirectory
            $corrupt.ExitCode | Should Be 4
            Test-Path -LiteralPath (
                Join-Path $launchDirectory "media") |
                Should Be $false
        }
        finally {
            Remove-Item -LiteralPath $fixture.Root -Recurse -Force `
                -ErrorAction SilentlyContinue
        }
    }
}
