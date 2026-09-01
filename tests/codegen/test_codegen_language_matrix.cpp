// test_codegen_language_matrix.cpp — coverage of DarkBASIC language constructs.
//
// The instruction matrix (test_codegen_instruction_matrix.cpp) proves that every
// *operator* lowers to clean machine code. This file proves the *control-flow
// and declaration constructs* do — the high-level grammar that the parser turns
// into statement trees before any operator is emitted:
//
//   L1  every control-flow construct emits a complete program (no early stop)
//   L2  every construct produces non-empty, fully-relocated machine code
//   L3  every forward branch target is resolved (no leap placeholder survives)
//   L4  emission is reproducible for each construct
//   L5  the construct does not leak process-global state to a later program
//   L6  deeply nested / combined constructs stay inside the buffer canary
//
// Each construct is exercised by both a minimal program and a "stress" program
// (nesting, combinations) so boundary structure is covered too. Adding a new
// language feature only means appending to kConstructCases below.

#include <gtest/gtest.h>

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "DBPLogger.h"
#include "codegen/CodegenHarness.h"

using namespace dbp::codegen;

namespace {

// A language-level construct and the programs that exercise it.
struct ConstructCase {
    const char* construct;       // stable test name (no spaces)
    const char* minimal;         // smallest well-formed program using it
    const char* stress;          // nested / combined variant (may be empty)
};

// DarkBASIC requires an explicit `end` (or the matching terminator) to close
// the program; the snippets below already carry their own terminators.
const ConstructCase kConstructCases[] = {
    // --- sequential / assignment -------------------------------------------
    {"SequentialStatements",
     "a = 1\r\nb = 2\r\nc = a + b\r\nend\r\n",
     "a = 1\r\nb = 2\r\nc = 3\r\nd = a + b * c\r\ne = d - a\r\nf = e / 2\r\nend\r\n"},

    // --- IF / ELSEIF / ELSE ------------------------------------------------
    {"IfElseChain",
     "a = 1\r\nif a = 1\r\n  b = 2\r\nelse\r\n  b = 3\r\nendif\r\nend\r\n",
     "a = 5\r\nif a = 1\r\n  b = 1\r\nelseif a = 2\r\n  b = 2\r\nelseif a = 5\r\n  b = 5\r\n"
     "else\r\n  b = 0\r\nendif\r\nend\r\n"},

    // --- nested IF ----------------------------------------------------------
    {"NestedIf",
     "a = 1\r\nb = 1\r\nif a = 1\r\n  if b = 1\r\n    c = 9\r\n  endif\r\nendif\r\nend\r\n",
     "a = 1\r\nb = 2\r\nc = 3\r\nif a = 1\r\n  if b = 2\r\n    if c = 3\r\n      d = 7\r\n"
     "    endif\r\n  endif\r\nendif\r\nend\r\n"},

    // --- WHILE / ENDWHILE ---------------------------------------------------
    {"WhileLoop",
     "a = 0\r\nwhile a < 5\r\n  a = a + 1\r\nendwhile\r\nend\r\n",
     "a = 0\r\nb = 0\r\nwhile a < 100\r\n  a = a + 1\r\n  while b < 10\r\n    b = b + 1\r\n"
     "  endwhile\r\nendwhile\r\nend\r\n"},

    // --- FOR / NEXT ---------------------------------------------------------
    {"ForNextLoop",
     "for a = 1 to 10\r\n  b = a\r\nnext a\r\nend\r\n",
     "for i = 1 to 5\r\n  for j = 1 to 5\r\n    k = i * j\r\n  next j\r\nnext i\r\nend\r\n"},

    // --- REPEAT / UNTIL -----------------------------------------------------
    {"RepeatUntilLoop",
     "a = 0\r\nrepeat\r\n  a = a + 1\r\nuntil a = 5\r\nend\r\n",
     "a = 0\r\nb = 0\r\nrepeat\r\n  a = a + 1\r\n  repeat\r\n    b = b + 1\r\n  until b = 3\r\n"
     "until a = 4\r\nend\r\n"},

    // --- DO / LOOP WHILE ----------------------------------------------------
    {"DoLoopWhile",
     "a = 0\r\ndo\r\n  a = a + 1\r\nloop while a < 5\r\nend\r\n",
     "a = 0\r\ndo\r\n  a = a + 1\r\n  do\r\n    b = a * 2\r\n  loop while b < 20\r\n"
     "loop while a < 3\r\nend\r\n"},

    // --- SELECT / CASE ------------------------------------------------------
    {"SelectCase",
     "a = 2\r\nselect a\r\ncase 1\r\n  b = 1\r\ncase 2\r\n  b = 2\r\nendselect\r\nend\r\n",
     "a = 3\r\nselect a\r\ncase 1\r\n  b = 1\r\ncase 2\r\n  b = 2\r\ncase 3\r\n  b = 3\r\n"
     "default\r\n  b = 0\r\nendselect\r\nend\r\n"},

    // --- GOSUB / RETURN -----------------------------------------------------
    {"GosubReturn",
     "gosub S\r\nend\r\n\r\nS:\r\n  a = 1\r\nreturn\r\n",
     "a = 0\r\ngosub S\r\ngosub S\r\nend\r\n\r\nS:\r\n  a = a + 1\r\nreturn\r\n"},

    // --- user function -------------------------------------------------------
    {"UserFunction",
     "r = F(3)\r\nend\r\n\r\nfunction F(x)\r\nendfunction x * 2\r\n",
     "a = G(2, 3)\r\nb = G(4, 5)\r\nend\r\n\r\nfunction G(p, q)\r\n"
     "  c = p + q\r\nendfunction c\r\n"},

    // --- EXIT / BREAK -------------------------------------------------------
    {"ExitLoop",
     "a = 0\r\nwhile a < 100\r\n  a = a + 1\r\n  if a = 10\r\n    exit\r\n  endif\r\n"
     "endwhile\r\nend\r\n",
     "for i = 1 to 100\r\n  if i = 50\r\n    exit\r\n  endif\r\nnext i\r\nend\r\n"},

    // --- CONTINUE / loop with early skip ------------------------------------
    {"ContinueLoop",
     "a = 0\r\nfor i = 1 to 10\r\n  if i = 5\r\n    continue\r\n  endif\r\n  a = a + i\r\n"
     "next i\r\nend\r\n",
     "a = 0\r\nwhile a < 20\r\n  a = a + 1\r\n  if a mod 2 = 0\r\n    continue\r\n  endif\r\n"
     "  b = a\r\nendwhile\r\nend\r\n"},

    // --- arrays (1D / 2D / 3D) ---------------------------------------------
    {"ArrayLifecycle",
     "dim a(10)\r\na(0) = 1\r\na(9) = 2\r\nb = a(0) + a(9)\r\nundim a()\r\nend\r\n",
     "dim m(3, 4)\r\ndim t(2, 2, 2)\r\nm(1, 2) = 7\r\nt(0, 1, 1) = 9\r\n"
     "x = m(1, 2) + t(0, 1, 1)\r\nend\r\n"},

    // --- UDT / TYPE ---------------------------------------------------------
    {"UdtType",
     "type T\r\n  v as integer\r\n  w# as float\r\nendtype\r\na as T\r\na.v = 1\r\n"
     "a.w# = 2.5\r\nb = a.v\r\nend\r\n",
     "type Point\r\n  x as integer\r\n  y as integer\r\nendtype\r\ntype Line\r\n"
     "  p as Point\r\n  q as Point\r\nendtype\r\nL as Line\r\nL.p.x = 1\r\nL.q.y = 2\r\n"
     "z = L.p.x + L.q.y\r\nend\r\n"},

    // --- constants -----------------------------------------------------------
    {"ConstantDecl",
     "#MAX = 10\r\na = #MAX\r\nb = #MAX * 2\r\nend\r\n",
     "#A = 1\r\n#B = 2\r\n#C = 3\r\nd = #A + #B + #C\r\nend\r\n"},

    // --- DATA / READ / RESTORE ----------------------------------------------
    {"DataReadRestore",
     "data 1, 2, 3\r\nread a\r\nread b\r\nread c\r\nend\r\n",
     "data 10, 20\r\ndata 30, 40\r\nrestore\r\nread a\r\nread b\r\nread c\r\n"
     "read d\r\nend\r\n"},

    // --- string handling -----------------------------------------------------
    {"StringOps",
     "a$ = \"hello\"\r\nb$ = \"world\"\r\nc$ = a$ + \" \" + b$\r\nif c$ = \"hello world\"\r\n"
     "  d = 1\r\nendif\r\nend\r\n",
     "s$ = \"\"\r\nfor i = 1 to 5\r\n  s$ = s$ + \"x\"\r\nnext i\r\nn = len(s$)\r\nend\r\n"},

    // --- mixed expression / precedence --------------------------------------
    {"MixedExpression",
     "a = 1 + 2 * 3 - 4 / 2\r\nb = (1 + 2) * (3 - 1)\r\nc = a > b and a < 10\r\nend\r\n",
     "x = 2 ^ 3 ^ 1\r\ny = 1 + 2 * 3 - 4 / 2 mod 3\r\nz = (x + y) * (a + 1) where a = 3\r\nend\r\n"},

    // --- combined control flow (stress integration) -------------------------
    {"CombinedControlFlow",
     "for i = 1 to 10\r\n  if i mod 2 = 0\r\n    a = a + i\r\n  else\r\n    b = b + i\r\n"
     "  endif\r\nnext i\r\nend\r\n",
     "while running = 1\r\n  for i = 1 to 5\r\n    select i\r\n    case 1\r\n      x = 1\r\n"
     "    case 2\r\n      x = 2\r\n    default\r\n      x = 0\r\n    endselect\r\n  next i\r\n"
     "  exit\r\nendwhile\r\nend\r\n"},

    // --- function with local array -----------------------------------------
    {"FunctionWithLocals",
     "r = Sum(5)\r\nend\r\n\r\nfunction Sum(n)\r\n  dim v(10)\r\n  v(n) = n\r\n"
     "endfunction v(n)\r\n",
     "r = Calc(4)\r\nend\r\n\r\nfunction Calc(n)\r\n  type T\r\n    v as integer\r\n"
     "  endtype\r\n  a as T\r\n  a.v = n * 2\r\nendfunction a.v\r\n"},

    // --- FOR with explicit STEP --------------------------------------------
    {"ForLoopWithStep",
     "for i = 0 to 10 step 2\r\n  a = a + i\r\nnext i\r\nend\r\n",
     "s = 0\r\nfor i = 1 to 100 step 3\r\n  s = s + i\r\nnext i\r\nend\r\n"},

    // --- FOR with negative (descending) step -------------------------------
    {"ForLoopReverse",
     "for i = 10 to 1 step -1\r\n  a = a + i\r\nnext i\r\nend\r\n",
     "p = 1\r\nfor j = 20 to 2 step -2\r\n  p = p * j\r\nnext j\r\nend\r\n"},

    // --- SELECT with computed (expression) case labels ---------------------
    {"SelectCaseComputed",
     "a = 2\r\nb = 1\r\nselect a\r\ncase b + 1\r\n  c = 1\r\ncase b * 2\r\n  c = 2\r\n"
     "default\r\n  c = 0\r\nendselect\r\nend\r\n",
     "v = 5\r\nselect v\r\ncase 1 + 1\r\n  w = 1\r\ncase 2 * 2\r\n  w = 2\r\n"
     "case 3 ^ 1\r\n  w = 3\r\ndefault\r\n  w = 9\r\nendselect\r\nend\r\n"},

    // --- nested SELECT inside SELECT ---------------------------------------
    {"NestedSelect",
     "a = 1\r\nselect a\r\ncase 1\r\n  select a\r\n  case 1\r\n    b = 1\r\n  endselect\r\n"
     "case 2\r\n  b = 2\r\nendselect\r\nend\r\n",
     "a = 2\r\nselect a\r\ncase 1\r\n  x = 1\r\ncase 2\r\n  select a\r\n  case 2\r\n    x = 2\r\n"
     "  endselect\r\nendselect\r\nend\r\n"},

    // --- array of UDT (from conformance: party(5) as PlayerInfo) ----------
    {"ArrayOfUdt",
     "type Player\r\n  name$ as string\r\n  hp as integer\r\nendtype\r\n"
     "dim party(5) as Player\r\nparty(1).name$ = \"Hero\"\r\nparty(1).hp = 100\r\n"
     "e$ = party(1).name$\r\nend\r\n",
     "type Pt\r\n  x as integer\r\n  y as integer\r\nendtype\r\n"
     "dim grid(3) as Pt\r\ngrid(0).x = 1\r\ngrid(0).y = 2\r\ngrid(1).x = grid(0).x + grid(0).y\r\n"
     "end\r\n"},

    // --- 4-dimensional array ------------------------------------------------
    {"Multidimensional4D",
     "dim hyper(3,3,3,3)\r\nhyper(1,2,3,1) = 9999\r\ne = hyper(1,2,3,1)\r\nend\r\n",
     "dim m(2,2,2,2)\r\nm(1,1,1,1) = 7\r\nr = m(1,1,1,1) + m(0,0,0,0)\r\nend\r\n"},

    // --- user function using the LOCAL keyword -----------------------------
    {"UserFunctionWithLocal",
     "r = F(3)\r\nend\r\n\r\nfunction F(x)\r\n  local t\r\n  t = x * 2\r\nendfunction t\r\n",
     "a = G(2, 4)\r\nend\r\n\r\nfunction G(p, q)\r\n  local acc\r\n  acc = p\r\n"
     "  acc = acc + q\r\nendfunction acc\r\n"},

    // --- recursive user function (single-line returns) ---------------------
    {"UserFunctionRecursive",
     "r = Fact(5)\r\nend\r\n\r\nfunction Fact(n)\r\n  if n <= 1 then exitfunction 1\r\n"
     "  v = n * Fact(n - 1)\r\nendfunction v\r\n",
     "s = Fib(8)\r\nend\r\n\r\nfunction Fib(n)\r\n  if n < 2 then exitfunction n\r\n"
     "  v = Fib(n - 1) + Fib(n - 2)\r\nendfunction v\r\n"},

    // --- GOSUB chains across multiple targets ------------------------------
    {"GosubChain",
     "a = 0\r\ngosub A\r\ngosub B\r\ngosub A\r\nend\r\n\r\nA:\r\n  a = a + 1\r\nreturn\r\n\r\n"
     "B:\r\n  a = a + 10\r\nreturn\r\n",
     "t = 0\r\ngosub X\r\ngosub Y\r\ngosub X\r\ngosub Y\r\nend\r\n\r\n"
     "X:\r\n  t = t + 1\r\nreturn\r\n\r\nY:\r\n  t = t + 2\r\nreturn\r\n"},

    // --- DO / LOOP UNTIL variant (post-condition) --------------------------
    {"DoLoopUntil",
     "a = 0\r\ndo\r\n  a = a + 1\r\nloop until a = 5\r\nend\r\n",
     "a = 0\r\ndo\r\n  a = a + 1\r\n  do\r\n    b = b + a\r\n  loop until b > 20\r\n"
     "loop until a = 10\r\nend\r\n"},

    // --- WHILE with a compound (AND) condition -----------------------------
    {"WhileWithCompoundCondition",
     "a = 0\r\nb = 10\r\nwhile a < 5 and b > 0\r\n  a = a + 1\r\n  b = b - 1\r\nendwhile\r\nend\r\n",
     "x = 0\r\ny = 0\r\nwhile x < 100 and y < 50\r\n  x = x + 1\r\n  y = y + 2\r\nendwhile\r\nend\r\n"},

    // --- REPEAT / UNTIL with a compound (OR) condition ---------------------
    {"RepeatUntilWithCompoundCondition",
     "a = 0\r\nb = 0\r\nrepeat\r\n  a = a + 1\r\n  b = b + 2\r\nuntil a = 5 or b = 20\r\nend\r\n",
     "i = 0\r\nj = 100\r\nrepeat\r\n  i = i + 1\r\n  j = j - 1\r\nuntil i > 10 or j < 0\r\nend\r\n"},

    // --- string array ------------------------------------------------------
    {"StringArray",
     "dim s$(10)\r\ns$(1) = \"alpha\"\r\ns$(2) = \"beta\"\r\nm$ = s$(1) + s$(2)\r\nend\r\n",
     "dim w$(5)\r\nfor k = 1 to 5\r\n  w$(k) = \"item\" + str$(k)\r\nnext k\r\n"
     "all$ = w$(1) + w$(5)\r\nend\r\n"},

    // --- EXIT from a nested loop (inner exit, outer continues) -------------
    {"ExitFromNestedLoop",
     "a = 0\r\nfor i = 1 to 10\r\n  for j = 1 to 10\r\n    if i * j > 20 then exit\r\n"
     "    a = a + 1\r\n  next j\r\nnext i\r\nend\r\n",
     "s = 0\r\nwhile s < 100\r\n  for k = 1 to 5\r\n    if k = 3 then exit\r\n    s = s + k\r\n"
     "  next k\r\n  s = s + 1\r\nendwhile\r\nend\r\n"},

    // --- CONTINUE in a nested loop -----------------------------------------
    {"ContinueInNestedLoop",
     "a = 0\r\nfor i = 1 to 10\r\n  if i mod 2 = 0 then continue\r\n  for j = 1 to 3\r\n"
     "    if j = 2 then continue\r\n    a = a + i * j\r\n  next j\r\nnext i\r\nend\r\n",
     "t = 0\r\nwhile t < 50\r\n  t = t + 1\r\n  if t mod 3 = 0 then continue\r\n"
     "  for m = 1 to 2\r\n    if m = 2 then continue\r\n    a = a + t\r\n  next m\r\n"
     "endwhile\r\nend\r\n"},
};

// L1+L2+L3: a construct must compile all the way to relocated machine code,
// with no unresolved leap placeholder and a non-empty body.
class CodegenConstructTest
    : public ::testing::TestWithParam<ConstructCase> {
protected:
    static void SetUpTestSuite()
    {
        DBPLogger::Initialize("test_codegen_language_matrix.log");
        EnsureEnvironment();
    }
    static void TearDownTestSuite() { spdlog::shutdown(); }
};

TEST_P(CodegenConstructTest, MinimalEmitsCompleteRelocatedCode)
{
    const ConstructCase& c = GetParam();
    const Snapshot snap = CompileSnippet(c.minimal);

    EXPECT_TRUE(snap.parsed)
        << c.construct << ": parsing failed at " << snap.stage << " ("
        << snap.errorMessage << ")";
    EXPECT_TRUE(snap.emitted)
        << c.construct << ": emission stopped at " << snap.stage << " ("
        << snap.errorMessage << ")";
    EXPECT_TRUE(snap.relocated)
        << c.construct << ": relocation failed (" << snap.errorMessage << ")";
    EXPECT_FALSE(snap.bytes.empty())
        << c.construct << ": the construct generated no machine code";

    for (const auto& ref : snap.refs) {
        if (ref.slotBytes == 4) {
            EXPECT_NE(ref.label, "0")
                << c.construct << ": unresolved leap placeholder at MCB+" << ref.offset;
        }
        for (std::size_t i = 0; i < snap.guardBytes.size(); ++i) {
            EXPECT_EQ(snap.guardBytes[i], 0xC3u)
                << c.construct << ": machine-code buffer canary overwritten at guard+" << i;
        }
    }
    RecordProperty("minimal_bytes", std::to_string(snap.bytes.size()));
}

TEST_P(CodegenConstructTest, StressEmitsCompleteRelocatedCode)
{
    const ConstructCase& c = GetParam();
    if (c.stress == nullptr || c.stress[0] == '\0') {
        GTEST_SKIP() << c.construct << ": no stress variant";
    }
    const Snapshot snap = CompileSnippet(c.stress);

    EXPECT_TRUE(snap.parsed)
        << c.construct << " (stress): parsing failed at " << snap.stage << " ("
        << snap.errorMessage << ")";
    EXPECT_TRUE(snap.emitted)
        << c.construct << " (stress): emission stopped at " << snap.stage << " ("
        << snap.errorMessage << ")";
    EXPECT_TRUE(snap.relocated)
        << c.construct << " (stress): relocation failed (" << snap.errorMessage << ")";
    EXPECT_FALSE(snap.bytes.empty())
        << c.construct << " (stress): generated no machine code";

    for (const auto& ref : snap.refs) {
        if (ref.slotBytes == 4) {
            EXPECT_NE(ref.label, "0")
                << c.construct << " (stress): unresolved leap placeholder at MCB+" << ref.offset;
        }
    }
    for (std::size_t i = 0; i < snap.guardBytes.size(); ++i) {
        EXPECT_EQ(snap.guardBytes[i], 0xC3u)
            << c.construct << " (stress): canary overwritten at guard+" << i;
    }
}

// L4: determinism — the same construct source must yield byte-identical output.
TEST_P(CodegenConstructTest, EmissionIsReproducible)
{
    const ConstructCase& c = GetParam();
    const Snapshot first = CompileSnippet(c.minimal);
    const Snapshot second = CompileSnippet(c.minimal);
    EXPECT_EQ(first.bytes, second.bytes)
        << c.construct << ": machine code is not reproducible across runs";

    if (c.stress != nullptr && c.stress[0] != '\0') {
        const Snapshot s1 = CompileSnippet(c.stress);
        const Snapshot s2 = CompileSnippet(c.stress);
        EXPECT_EQ(s1.bytes, s2.bytes)
            << c.construct << " (stress): machine code is not reproducible";
    }
}

// L5: global-state leakage across constructs. Compiling two different
// constructs in sequence must not perturb either's output.
TEST(CodegenConstructGlobalState, InterleavedConstructsDoNotLeakState)
{
    DBPLogger::Initialize("test_codegen_language_matrix.log");
    EnsureEnvironment();

    const ConstructCase& ca = kConstructCases[0];   // SequentialStatements
    const ConstructCase& cb = kConstructCases[6];   // SelectCase

    const Snapshot a1 = CompileSnippet(ca.minimal);
    const Snapshot b1 = CompileSnippet(cb.minimal);
    const Snapshot a2 = CompileSnippet(ca.minimal);

    EXPECT_EQ(Fingerprint(a1), Fingerprint(a2))
        << "compiling a '" << cb.construct << "' program changed the '"
        << ca.construct << "' output — global compiler state leaked";
    EXPECT_NE(Fingerprint(a1), Fingerprint(b1))
        << "two distinct constructs produced identical code (sanity check)";

    spdlog::shutdown();
}

// L6: deep nesting must stay inside the buffer canary (no write-cursor overrun
// on pathological nesting). Walk the worst-case nested-IF program.
TEST(CodegenConstructBoundary, DeeplyNestedIfStaysInsideCanary)
{
    DBPLogger::Initialize("test_codegen_language_matrix.log");
    EnsureEnvironment();

    std::string src;
    constexpr int depth = 64;
    for (int i = 0; i < depth; ++i) {
        src += "if a = " + std::to_string(i) + "\r\n";
    }
    for (int i = 0; i < depth; ++i) {
        src += "  a = a + 1\r\n";
    }
    for (int i = 0; i < depth; ++i) {
        src += "endif\r\n";
    }
    src += "end\r\n";

    const Snapshot snap = CompileSnippet(src);
    EXPECT_TRUE(snap.parsed) << "deeply nested IF failed to parse";
    EXPECT_TRUE(snap.emitted) << "deeply nested IF failed to emit";
    EXPECT_FALSE(snap.bytes.empty()) << "deeply nested IF emitted no code";
    for (std::size_t i = 0; i < snap.guardBytes.size(); ++i) {
        EXPECT_EQ(snap.guardBytes[i], 0xC3u)
            << "deeply nested IF overran the machine-code buffer canary at guard+" << i;
    }
    spdlog::shutdown();
}

INSTANTIATE_TEST_SUITE_P(
    CodegenConstructs,
    CodegenConstructTest,
    ::testing::ValuesIn(kConstructCases),
    [](const ::testing::TestParamInfo<ConstructCase>& info) {
        return std::string(info.param.construct);
    });

} // namespace
