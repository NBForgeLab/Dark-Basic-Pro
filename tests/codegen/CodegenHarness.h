// CodegenHarness.h — in-process driver for the DBP compiler and x64 assembly
// generator.
//
// The production compiler only reveals its machine code by writing an EXE to
// disk (CASMWriter::PrepareEXE). This harness reproduces the exact emission
// pipeline used by CDBMWriter::WriteProgramAsEXEOrDEBUG() but stops before any
// file is touched, so a test can observe:
//
//   * the raw machine-code bytes sitting in the CMachineCodeBuffer,
//   * the DBM textual assembly listing (one line per emitted instruction),
//   * every forward-reference record awaiting relocation,
//   * the diagnostics accumulated by CError.
//
// Everything is captured into a plain value type (Snapshot) so callers can
// golden-file it, fingerprint it, or assert invariants over it.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dbp::codegen {

/// A single forward reference emitted alongside the machine code.
struct ReferenceRecord {
    std::uint32_t offset = 0;      ///< byte offset inside the emitted program
    std::string   label;           ///< unresolved symbolic or numeric label
    std::uint32_t slotBytes = 0;   ///< width of the operand slot (4 or 8)
    std::uint32_t relEnd = 0;      ///< end of the enclosing instruction
};

/// Everything one compilation produced.
struct Snapshot {
    // --- lifecycle -------------------------------------------------------
    bool   parsed = false;    ///< MakeStatements() completed without failure
    bool   emitted = false;   ///< the statement walk ran to completion
    bool   relocated = false; ///< UpdateMCB + UpdateMCBRefData succeeded
    std::string stage;        ///< last pipeline stage that was reached

    // --- diagnostics -----------------------------------------------------
    bool hasError = false;
    std::string errorMessage;
    bool hasParserError = false;
    std::string parserErrorMessage;

    // --- outputs ---------------------------------------------------------
    std::vector<std::uint8_t> bytes;         ///< raw machine code
    std::vector<std::uint8_t> relocatedBytes;///< machine code after relocation
    std::string listing;                     ///< DBM textual assembly listing
    std::vector<ReferenceRecord> refs;       ///< pre-relocation references
    std::size_t listingLines = 0;
    std::uint32_t varSpaceSize = 0;
    std::uint32_t instructionCount = 0;

    /// Allocated size of the machine-code buffer. The generator fills the
    /// whole buffer with 0xC3 (RET) on initialization, so the region after the
    /// emitted program is a canary: if it is not 0xC3 something wrote past the
    /// write cursor.
    std::uint32_t capacity = 0;
    std::vector<std::uint8_t> guardBytes;    ///< canary region after `bytes`

    // --- timing ----------------------------------------------------------
    double elapsedMs = 0.0;
};

/// Knobs for a single compilation.
struct Options {
    /// Run UpdateMCB() + UpdateMCBRefData() after emission so that operand
    /// slots are patched and can be checked for left-over placeholders.
    bool relocate = true;

    /// Capture the DBM textual listing (requires a scratch buffer).
    bool captureListing = true;

    /// Scratch size for the DBM capture buffer.
    std::size_t dbmBufferSize = 102400u * 10u;

    /// Load plugin-exported commands from the runtime root. Off by default:
    /// it requires the installed runtime and is not hermetic.
    bool loadPluginCommands = false;
};

/// Process-wide one-time bootstrap (instruction database + a CDBPCompiler
/// instance for the globals the legacy emission code dereferences).
/// Safe to call any number of times from any test.
void EnsureEnvironment();

/// Complete bootstrap for an unattended process (test binary, corpus runner,
/// fuzz target). Performs the setup every such entry point needs, in the right
/// order:
///
///   * headless mode, so DB3_CRASH()/assert dialogs cannot block the run,
///   * the logger, so traces land in a per-process file instead of the console,
///   * crash diagnostics (message + minidump) for post-mortem triage,
///   * EnsureEnvironment() for the compiler globals.
///
/// Call this once from main() instead of open-coding the sequence: every entry
/// point that re-implements it is another place for the setup to drift out of
/// sync — which is how the fuzz target ended up running without headless mode.
/// Safe to call more than once.
void InitializeForUnattendedUse(const char* logFileName);

/// Compiles a DarkBASIC source snippet all the way to machine code and
/// returns everything that was produced. Never writes to disk.
///
/// The source is normalized to CRLF and copied into a mutable scratch buffer
/// because CStatementList::MakeStatements() parses in place.
[[nodiscard]] Snapshot CompileSnippet(std::string_view source,
                                      const Options& options = {});

/// 16-bytes-per-line hex dump with an address column and ASCII gutter.
/// Deterministic: no pointers, no timings.
[[nodiscard]] std::string HexDump(const std::vector<std::uint8_t>& bytes,
                                  std::size_t perLine = 16);

/// Strips anything that varies between machines or runs (absolute
/// addresses, temp paths, timings, drive letters) so goldens stay stable.
[[nodiscard]] std::string Normalize(std::string_view text);

/// Stable SHA-256 style fingerprint of a snapshot (FNV-1a 128-bit, rendered
/// as hex). Used to prove determinism without depending on exact bytes.
[[nodiscard]] std::string Fingerprint(const Snapshot& snapshot);

/// Renders a snapshot as the canonical golden-file document:
/// a short header block, the listing, the hex dump and the reference table.
[[nodiscard]] std::string RenderGoldenDocument(const Snapshot& snapshot,
                                               std::string_view caseName);

/// Directory holding the golden corpus (tests/codegen/goldens).
[[nodiscard]] std::filesystem::path GoldensDirectory();

/// True when DBP_UPDATE_GOLDENS=1 — goldens are regenerated instead of
/// compared.
[[nodiscard]] bool UpdateGoldensEnabled();

/// Compares `actual` against `goldenPath`. When update mode is on, the file
/// is rewritten and true is returned. On mismatch, `diff` receives a unified
/// diff when it is non-null.
bool CompareOrUpdateGolden(const std::filesystem::path& goldenPath,
                           std::string_view actual,
                           std::string* diff = nullptr);

/// First differing line between two texts, or nullopt when equal.
[[nodiscard]] std::optional<std::string> FirstDifference(std::string_view expected,
                                                         std::string_view actual);

} // namespace dbp::codegen
