// CodegenHarness.cpp — see CodegenHarness.h for the contract.
//
// The emission sequence mirrors CDBMWriter::WriteProgramAsEXEOrDEBUG()
// (DBMWriter.cpp:116) with every disk-touching step removed:
//
//   MakeStatements  ->  CreateASMHeader  ->  StatementList::WriteDBM
//   ->  ProgramStatements::WriteDBM  ->  PreScanStatements::WriteDBM
//   ->  EstablishVarOffsets  ->  [UpdateMCB + UpdateMCBRefData]
//
// The final two steps are the ones CASMWriter::PrepareEXE() performs inside
// the ExecutablePreparationPipeline; running them directly is what lets a test
// observe relocated bytes without producing an EXE.

#include "CodegenHarness.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <regex>
#include <sstream>

#include <windows.h>

#include "ASMWriter.h"
#include "CompilerContext.h"
#include "CrashHandler.h"
#include "DBPCompiler.h"
#include "DBPLogger.h"
#include "DBMWriter.h"
#include "Error.h"
#include "EXEBlock.h"
#include "InstructionTable.h"
#include "StatementList.h"
#include "StructTable.h"
#include "VarTable.h"

#include "DB3.h"

// Legacy compiler globals owned by dbp_compiler_lib (DBPCompiler.cpp:38).
extern CEXEBlock*         g_pEXE;
extern CDBPCompiler*      g_pDBPCompiler;
extern CError*            g_pErrorReport;
extern ICodeGenerator*    g_pASMWriter;
extern CDBMWriter*        g_pDBMWriter;
extern CStructTable*      g_pStructTable;
extern CStatementList*    g_pStatementList;
extern CInstructionTable* g_pInstructionTable;
extern CVarTable*         g_pVarTable;

namespace dbp::codegen {
namespace {

// A long-lived CDBPCompiler keeps g_pDBPCompiler valid for the whole process.
// CStatement::WriteDBM() dereferences it unconditionally whenever
// m_bPerformJumpChecks is set (Statement.cpp:5200), which is the default.
CDBPCompiler* g_harnessCompiler = nullptr;
char          g_harnessCompilerPath[] = "DBPCompiler.exe";

void EnsureInstructionDatabase()
{
    if (g_pInstructionTable == nullptr) {
        g_pInstructionTable = new CInstructionTable();
    }
    // SetInternalInstructionDatabase() appends to a linked list, so calling it
    // twice in one process duplicates every entry. Probe first: Alloc is the
    // first internal instruction registered (InstructionTable.h:25).
    if (g_pInstructionTable->GetIIValue(1) == 0) {
        g_pInstructionTable->SetInternalInstructionDatabase();
    }
}

std::string NormalizeLineEndings(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                ++i;
            }
            out.push_back('\n');
        } else {
            out.push_back(text[i]);
        }
    }
    return out;
}

// The parser walks the buffer in place and expects CRLF; every conformance
// case is normalized the same way by the Pester runner (ConformanceRunner.psm1).
std::string ToCompilerSource(std::string_view source)
{
    const std::string lf = NormalizeLineEndings(source);
    std::string crlf;
    crlf.reserve(lf.size() + 2);
    for (const char c : lf) {
        if (c == '\n') {
            crlf.push_back('\r');
        }
        crlf.push_back(c);
    }
    if (crlf.empty() || (crlf.size() >= 2 && crlf.compare(crlf.size() - 2, 2, "\r\n") != 0)) {
        if (crlf.empty() || crlf.back() != '\n') {
            crlf.push_back('\r');
            crlf.push_back('\n');
        }
    }
    return crlf;
}

std::string ReadListing(DWORD used)
{
    if (used == 0) {
        return {};
    }
    LPSTR cursor = g_pDBMWriter->GetDBMDataPointer();
    if (cursor == nullptr) {
        return {};
    }
    const char* base = cursor - used;
    return std::string(base, static_cast<std::size_t>(used));
}

} // namespace

void EnsureEnvironment()
{
    if (g_harnessCompiler == nullptr) {
        g_harnessCompiler = new CDBPCompiler(g_harnessCompilerPath);
    }
    g_pDBPCompiler = g_harnessCompiler;
    EnsureInstructionDatabase();
}

void InitializeForUnattendedUse(const char* logFileName)
{
    // Dialogs must be suppressed before anything can trip an assert: an
    // unattended process would otherwise block forever waiting for input.
    db3::g_bHeadlessMode = true;

    DBPLogger::Initialize(logFileName);

    // Crash diagnostics last, so a fault during this very bootstrap is still
    // reported with a message and a minidump next to the executable.
    db3::SetupDiagnosticHandlers();

    EnsureEnvironment();
}

Snapshot CompileSnippet(std::string_view source, const Options& options)
{
    Snapshot snapshot;
    const auto started = std::chrono::steady_clock::now();

    EnsureEnvironment();

    CompilerContext context;
    context.Initialize();
    g_pStructTable->SetStructDefaults();
    EnsureInstructionDatabase();

    if (options.loadPluginCommands && g_pInstructionTable != nullptr) {
        g_pInstructionTable->LoadInstructionDatabase();
    }

    std::string program = ToCompilerSource(source);
    if (program.empty()) {
        program = "\r\n";
    }

    // --- parse ------------------------------------------------------------
    snapshot.stage = "MakeStatements";
    snapshot.parsed = g_pStatementList->MakeStatements(program.data(),
                                                       static_cast<DWORD>(program.size()));

    const bool hadParseError = snapshot.parsed ? false : g_pErrorReport->IsError();

    // --- backend init -----------------------------------------------------
    if (!g_pASMWriter->CreateASMHeader()) {
        snapshot.stage = "CreateASMHeader";
        snapshot.hasError = true;
        snapshot.errorMessage = g_pErrorReport ? std::string(g_pErrorReport->GetErrorString()) : "";
        snapshot.elapsedMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        context.Cleanup();
        return snapshot;
    }
    snapshot.stage = "CreateASMHeader";

    // Enable DBM capture before any emission: CDBMWriter::OutputDBM() silently
    // discards the listing while its buffer is empty (DBMWriter.cpp:43).
    if (options.captureListing) {
        g_pDBMWriter->InitializeBufferForTests(static_cast<DWORD>(options.dbmBufferSize));
    }

    auto* writer = static_cast<CASMWriter*>(g_pASMWriter);

    // --- emit -------------------------------------------------------------
    bool ok = true;
    g_pStatementList->SetWriteStarted(true);
    ok = ok && g_pStatementList->WriteDBM();
    snapshot.stage = "StatementList::WriteDBM";

    if (ok && g_pStatementList->GetProgramStatements() != nullptr) {
        ok = g_pStatementList->GetProgramStatements()->WriteDBM();
        snapshot.stage = "ProgramStatements::WriteDBM";
    }
    if (ok && g_pStatementList->GetPreScanStatements() != nullptr) {
        ok = g_pStatementList->GetPreScanStatements()->WriteDBM();
        snapshot.stage = "PreScanStatements::WriteDBM";
    }
    g_pStatementList->SetWriteStarted(false);

    if (ok) {
        DWORD varSize = 0;
        g_pVarTable->EstablishVarOffsets(&varSize);
        g_pStatementList->SetVarOffsetCounter(varSize);
        snapshot.varSpaceSize = varSize;
        snapshot.stage = "EstablishVarOffsets";
    }
    snapshot.emitted = ok;

    // --- capture pre-relocation state -------------------------------------
    const CMachineCodeBuffer& mcb = writer->GetMachineCodeBuffer();
    const DWORD emitted = writer->GetCurrentMCPosition();
    const char* base = mcb.GetProgramStart();
    snapshot.capacity = mcb.GetMCBlockSize();
    if (base != nullptr) {
        if (emitted > 0) {
            snapshot.bytes.assign(
                reinterpret_cast<const std::uint8_t*>(base),
                reinterpret_cast<const std::uint8_t*>(base) + emitted);
        }
        // Capture the canary region that follows the emitted program.
        const DWORD guardLength = std::min<DWORD>(64, mcb.GetMCBlockSize() - emitted);
        if (guardLength > 0) {
            const char* guardBase = base + emitted;
            snapshot.guardBytes.assign(
                reinterpret_cast<const std::uint8_t*>(guardBase),
                reinterpret_cast<const std::uint8_t*>(guardBase) + guardLength);
        }
    }

    for (const auto& record : writer->GetReferenceTracker().GetRecords()) {
        ReferenceRecord copy;
        copy.offset = record.machineCodeOffset;
        copy.label = record.label;
        copy.slotBytes = record.slotBytes;
        copy.relEnd = record.relEnd;
        snapshot.refs.push_back(std::move(copy));
    }

    if (options.captureListing) {
        snapshot.listing = ReadListing(g_pDBMWriter->GetUsedBufferSizeForTests());
        snapshot.listingLines = std::count(snapshot.listing.begin(), snapshot.listing.end(), '\n');
        // Section headers ("######CODE:") are structural, not instructions.
        snapshot.instructionCount = 0;
        std::istringstream lines(snapshot.listing);
        std::string line;
        while (std::getline(lines, line)) {
            if (!line.empty() && line[0] != '#') {
                ++snapshot.instructionCount;
            }
        }
    }

    // --- relocate ---------------------------------------------------------
    if (ok && options.relocate) {
        snapshot.stage = "UpdateMCB";
        if (writer->UpdateMCB(emitted)) {
            const DWORD errorsBefore = g_pErrorReport != nullptr && g_pErrorReport->IsError() ? 1u : 0u;
            if (writer->UpdateMCBRefData()) {
                snapshot.relocated = true;
                snapshot.stage = "UpdateMCBRefData";
            }
            (void)errorsBefore;

            if (g_pEXE != nullptr && g_pEXE->m_pMachineCodeBlock != nullptr && g_pEXE->m_dwSizeOfMCB > 0) {
                const std::uint8_t* relocatedBase = g_pEXE->GetMachineCodeBlockBytePointer();
                snapshot.relocatedBytes.assign(relocatedBase,
                                               relocatedBase + g_pEXE->m_dwSizeOfMCB);
            }
        }
    }

    // --- diagnostics ------------------------------------------------------
    if (g_pErrorReport != nullptr) {
        snapshot.hasError = g_pErrorReport->IsError();
        if (snapshot.hasError) {
            snapshot.errorMessage = std::string(g_pErrorReport->GetErrorString());
        }
        snapshot.hasParserError = g_pErrorReport->IsParserError();
        if (snapshot.hasParserError) {
            snapshot.parserErrorMessage = std::string(g_pErrorReport->GetParserErrorString());
        }
    }
    if (hadParseError) {
        snapshot.hasError = true;
        if (snapshot.errorMessage.empty()) {
            snapshot.errorMessage = "MakeStatements failed";
        }
    }

    snapshot.elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();

    context.Cleanup();
    g_pDBPCompiler = g_harnessCompiler;
    return snapshot;
}

std::string HexDump(const std::vector<std::uint8_t>& bytes, std::size_t perLine)
{
    std::ostringstream out;
    static const char* kHex = "0123456789ABCDEF";
    for (std::size_t i = 0; i < bytes.size(); i += perLine) {
        out << i << ':';
        const std::size_t pad = 8 - std::min<std::size_t>(8, std::to_string(i).size());
        out << std::string(pad, ' ');
        for (std::size_t j = 0; j < perLine; ++j) {
            if (j > 0 && j % 8 == 0) {
                out << ' ';
            }
            if (i + j < bytes.size()) {
                const std::uint8_t b = bytes[i + j];
                out << kHex[b >> 4] << kHex[b & 0x0F] << ' ';
            } else {
                out << "   ";
            }
        }
        out << ' ';
        for (std::size_t j = 0; j < perLine && i + j < bytes.size(); ++j) {
            const unsigned char c = bytes[i + j];
            out << (c >= 32 && c < 127 ? static_cast<char>(c) : '.');
        }
        out << '\n';
    }
    return out.str();
}

std::string Normalize(std::string_view text)
{
    std::string out = NormalizeLineEndings(text);

    // Hexadecimal addresses / pointer values.
    out = std::regex_replace(out, std::regex("0[xX][0-9a-fA-F]{6,16}"), "<addr>");

    // Windows paths — collapse temp directories first, then any absolute path.
    out = std::regex_replace(
        out,
        std::regex("[A-Za-z]:\\\\[^\\r\\n\"']*?[\\\\/](?:Temp|temp)[\\\\/][^\\r\\n\"']*"),
        "<temp>");
    out = std::regex_replace(out, std::regex("[A-Za-z]:\\\\[^\\r\\n\"' ,;)]*"), "<path>");

    // Posix-style temp roots.
    out = std::regex_replace(out, std::regex("/(?:tmp|var/folders)/[^\\r\\n\"' ]*"), "<temp>");

    return out;
}

std::string Fingerprint(const Snapshot& snapshot)
{
    // FNV-1a across four independent offsets yields a 128-bit digest without
    // pulling in a crypto dependency.
    constexpr std::array<std::uint64_t, 4> kOffset = {
        0x9E3779B97F4A7C15ull, 0xC2B2AE3D27D4EB4Full,
        0x165667B19E3779F9ull, 0x85EBCA77C2B2AE63ull};
    constexpr std::uint64_t kPrime = 0x100000001B3ull;

    std::array<std::uint64_t, 4> state = kOffset;
    auto absorb = [&state, kPrime](std::string_view data) {
        for (const char c : data) {
            const auto byte = static_cast<std::uint8_t>(c);
            for (std::size_t i = 0; i < state.size(); ++i) {
                state[i] ^= byte;
                state[i] *= kPrime;
            }
        }
    };

    absorb(std::string_view(reinterpret_cast<const char*>(snapshot.bytes.data()),
                            snapshot.bytes.size()));
    absorb(snapshot.listing);
    for (const auto& ref : snapshot.refs) {
        absorb(ref.label);
        absorb(std::string_view(reinterpret_cast<const char*>(&ref.offset), sizeof(ref.offset)));
    }

    std::ostringstream out;
    for (const std::uint64_t word : state) {
        out << std::hex << std::setw(16) << std::setfill('0') << word;
    }
    return out.str();
}

std::string RenderGoldenDocument(const Snapshot& snapshot, std::string_view caseName)
{
    std::ostringstream out;
    out << "# codegen golden: " << caseName << "\n";
    out << "# generated by dbp codegen harness -- do not edit by hand\n";
    out << "# regenerate with: DBP_UPDATE_GOLDENS=1\n";
    out << "\n";
    out << "[meta]\n";
    out << "parsed=" << (snapshot.parsed ? "true" : "false") << "\n";
    out << "emitted=" << (snapshot.emitted ? "true" : "false") << "\n";
    out << "relocated=" << (snapshot.relocated ? "true" : "false") << "\n";
    out << "stage=" << snapshot.stage << "\n";
    out << "hasError=" << (snapshot.hasError ? "true" : "false") << "\n";
    out << "errorMessage=" << Normalize(snapshot.errorMessage) << "\n";
    out << "hasParserError=" << (snapshot.hasParserError ? "true" : "false") << "\n";
    out << "parserErrorMessage=" << Normalize(snapshot.parserErrorMessage) << "\n";
    out << "byteCount=" << snapshot.bytes.size() << "\n";
    out << "relocatedByteCount=" << snapshot.relocatedBytes.size() << "\n";
    out << "instructionCount=" << snapshot.instructionCount << "\n";
    out << "listingLines=" << snapshot.listingLines << "\n";
    out << "varSpaceSize=" << snapshot.varSpaceSize << "\n";
    out << "referenceCount=" << snapshot.refs.size() << "\n";
    out << "\n";

    out << "[listing]\n";
    out << Normalize(snapshot.listing);
    if (!snapshot.listing.empty() && snapshot.listing.back() != '\n') {
        out << "\n";
    }
    out << "\n";

    out << "[references]\n";
    for (const auto& ref : snapshot.refs) {
        out << ref.offset << " slot=" << ref.slotBytes << " relEnd=" << ref.relEnd
            << " label=" << ref.label << "\n";
    }
    out << "\n";

    out << "[bytes]\n";
    out << HexDump(snapshot.bytes);
    out << "\n";

    out << "[relocated-bytes]\n";
    out << HexDump(snapshot.relocatedBytes);
    out << "\n";

    return out.str();
}

std::filesystem::path GoldensDirectory()
{
    return std::filesystem::path(DBP_TEST_SOURCE_ROOT) / "tests" / "codegen" / "goldens";
}

bool UpdateGoldensEnabled()
{
    const char* value = std::getenv("DBP_UPDATE_GOLDENS");
    return value != nullptr && std::string(value) == "1";
}

std::optional<std::string> FirstDifference(std::string_view expected, std::string_view actual)
{
    std::size_t line = 1;
    std::size_t expectedStart = 0;
    std::size_t actualStart = 0;

    while (expectedStart < expected.size() || actualStart < actual.size()) {
        std::size_t expectedEnd = expected.find('\n', expectedStart);
        std::size_t actualEnd = actual.find('\n', actualStart);
        if (expectedEnd == std::string_view::npos) expectedEnd = expected.size();
        if (actualEnd == std::string_view::npos) actualEnd = actual.size();

        const auto expectedLine = expected.substr(expectedStart, expectedEnd - expectedStart);
        const auto actualLine = actual.substr(actualStart, actualEnd - actualStart);

        if (expectedLine != actualLine) {
            std::ostringstream out;
            out << "line " << line << "\n  expected: " << expectedLine
                << "\n  actual  : " << actualLine;
            return out.str();
        }

        if (expectedEnd >= expected.size() || actualEnd >= actual.size()) {
            break;
        }
        expectedStart = expectedEnd + 1;
        actualStart = actualEnd + 1;
        ++line;
    }
    return std::nullopt;
}

bool CompareOrUpdateGolden(const std::filesystem::path& goldenPath,
                           std::string_view actual,
                           std::string* diff)
{
    std::error_code ec;
    if (UpdateGoldensEnabled()) {
        std::filesystem::create_directories(goldenPath.parent_path(), ec);
        std::FILE* file = nullptr;
        const auto wide = goldenPath.wstring();
        if (_wfopen_s(&file, wide.c_str(), L"wb") == 0 && file != nullptr) {
            std::fwrite(actual.data(), 1, actual.size(), file);
            std::fclose(file);
        }
        return true;
    }

    std::string expected;
    {
        std::FILE* file = nullptr;
        const auto wide = goldenPath.wstring();
        if (_wfopen_s(&file, wide.c_str(), L"rb") != 0 || file == nullptr) {
            if (diff != nullptr) {
                std::ostringstream out;
                out << "golden file missing: " << goldenPath.string()
                    << "\nrun with DBP_UPDATE_GOLDENS=1 to create it";
                *diff = out.str();
            }
            return false;
        }
        char buffer[8192];
        std::size_t read = 0;
        while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
            expected.append(buffer, read);
        }
        std::fclose(file);
    }

    if (expected == actual) {
        return true;
    }

    if (diff != nullptr) {
        if (const auto first = FirstDifference(expected, actual)) {
            *diff = *first;
        } else {
            *diff = "content differs (same line count, byte-level difference)";
        }
    }
    return false;
}

} // namespace dbp::codegen
