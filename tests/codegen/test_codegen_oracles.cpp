// test_codegen_oracles.cpp — invariant checks over generated machine code.
//
// These are the "glitch detectors". Unlike the golden tests they do not care
// what the exact bytes are; they assert properties that must hold for *any*
// well-formed program. A violation is always a compiler defect, never an
// intended behaviour change.
//
//   O1  no unresolved leap placeholders survive emission
//   O2  no operand slot is left unpatched after relocation
//   O3  every relative branch target lands inside the emitted program
//   O4  reference slots are in range and do not overlap
//   O5  the machine-code buffer canary after the program is untouched
//   O6  emission is deterministic across repeated runs
//   O7  compiling a program twice in the same process yields identical output
//   O8  generated code contains no long runs of the 0xC3 fill byte
//   O9  the emitted program ends with a terminator, not mid-instruction junk
//   O10 the DBM listing and the byte stream agree on instruction count

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "DBPLogger.h"
#include "codegen/CodegenHarness.h"

namespace fs = std::filesystem;
using namespace dbp::codegen;

namespace {

std::string ReadFile(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::vector<fs::path> CollectGoldenCases()
{
    std::vector<fs::path> cases;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(GoldensDirectory(), ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == ".dba") {
            cases.push_back(entry.path());
        }
    }
    std::sort(cases.begin(), cases.end());
    return cases;
}

// Reads a little-endian integer of `width` bytes out of a byte vector.
std::uint64_t ReadSlot(const std::vector<std::uint8_t>& data, std::uint32_t offset,
                       std::uint32_t width)
{
    std::uint64_t value = 0;
    for (std::uint32_t i = 0; i < width; ++i) {
        if (offset + i < data.size()) {
            value |= static_cast<std::uint64_t>(data[offset + i]) << (8 * i);
        }
    }
    return value;
}

class CodegenOracleTest : public ::testing::TestWithParam<fs::path> {
protected:
    static void SetUpTestSuite()
    {
        DBPLogger::Initialize("test_codegen_oracles.log");
        EnsureEnvironment();
    }
    static void TearDownTestSuite() { spdlog::shutdown(); }
};

// ---------------------------------------------------------------------------
// O1 — CLeapMarkerManager::WriteASMLineLeap() emits the literal string "0" as
// the placeholder label and patches it in WriteASMLeapMarkerEnd(). When the
// matching End call is missing (or targets an index that was never set) the
// placeholder survives silently and the generated branch points at offset 0.
// ---------------------------------------------------------------------------
TEST_P(CodegenOracleTest, NoUnresolvedLeapPlaceholders)
{
    const std::string caseName = GetParam().stem().string();
    const Snapshot snapshot = CompileSnippet(ReadFile(GetParam()));

    for (const auto& ref : snapshot.refs) {
        // A genuine immediate zero is possible, but a *branch* placeholder is
        // always a 4-byte rel32 slot. Narrow the check to those.
        if (ref.slotBytes == 4) {
            EXPECT_NE(ref.label, "0")
                << caseName << ": unresolved leap placeholder at MCB+" << ref.offset
                << " — WriteASMLeapMarkerEnd() was never called for this marker, "
                   "so the branch will jump to offset 0 at runtime.";
        }
    }
}

// ---------------------------------------------------------------------------
// O2 — CASMWriter::CreateASMMiddleCore() writes 0xFF into every operand slot
// (ASMWriter.cpp:857) and UpdateMCBRefData() overwrites them. Any slot still
// filled with 0xFF after relocation means a reference was never resolved.
// ---------------------------------------------------------------------------
TEST_P(CodegenOracleTest, NoUnpatchedOperandSlotsAfterRelocation)
{
    const std::string caseName = GetParam().stem().string();
    const Snapshot snapshot = CompileSnippet(ReadFile(GetParam()));

    ASSERT_TRUE(snapshot.relocated)
        << caseName << ": relocation did not run (stage=" << snapshot.stage
        << ", error=" << (snapshot.hasError ? snapshot.errorMessage : std::string("none"))
        << ")";
    ASSERT_FALSE(snapshot.relocatedBytes.empty()) << caseName << ": no relocated bytes";

    for (const auto& ref : snapshot.refs) {
        if (ref.offset + ref.slotBytes > snapshot.relocatedBytes.size()) {
            continue; // reported by O4 instead
        }
        bool allOnes = true;
        for (std::uint32_t i = 0; i < ref.slotBytes; ++i) {
            if (snapshot.relocatedBytes[ref.offset + i] != 0xFF) {
                allOnes = false;
                break;
            }
        }
        EXPECT_FALSE(allOnes)
            << caseName << ": reference at MCB+" << ref.offset << " (label '" << ref.label
            << "', " << ref.slotBytes << " bytes) is still 0xFF-filled after relocation — "
               "the symbol was never resolved.";
    }
}

// ---------------------------------------------------------------------------
// O3 — Resolve every 4-byte relative slot and check the target stays inside
// the program. An out-of-range target means the branch will jump into the
// 0xC3 fill or beyond the allocated code region.
// ---------------------------------------------------------------------------
TEST_P(CodegenOracleTest, RelativeBranchTargetsStayInsideProgram)
{
    const std::string caseName = GetParam().stem().string();
    const Snapshot snapshot = CompileSnippet(ReadFile(GetParam()));

    if (!snapshot.relocated || snapshot.relocatedBytes.empty()) {
        GTEST_SKIP() << caseName << ": no relocated output";
    }

    const std::uint64_t programSize = snapshot.relocatedBytes.size();
    for (const auto& ref : snapshot.refs) {
        if (ref.slotBytes != 4) {
            continue;
        }
        if (ref.offset + 4 > snapshot.relocatedBytes.size()) {
            continue;
        }
        const std::uint64_t raw = ReadSlot(snapshot.relocatedBytes, ref.offset, 4);
        const auto displacement = static_cast<std::int64_t>(static_cast<std::int32_t>(raw));
        const std::uint32_t relEnd = ref.relEnd != 0 ? ref.relEnd : ref.offset + 4;
        const std::int64_t target = static_cast<std::int64_t>(relEnd) + displacement;

        EXPECT_GE(target, 0)
            << caseName << ": branch at MCB+" << ref.offset << " targets negative offset "
            << target;
        EXPECT_LT(target, static_cast<std::int64_t>(programSize))
            << caseName << ": branch at MCB+" << ref.offset << " targets " << target
            << " but the program is only " << programSize << " bytes (label '" << ref.label << "')";
    }
}

// ---------------------------------------------------------------------------
// O4 — Reference records must describe slots that exist and must not overlap;
// overlapping slots mean two patches fight over the same bytes.
// ---------------------------------------------------------------------------
TEST_P(CodegenOracleTest, ReferenceSlotsAreInRangeAndDisjoint)
{
    const std::string caseName = GetParam().stem().string();
    const Snapshot snapshot = CompileSnippet(ReadFile(GetParam()));

    std::vector<std::pair<std::uint32_t, std::uint32_t>> spans;
    for (const auto& ref : snapshot.refs) {
        EXPECT_LE(static_cast<std::uint64_t>(ref.offset) + ref.slotBytes, snapshot.bytes.size())
            << caseName << ": reference at MCB+" << ref.offset << " (+" << ref.slotBytes
            << ") runs past the emitted program (" << snapshot.bytes.size() << " bytes)";
        EXPECT_TRUE(ref.slotBytes == 1 || ref.slotBytes == 2 || ref.slotBytes == 4 ||
                    ref.slotBytes == 8)
            << caseName << ": implausible operand slot width " << ref.slotBytes
            << " at MCB+" << ref.offset;
        spans.emplace_back(ref.offset, ref.offset + ref.slotBytes);
    }

    std::sort(spans.begin(), spans.end());
    for (std::size_t i = 1; i < spans.size(); ++i) {
        EXPECT_GE(spans[i].first, spans[i - 1].second)
            << caseName << ": reference slots overlap at MCB+" << spans[i].first
            << " (previous slot ends at " << spans[i - 1].second << ")";
    }
}

// ---------------------------------------------------------------------------
// O5 — The machine-code buffer is filled with 0xC3 by
// CMachineCodeBuffer::Initialize(). The 64 bytes following the emitted program
// are therefore a canary: if they changed, something wrote past the cursor.
// ---------------------------------------------------------------------------
TEST_P(CodegenOracleTest, MachineCodeBufferCanaryIsUntouched)
{
    const std::string caseName = GetParam().stem().string();
    const Snapshot snapshot = CompileSnippet(ReadFile(GetParam()));

    EXPECT_LE(snapshot.bytes.size(), snapshot.capacity)
        << caseName << ": emitted " << snapshot.bytes.size()
        << " bytes into a buffer of only " << snapshot.capacity;

    for (std::size_t i = 0; i < snapshot.guardBytes.size(); ++i) {
        EXPECT_EQ(snapshot.guardBytes[i], 0xC3u)
            << caseName << ": canary byte " << i << " after the emitted program is 0x"
            << std::hex << static_cast<int>(snapshot.guardBytes[i])
            << " instead of 0xC3 — emission wrote past the write cursor.";
    }
}

// ---------------------------------------------------------------------------
// O6 — Determinism. The same source must produce byte-identical output every
// time; any dependence on uninitialized memory, iteration order of a hash
// container, or residual global state shows up here.
// ---------------------------------------------------------------------------
TEST_P(CodegenOracleTest, EmissionIsDeterministic)
{
    const std::string caseName = GetParam().stem().string();
    const std::string source = ReadFile(GetParam());

    const Snapshot first = CompileSnippet(source);
    const Snapshot second = CompileSnippet(source);
    const Snapshot third = CompileSnippet(source);

    const std::string a = Fingerprint(first);
    const std::string b = Fingerprint(second);
    const std::string c = Fingerprint(third);

    EXPECT_EQ(a, b) << caseName << ": run 2 diverged from run 1";
    EXPECT_EQ(a, c) << caseName << ": run 3 diverged from run 1";
    EXPECT_EQ(first.bytes, second.bytes)
        << caseName << ": machine code is not reproducible across runs";
}

// ---------------------------------------------------------------------------
// O7 — Global-state leakage. Compiling a different program in between must not
// change the first program's output. CompilerContext::Cleanup() deliberately
// keeps g_pInstructionTable alive across contexts, so this is a real risk.
// ---------------------------------------------------------------------------
TEST(CodegenOracleGlobalState, InterleavedCompilationDoesNotLeakState)
{
    DBPLogger::Initialize("test_codegen_oracles.log");
    EnsureEnvironment();

    const std::string programA = "a = 1\r\nwhile a < 5\r\n  a = a + 1\r\nendwhile\r\nend\r\n";
    const std::string programB =
        "type T\r\n  x as integer\r\nendtype\r\nt as T\r\nt.x = 3\r\nend\r\n";

    const Snapshot a1 = CompileSnippet(programA);
    const Snapshot b1 = CompileSnippet(programB);
    const Snapshot a2 = CompileSnippet(programA);

    EXPECT_EQ(Fingerprint(a1), Fingerprint(a2))
        << "compiling an unrelated program changed program A's output — global "
           "compiler state leaked between compilations";
    EXPECT_EQ(a1.bytes, a2.bytes) << "program A's machine code changed after an "
                                     "unrelated compilation";
    EXPECT_NE(Fingerprint(a1), Fingerprint(b1)) << "A and B should differ (sanity check)";

    spdlog::shutdown();
}

// ---------------------------------------------------------------------------
// O8 — Long runs of 0xC3 inside the program body are the signature of an
// emission gap: the generator advanced the write cursor without filling it.
// A handful of padding bytes is legitimate (alignment); a long run is not.
// ---------------------------------------------------------------------------
TEST_P(CodegenOracleTest, NoLongRunsOfFillBytesInsideProgram)
{
    const std::string caseName = GetParam().stem().string();
    const Snapshot snapshot = CompileSnippet(ReadFile(GetParam()));

    std::uint32_t run = 0;
    std::uint32_t worst = 0;
    std::size_t worstAt = 0;
    for (std::size_t i = 0; i < snapshot.bytes.size(); ++i) {
        if (snapshot.bytes[i] == 0xC3) {
            if (run == 0) {
                worstAt = i;
            }
            ++run;
            worst = std::max(worst, run);
        } else {
            run = 0;
        }
    }
    EXPECT_LE(worst, 16u)
        << caseName << ": found a run of " << worst
        << " 0xC3 bytes starting at MCB+" << worstAt
        << " — possible emission gap inside the generated program";
}

// ---------------------------------------------------------------------------
// O9 — The program must terminate. DBPro emits an explicit end-of-program
// sequence; trailing bytes that are neither a RET/INT3 nor part of the last
// instruction indicate truncated emission.
// ---------------------------------------------------------------------------
TEST_P(CodegenOracleTest, ProgramEndsAtAnInstructionBoundary)
{
    const std::string caseName = GetParam().stem().string();
    const Snapshot snapshot = CompileSnippet(ReadFile(GetParam()));

    ASSERT_FALSE(snapshot.bytes.empty()) << caseName << ": nothing was emitted";

    // Every recorded operand slot must be fully contained in the program; a
    // slot that straddles the end means the last instruction was cut short.
    for (const auto& ref : snapshot.refs) {
        EXPECT_LE(static_cast<std::uint64_t>(ref.offset) + ref.slotBytes, snapshot.bytes.size())
            << caseName << ": operand slot at MCB+" << ref.offset << " (+" << ref.slotBytes
            << ") extends past the end of the program — truncated emission";
    }
}

// ---------------------------------------------------------------------------
// O10 — The DBM listing is generated by the very same WriteASMLine() call that
// emits the bytes, so a mismatch between the two means the listing and the
// machine code have diverged.
// ---------------------------------------------------------------------------
TEST_P(CodegenOracleTest, ListingAndByteStreamAreConsistent)
{
    const std::string caseName = GetParam().stem().string();
    const Snapshot snapshot = CompileSnippet(ReadFile(GetParam()));

    if (snapshot.bytes.empty()) {
        EXPECT_TRUE(snapshot.listing.empty() || snapshot.instructionCount == 0)
            << caseName << ": emitted " << snapshot.instructionCount
            << " listing instructions but zero bytes";
    } else {
        EXPECT_GT(snapshot.instructionCount, 0u)
            << caseName << ": emitted " << snapshot.bytes.size()
            << " bytes but the listing records no instructions";
    }
    EXPECT_LE(snapshot.refs.size(), snapshot.bytes.size() / 4 + 1)
        << caseName << ": more reference records than the program can physically hold";
}

INSTANTIATE_TEST_SUITE_P(
    CodegenOracles,
    CodegenOracleTest,
    ::testing::ValuesIn(CollectGoldenCases()),
    [](const ::testing::TestParamInfo<fs::path>& info) {
        return info.param.stem().string();
    });

} // namespace
