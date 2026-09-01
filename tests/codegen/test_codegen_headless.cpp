// test_codegen_headless.cpp — unattended ("headless") soak and hygiene checks.
//
// The in-process suite is deliberately hostile to itself: it compiles the same
// programs hundreds of times, watches the CRT heap and the OS handle count for
// growth, and asserts that every compilation is reproducible. Legacy code with
// process-global state leaks in exactly these three ways, and this is the layer
// that surfaces it without needing an external sanitizer build.
//
// Run the ASan / UBSan presets (windows-x64-asan / windows-x64-ubsan) to get
// the same corpus checked with real instrumentation.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#ifdef _MSC_VER
// windows.h must precede psapi.h: the latter relies on types (DWORD, SIZE_T)
// defined by the former, and including it first makes PROCESS_MEMORY_COUNTERS
// parse correctly instead of emitting "unknown override specifier" errors.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#include <crtdbg.h>
#endif

#include "DBPLogger.h"
#include "codegen/CodegenHarness.h"

using namespace dbp::codegen;

namespace {

#ifdef _MSC_VER
std::size_t CurrentWorkingSetBytes()
{
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return static_cast<std::size_t>(counters.WorkingSetSize);
    }
    return 0;
}

DWORD CurrentHandleCount()
{
    DWORD count = 0;
    if (GetProcessHandleCount(GetCurrentProcess(), &count)) {
        return count;
    }
    return 0;
}
#endif

const char* const kSoakProgram =
    "type Point\r\n"
    "  x as integer\r\n"
    "  y as integer\r\n"
    "endtype\r\n"
    "dim grid(10) as Point\r\n"
    "total = 0\r\n"
    "for i = 0 to 10\r\n"
    "  grid(i).x = i * 2\r\n"
    "  grid(i).y = i + 1\r\n"
    "  if grid(i).x > 8\r\n"
    "    total = total + grid(i).y\r\n"
    "  endif\r\n"
    "next i\r\n"
    "while total < 100\r\n"
    "  total = total + 1\r\n"
    "endwhile\r\n"
    "s$ = \"done\" + str$(total)\r\n"
    "gosub Report\r\n"
    "end\r\n"
    "\r\n"
    "Report:\r\n"
    "  r = total\r\n"
    "return\r\n";

class CodegenHeadlessTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        DBPLogger::Initialize("test_codegen_headless.log");
        EnsureEnvironment();
    }
    static void TearDownTestSuite() { spdlog::shutdown(); }
};

// ---------------------------------------------------------------------------
// Repeated compilation must not grow the CRT heap. The first few iterations
// warm up lazily built caches (error database, string tables), so the
// measurement starts after a warm-up phase.
// ---------------------------------------------------------------------------
TEST_F(CodegenHeadlessTest, RepeatedCompilationDoesNotLeakHeap)
{
#ifdef _MSC_VER
    constexpr int kWarmup = 12;
    constexpr int kMeasured = 80;

    for (int i = 0; i < kWarmup; ++i) {
        (void)CompileSnippet(kSoakProgram);
    }

    _CrtMemState before{};
    _CrtMemState after{};
    _CrtMemState delta{};
    _CrtMemCheckpoint(&before);

    for (int i = 0; i < kMeasured; ++i) {
        const Snapshot snapshot = CompileSnippet(kSoakProgram);
        ASSERT_TRUE(snapshot.emitted) << "iteration " << i << " failed to emit code";
    }

    _CrtMemCheckpoint(&after);
    const int hasDelta = _CrtMemDifference(&delta, &before, &after);
    if (hasDelta) {
        const long net = static_cast<long>(delta.lSizes[1]) -
                         static_cast<long>(delta.lSizes[2]);
        // Fragmentation makes small deltas normal; anything that scales with
        // the iteration count does not. 4 KB over 80 compiles is 52 bytes per
        // compile — well under any real leak.
        EXPECT_LT(net, 4096)
            << "CRT heap grew by " << net << " bytes over " << kMeasured
            << " compilations (" << (net / kMeasured) << " bytes per compile) — "
               "the compiler leaks memory on each run.\n"
            << "  allocations: " << delta.lCounts[1] << " normal blocks\n"
            << "  frees:       " << delta.lCounts[2] << "\n";
    }
#else
    GTEST_SKIP() << "CRT heap accounting is MSVC-only";
#endif
}

// ---------------------------------------------------------------------------
// File handles / file mappings opened during compilation must be released.
// CDebuggerInterface creates a shared-memory mapping and CError may open the
// error database; either one leaking is invisible until a long build session
// runs out of handles.
// ---------------------------------------------------------------------------
TEST_F(CodegenHeadlessTest, RepeatedCompilationDoesNotLeakHandles)
{
#ifdef _MSC_VER
    constexpr int kWarmup = 12;
    constexpr int kMeasured = 60;

    for (int i = 0; i < kWarmup; ++i) {
        (void)CompileSnippet(kSoakProgram);
    }

    const DWORD before = CurrentHandleCount();
    for (int i = 0; i < kMeasured; ++i) {
        (void)CompileSnippet(kSoakProgram);
    }
    const DWORD after = CurrentHandleCount();

    EXPECT_LE(after, before + 8)
        << "handle count grew from " << before << " to " << after << " over "
        << kMeasured << " compilations — a kernel object is being leaked.";
#else
    GTEST_SKIP() << "handle counting is Windows-only";
#endif
}

// ---------------------------------------------------------------------------
// Working-set growth across a long soak is a coarse but effective tripwire for
// unbounded caches keyed on per-compilation data.
// ---------------------------------------------------------------------------
TEST_F(CodegenHeadlessTest, WorkingSetStaysBoundedAcrossSoak)
{
#ifdef _MSC_VER
    constexpr int kWarmup = 10;
    constexpr int kMeasured = 60;

    for (int i = 0; i < kWarmup; ++i) {
        (void)CompileSnippet(kSoakProgram);
    }

    const std::size_t before = CurrentWorkingSetBytes();
    for (int i = 0; i < kMeasured; ++i) {
        (void)CompileSnippet(kSoakProgram);
    }
    const std::size_t after = CurrentWorkingSetBytes();

    // 8 MB of slack absorbs allocator arena growth; a real per-compile leak
    // over 60 runs is far larger.
    EXPECT_LT(after, before + (8u * 1024u * 1024u))
        << "working set grew from " << (before / 1024) << " KB to " << (after / 1024)
        << " KB over " << kMeasured << " compilations.";
#else
    GTEST_SKIP() << "working-set measurement is Windows-only";
#endif
}

// ---------------------------------------------------------------------------
// Soak with varied programs: guarantees the compiler survives being driven
// through every construct repeatedly in one process without degrading.
// ---------------------------------------------------------------------------
TEST_F(CodegenHeadlessTest, MixedProgramSoakRemainsStable)
{
    const std::vector<std::string> programs = {
        "a = 1\r\nend\r\n",
        "a# = 1.5 * 2.0\r\nend\r\n",
        "dim a(5)\r\na(1) = 2\r\nundim a()\r\nend\r\n",
        "for i = 1 to 3\r\n  j = j + i\r\nnext i\r\nend\r\n",
        "select 2\r\n  case 1\r\n    r = 1\r\n  endcase\r\n  case default\r\n    r = 0\r\n  endcase\r\nendselect\r\nend\r\n",
        "s$ = \"x\" + \"y\"\r\nend\r\n",
        "gosub L\r\nend\r\n\r\nL:\r\n  a = 1\r\nreturn\r\n",
        "r = F(2)\r\nend\r\n\r\nfunction F(n)\r\nendfunction n * 2\r\n",
    };

    // Baseline fingerprints from a clean pass.
    std::vector<std::string> baseline;
    for (const auto& program : programs) {
        baseline.push_back(Fingerprint(CompileSnippet(program)));
    }

    // Soak: drive every program repeatedly, interleaved.
    for (int round = 0; round < 12; ++round) {
        for (const auto& program : programs) {
            const Snapshot snapshot = CompileSnippet(program);
            ASSERT_TRUE(snapshot.emitted)
                << "round " << round << " failed to emit for: " << program;
        }
    }

    // Output must be byte-identical to the baseline after the soak.
    for (std::size_t i = 0; i < programs.size(); ++i) {
        EXPECT_EQ(baseline[i], Fingerprint(CompileSnippet(programs[i])))
            << "program " << i << " produced different code after the soak:\n"
            << programs[i];
    }
}

// ---------------------------------------------------------------------------
// The compiler must never let an exception escape into the test runner. The
// harness is noexcept-free on purpose: if a std::bad_alloc or std::out_of_range
// escapes from the emission path, this test is where it is caught.
// ---------------------------------------------------------------------------
TEST_F(CodegenHeadlessTest, CompilationNeverThrows)
{
    const std::vector<std::string> programs = {
        "",
        "a = 1\r\nend\r\n",
        "dim a(10)\r\na(1) = 1\r\nend\r\n",
        "this is not valid basic at all\r\n",
        "type T\r\nendtype\r\nend\r\n",
        std::string("a$ = \"") + std::string(70000, 'q') + "\"\r\nend\r\n",
    };

    for (const auto& program : programs) {
        EXPECT_NO_THROW({ (void)CompileSnippet(program); })
            << "an exception escaped the compiler for input: " << program.substr(0, 64);
    }
}

// ---------------------------------------------------------------------------
// Timing guard: a regression that turns a linear pass into quadratic behaviour
// shows up here long before it becomes a timeout in CI.
// ---------------------------------------------------------------------------
TEST_F(CodegenHeadlessTest, CompilationTimeIsSane)
{
    // Warm up.
    for (int i = 0; i < 5; ++i) {
        (void)CompileSnippet(kSoakProgram);
    }

    double total = 0.0;
    constexpr int kRuns = 20;
    for (int i = 0; i < kRuns; ++i) {
        total += CompileSnippet(kSoakProgram).elapsedMs;
    }
    const double average = total / kRuns;

    EXPECT_LT(average, 2000.0)
        << "average compile took " << average << " ms — unexpected slow path in codegen";
    RecordProperty("average_compile_ms", std::to_string(average));
}

} // namespace
