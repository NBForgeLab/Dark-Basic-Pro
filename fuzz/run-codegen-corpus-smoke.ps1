param(
    [Parameter(Mandatory = $true)]
    [string]$BinaryDirectory,
    [string]$CorpusRoot = "out/codegen-fuzz-corpus"
)

$ErrorActionPreference = "Stop"
$binaryRoot = (Resolve-Path -LiteralPath $BinaryDirectory).Path
$corpusRootPath = [IO.Path]::GetFullPath($CorpusRoot)
New-Item -ItemType Directory -Path $corpusRootPath -Force | Out-Null

# 1. Generate a reproducible corpus of valid + malformed DarkBASIC snippets.
& (Join-Path $binaryRoot "dbp_codegen_seed_generator.exe") $corpusRootPath
if ($LASTEXITCODE -ne 0) {
    throw "Codegen fuzz seed generation failed with exit code $LASTEXITCODE."
}

# 2. Drive the corpus. Prefer the libFuzzer target when present (Clang build);
#    otherwise fall back to the MSVC standalone runner driven by the Python
#    harness, which isolates one process per input.
$fuzzer = Join-Path $binaryRoot "dbp_codegen_fuzzer.exe"
if (Test-Path $fuzzer) {
    & $fuzzer -runs=128 -max_total_time=30 $corpusRootPath
    if ($LASTEXITCODE -ne 0) {
        throw "Codegen libFuzzer smoke failed with exit code $LASTEXITCODE."
    }
} else {
    $runner = Join-Path $binaryRoot "dbp_codegen_corpus_runner.exe"
    $script = Join-Path $PSScriptRoot "..\tests\codegen\run_codegen_tests.py"
    & python3 $script --mode corpus --corpus $corpusRootPath --runner $runner
    if ($LASTEXITCODE -ne 0) {
        throw "Codegen corpus runner smoke failed with exit code $LASTEXITCODE."
    }
}
