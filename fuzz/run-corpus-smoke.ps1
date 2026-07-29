param(
    [Parameter(Mandatory = $true)]
    [string]$BinaryDirectory,
    [string]$CorpusRoot = "out/fuzz-corpus"
)

$ErrorActionPreference = "Stop"
$binaryRoot = (Resolve-Path -LiteralPath $BinaryDirectory).Path
$corpusRootPath = [IO.Path]::GetFullPath($CorpusRoot)
$v2Corpus = Join-Path $corpusRootPath "v2"
$legacyCorpus = Join-Path $corpusRootPath "legacy"
New-Item -ItemType Directory -Path $v2Corpus -Force | Out-Null
New-Item -ItemType Directory -Path $legacyCorpus -Force | Out-Null

& (Join-Path $binaryRoot "dbp_fuzz_seed_generator.exe") `
    $v2Corpus $legacyCorpus
if ($LASTEXITCODE -ne 0) {
    throw "Fuzz seed generation failed with exit code $LASTEXITCODE."
}

& (Join-Path $binaryRoot "dbp_package_reader_fuzzer.exe") `
    -runs=128 -max_total_time=30 $v2Corpus
if ($LASTEXITCODE -ne 0) {
    throw "DBPAK v2 corpus smoke failed with exit code $LASTEXITCODE."
}

& (Join-Path $binaryRoot "dbp_legacy_pck_reader_fuzzer.exe") `
    -runs=128 -max_total_time=30 $legacyCorpus
if ($LASTEXITCODE -ne 0) {
    throw "Legacy PCK corpus smoke failed with exit code $LASTEXITCODE."
}
