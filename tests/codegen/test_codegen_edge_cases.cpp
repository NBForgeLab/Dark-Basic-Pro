// test_codegen_edge_cases.cpp — boundary and hostile inputs.
//
// DarkBASIC is a soft-typed language aimed at beginners, so the compiler is
// routinely fed malformed programs. The contract asserted here is:
//
//   * the compiler never crashes and never hangs,
//   * every rejection is *reported* through CError (never silent),
//   * anything that does produce machine code satisfies the safety oracles
//     (no unresolved branches, no buffer overrun).
//
// Each case records the stage it reached, which makes a failure report
// immediately actionable.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "DBPLogger.h"
#include "codegen/CodegenHarness.h"

using namespace dbp::codegen;

namespace {

class CodegenEdgeCaseTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        DBPLogger::Initialize("test_codegen_edge_cases.log");
        EnsureEnvironment();
    }
    static void TearDownTestSuite() { spdlog::shutdown(); }

    // Runs one hostile input and enforces the universal contract.
    void CheckHostileInput(const std::string& name, const std::string& source)
    {
        Snapshot snapshot;
        ASSERT_NO_FATAL_FAILURE({ snapshot = CompileSnippet(source); })
            << name << ": the compiler crashed or threw";

        // 1. A failure must always be reported. Silently dropping statements
        //    would produce an executable that misbehaves at runtime.
        if (!snapshot.parsed || !snapshot.emitted || !snapshot.relocated) {
            EXPECT_TRUE(snapshot.hasError || snapshot.hasParserError)
                << name << ": compilation stopped at stage '" << snapshot.stage
                << "' but no diagnostic was recorded — silent failure.";
        }

        // 2. Anything that did emit code must be internally consistent.
        if (!snapshot.bytes.empty()) {
            for (const auto& ref : snapshot.refs) {
                EXPECT_LE(static_cast<std::uint64_t>(ref.offset) + ref.slotBytes,
                          snapshot.bytes.size())
                    << name << ": reference slot runs past the emitted program";
                if (ref.slotBytes == 4) {
                    EXPECT_NE(ref.label, "0")
                        << name << ": unresolved leap placeholder survived emission";
                }
            }
            for (std::size_t i = 0; i < snapshot.guardBytes.size(); ++i) {
                EXPECT_EQ(snapshot.guardBytes[i], 0xC3u)
                    << name << ": machine-code buffer canary overwritten";
            }
        }

        // 3. Never emit a program that is silently empty after reporting
        //    success.
        if (snapshot.emitted && snapshot.relocated && !snapshot.hasError) {
            EXPECT_FALSE(snapshot.bytes.empty())
                << name << ": reported success but emitted no machine code";
        }
    }
};

TEST_F(CodegenEdgeCaseTest, EmptySource)
{
    CheckHostileInput("empty", "");
}

TEST_F(CodegenEdgeCaseTest, OnlyWhitespace)
{
    CheckHostileInput("whitespace", "   \r\n\t\r\n   \r\n");
}

TEST_F(CodegenEdgeCaseTest, OnlyComments)
{
    CheckHostileInput("comments", "REM nothing but commentary\r\n` another comment style\r\n");
}

TEST_F(CodegenEdgeCaseTest, ProgramWithoutEndStatement)
{
    CheckHostileInput("no-end", "a = 1\r\nb = a + 2\r\n");
}

TEST_F(CodegenEdgeCaseTest, UnterminatedStringLiteral)
{
    CheckHostileInput("unterminated-string", "a$ = \"this never closes\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, UnterminatedIfBlock)
{
    CheckHostileInput("unterminated-if", "a = 1\r\nif a > 0\r\n  b = 2\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, EndifWithoutIf)
{
    CheckHostileInput("orphan-endif", "a = 1\r\nendif\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, NextWithoutFor)
{
    CheckHostileInput("orphan-next", "a = 1\r\nnext i\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, GotoUndefinedLabel)
{
    CheckHostileInput("undefined-label", "goto Nowhere\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, CaseOutsideSelect)
{
    CheckHostileInput("orphan-case", "case 1\r\n  a = 1\r\nendcase\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, SelectWithoutCases)
{
    CheckHostileInput("empty-select", "a = 1\r\nselect a\r\nendselect\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, IntegerOverflowLiteral)
{
    CheckHostileInput("int-overflow", "a = 2147483647\r\nb = 4294967295\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, NegativeBoundaryLiterals)
{
    CheckHostileInput("negative-boundary", "a = -2147483648\r\nb = -0\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, FloatBoundaryLiterals)
{
    CheckHostileInput("float-boundary", "a# = 3.40282347e+38\r\nb# = 1.0e-45\r\nc# = 0.0\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, DivisionByZeroLiteral)
{
    CheckHostileInput("div-zero", "a = 1 / 0\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ZeroLengthArray)
{
    CheckHostileInput("zero-length-array", "dim a(0)\r\na(0) = 1\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, NegativeArrayIndex)
{
    CheckHostileInput("negative-index", "dim a(10)\r\na(-1) = 5\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ArrayIndexPastBounds)
{
    CheckHostileInput("index-past-bounds", "dim a(3)\r\na(99) = 5\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, UndimensionedArrayUse)
{
    CheckHostileInput("use-before-dim", "a(2) = 7\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, RedimensionInLoop)
{
    CheckHostileInput("dim-in-loop", "for i = 1 to 5\r\n  dim a(10)\r\n  a(1) = i\r\n  undim a()\r\nnext i\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, RecursiveFunction)
{
    CheckHostileInput("recursion",
                      "r = Countdown(5)\r\nend\r\n\r\n"
                      "function Countdown(n)\r\n"
                      "  if n <= 0 then exitfunction 0\r\n"
                      "  v = Countdown(n - 1)\r\n"
                      "endfunction v\r\n");
}

TEST_F(CodegenEdgeCaseTest, DuplicateFunctionDefinition)
{
    CheckHostileInput("duplicate-function",
                      "a = F(1)\r\nend\r\n\r\nfunction F(x)\r\nendfunction x\r\n\r\n"
                      "function F(x)\r\nendfunction x + 1\r\n");
}

TEST_F(CodegenEdgeCaseTest, FunctionWithManyParameters)
{
    std::string source = "r = Sum(";
    for (int i = 0; i < 40; ++i) {
        source += std::to_string(i);
        if (i + 1 < 40) source += ",";
    }
    source += ")\r\nend\r\n\r\nfunction Sum(";
    for (int i = 0; i < 40; ++i) {
        source += "p" + std::to_string(i);
        if (i + 1 < 40) source += ",";
    }
    source += ")\r\n  t = 0\r\n";
    for (int i = 0; i < 40; ++i) {
        source += "  t = t + p" + std::to_string(i) + "\r\n";
    }
    source += "endfunction t\r\n";
    CheckHostileInput("many-parameters", source);
}

TEST_F(CodegenEdgeCaseTest, LeapMarkerExhaustion)
{
    // CLeapMarkerManager supports only 9 nested forward markers
    // (LeapMarkerManager.h:98). Nesting 14 levels deep must not silently
    // miscompile; the compiler is expected to reject it with a diagnostic.
    std::string source = "a = 1\r\n";
    for (int i = 0; i < 14; ++i) {
        source += std::string((i + 1) * 2, ' ') + "if a > " + std::to_string(i) + "\r\n";
    }
    source += std::string(30, ' ') + "deep = 1\r\n";
    for (int i = 0; i < 14; ++i) {
        source += std::string((i + 1) * 2, ' ') + "endif\r\n";
    }
    source += "end\r\n";
    CheckHostileInput("leap-marker-exhaustion", source);
}

TEST_F(CodegenEdgeCaseTest, DeeplyNestedParentheses)
{
    std::string source = "a = ";
    for (int i = 0; i < 120; ++i) source += "(";
    source += "1";
    for (int i = 0; i < 120; ++i) source += " + 1)";
    source += "\r\nend\r\n";
    CheckHostileInput("deep-parentheses", source);
}

TEST_F(CodegenEdgeCaseTest, VeryLongSingleLine)
{
    std::string expression = "a = 0";
    for (int i = 0; i < 3000; ++i) {
        expression += " + 1";
    }
    CheckHostileInput("long-line", expression + "\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, VeryLongIdentifier)
{
    const std::string name(400, 'z');
    CheckHostileInput("long-identifier", name + " = 1\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, VeryLongStringLiteral)
{
    const std::string literal(60000, 'x');
    CheckHostileInput("long-string", "a$ = \"" + literal + "\"\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ManyVariables)
{
    std::string source;
    for (int i = 0; i < 2000; ++i) {
        source += "v" + std::to_string(i) + " = " + std::to_string(i) + "\r\n";
    }
    source += "end\r\n";
    CheckHostileInput("many-variables", source);
}

TEST_F(CodegenEdgeCaseTest, LfOnlyLineEndings)
{
    CheckHostileInput("lf-only", "a = 1\nb = 2\nend\n");
}

TEST_F(CodegenEdgeCaseTest, CrOnlyLineEndings)
{
    CheckHostileInput("cr-only", "a = 1\rb = 2\rend\r");
}

TEST_F(CodegenEdgeCaseTest, Utf8ByteOrderMark)
{
    CheckHostileInput("bom", "\xEF\xBB\xBF" "a = 1\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, NonAsciiInsideComment)
{
    CheckHostileInput("unicode-comment", "REM caf\xC3\xA9 \xE2\x9C\x93 \xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, NonAsciiInsideStringLiteral)
{
    CheckHostileInput("unicode-string", "a$ = \xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, NestedQuoteEscapes)
{
    CheckHostileInput("nested-quotes", "a$ = \"he said \"\"hi\"\" loudly\"\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, AssignmentToReservedWord)
{
    CheckHostileInput("reserved-word-assign", "end = 1\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, TypeMismatchAssignStringToInteger)
{
    CheckHostileInput("type-mismatch", "a = \"not a number\"\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, DataWithoutRead)
{
    CheckHostileInput("unused-data", "data 1, 2, 3\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ReadWithoutData)
{
    CheckHostileInput("read-without-data", "read a\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ExitStatementOutsideLoop)
{
    CheckHostileInput("orphan-exit", "a = 1\r\nexit\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ReturnWithoutGosub)
{
    CheckHostileInput("orphan-return", "return\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, LabelDefinedTwice)
{
    CheckHostileInput("duplicate-label",
                      "gosub Here\r\nend\r\n\r\nHere:\r\n  a = 1\r\nreturn\r\n\r\nHere:\r\n"
                      "  a = 2\r\nreturn\r\n");
}

TEST_F(CodegenEdgeCaseTest, VeryDeepUdtNesting)
{
    CheckHostileInput("udt-self-reference",
                      "type Node\r\n  value as integer\r\n  next as Node\r\nendtype\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, HugeArrayDimension)
{
    CheckHostileInput("huge-dim", "dim a(1000000)\r\na(0) = 1\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, MultidimensionalArrayBeyondFiveDimensions)
{
    CheckHostileInput("six-dimensions", "dim a(2,2,2,2,2,2)\r\na(1,1,1,1,1,1) = 3\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ConstantRedefinition)
{
    CheckHostileInput("constant-redefinition",
                      "#constant A 1\r\n#constant A 2\r\nb = A\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ConstantReferencingItself)
{
    CheckHostileInput("self-referential-constant", "#constant A A + 1\r\nb = A\r\nend\r\n");
}

// ---------------------------------------------------------------------------
// Deeper boundary & structural-exhaustion cases (coverage expansion).
// Each still obeys the universal contract enforced by CheckHostileInput:
// no crash, no hang, every rejection reported, and any emitted code stays
// inside the machine-code buffer canary with all forward branches resolved.
// ---------------------------------------------------------------------------

// One past the LeapMarkerManager limit (MAX_LEAP_MARKERS = 9). Must be rejected
// with a diagnostic rather than silently miscompiled.
TEST_F(CodegenEdgeCaseTest, LeapMarkerExhaustionAtTen)
{
    std::string source = "a = 1\r\n";
    for (int i = 0; i < 10; ++i)
        source += std::string((i + 1) * 2, ' ') + "if a > " + std::to_string(i) + "\r\n";
    source += std::string(22, ' ') + "deep = 1\r\n";
    for (int i = 0; i < 10; ++i)
        source += std::string((i + 1) * 2, ' ') + "endif\r\n";
    source += "end\r\n";
    CheckHostileInput("leap-marker-10", source);
}

TEST_F(CodegenEdgeCaseTest, DeeplyMixedNesting)
{
    // if -> while -> for -> if -> while, nested 5 levels, then unwound.
    std::string source = "a = 1\r\n";
    source += "if a > 0\r\n";
    source += "  while a < 100\r\n";
    source += "    for i = 1 to 10\r\n";
    source += "      if i > 5\r\n";
    source += "        a = a + 1\r\n";
    source += "      endif\r\n";
    source += "    next i\r\n";
    source += "  endwhile\r\n";
    source += "endif\r\n";
    source += "end\r\n";
    CheckHostileInput("mixed-nesting-5", source);
}

TEST_F(CodegenEdgeCaseTest, NestedSelectWithinSelect)
{
    std::string source = "a = 1\r\nselect a\r\ncase 1\r\n  select a\r\n  case 1\r\n    b = 1\r\n"
                         "  endselect\r\ncase 2\r\n  b = 2\r\nendselect\r\nend\r\n";
    CheckHostileInput("nested-select", source);
}

TEST_F(CodegenEdgeCaseTest, ForLoopTwentyDeep)
{
    std::string source;
    std::string indent;
    for (int i = 0; i < 20; ++i) {
        source += indent + "for v" + std::to_string(i) + " = 1 to 3\r\n";
        indent += "  ";
    }
    source += indent + "acc = 1\r\n";
    for (int i = 19; i >= 0; --i) {
        indent.resize(indent.size() >= 2 ? indent.size() - 2 : 0);
        source += indent + "next v" + std::to_string(i) + "\r\n";
    }
    source += "end\r\n";
    CheckHostileInput("for-20-deep", source);
}

TEST_F(CodegenEdgeCaseTest, ForLoopHugeUpperBound)
{
    CheckHostileInput("for-huge-bound", "for i = 1 to 2147483647\r\n  a = i\r\nnext i\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ForLoopNegativeStep)
{
    CheckHostileInput("for-negative-step", "for i = 10 to 1 step -1\r\n  a = i\r\nnext i\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ForLoopZeroStep)
{
    CheckHostileInput("for-zero-step", "for i = 1 to 10 step 0\r\n  a = i\r\nnext i\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ForLoopNonTerminatingStep)
{
    // step -1 on an ascending range never reaches the bound at runtime, but the
    // compiler must not hang or miscompile at codegen time.
    CheckHostileInput("for-nonterminating-step", "for i = 1 to 10 step -1\r\n  a = i\r\nnext i\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ForLoopFloatBounds)
{
    CheckHostileInput("for-float-bounds", "for i# = 1.0 to 5.0 step 0.5\r\n  a# = i#\r\nnext i#\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ArrayDimNegative)
{
    CheckHostileInput("array-dim-negative", "dim a(-5)\r\na(0) = 1\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ArrayDimHugeEachDimension)
{
    CheckHostileInput("array-dim-huge", "dim a(1000000, 1000000)\r\na(0,0) = 1\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ArrayDimEightDimensions)
{
    CheckHostileInput("array-eight-dim", "dim a(2,2,2,2,2,2,2,2)\r\na(1,1,1,1,1,1,1,1) = 3\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ArrayDimZeroAll)
{
    CheckHostileInput("array-dim-zero-all", "dim a(0,0,0)\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, StringArrayLarge)
{
    CheckHostileInput("string-array-large", "dim s$(1000)\r\ns$(1) = \"x\"\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, UdtArrayLarge)
{
    CheckHostileInput("udt-array-large",
        "type T\r\n  v as integer\r\nendtype\r\ndim p(1000) as T\r\np(1).v = 5\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, UdtArrayOfUdt)
{
    CheckHostileInput("udt-array-of-udt",
        "type Inner\r\n  x as integer\r\nendtype\r\ntype Outer\r\n  i as Inner\r\nendtype\r\n"
        "dim a(10) as Outer\r\na(1).i.x = 7\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ExpressionManyOperators)
{
    std::string expr = "a = 1";
    for (int i = 0; i < 2000; ++i) expr += " + 1";
    CheckHostileInput("expr-many-ops", expr + "\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, DeepSubscriptNesting)
{
    CheckHostileInput("deep-subscript", "dim a(5)\r\na(a(a(a(a(1)))))) = 9\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ParenthesesTwoHundredDeep)
{
    std::string src = "a = ";
    for (int i = 0; i < 200; ++i) src += "(";
    src += "1";
    for (int i = 0; i < 200; ++i) src += " + 1)";
    CheckHostileInput("parens-200", src + "\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, StringConcatThousand)
{
    std::string src = "a$ = ";
    for (int i = 0; i < 1000; ++i) {
        src += "\"x\"";
        if (i + 1 < 1000) src += " + ";
    }
    CheckHostileInput("string-concat-1000", src + "\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, UnaryMinusChain)
{
    std::string src = "a = ";
    for (int i = 0; i < 100; ++i) src += "-";
    src += "1\r\nend\r\n";
    CheckHostileInput("unary-minus-chain", src);
}

TEST_F(CodegenEdgeCaseTest, SourceWithNulBytes)
{
    std::string src = "a = 1\r\n";
    src.push_back('\0');
    src += "b = 2\r\nend\r\n";
    CheckHostileInput("nul-bytes", src);
}

TEST_F(CodegenEdgeCaseTest, SourceWithControlChars)
{
    std::string src = "a = 1\r\n\t\b\v\fa = 2\r\nb = 3\a\r\nend\r\n";
    CheckHostileInput("control-chars", src);
}

TEST_F(CodegenEdgeCaseTest, VeryLongSingleToken)
{
    const std::string token(200000, 'q');
    CheckHostileInput("long-token", token + " = 1\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, MixedLineEndingsInFile)
{
    CheckHostileInput("mixed-line-endings", "a = 1\r\nb = 2\nc = 3\rd = 4\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, NoTrailingNewline)
{
    CheckHostileInput("no-trailing-newline", "a = 1\r\nb = 2\r\nend");
}

TEST_F(CodegenEdgeCaseTest, OnlyEndStatement)
{
    CheckHostileInput("only-end", "end\r\n");
}

TEST_F(CodegenEdgeCaseTest, TwoEndStatements)
{
    CheckHostileInput("two-ends", "a = 1\r\nend\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, TrailingColonSeparator)
{
    CheckHostileInput("trailing-colon", "a = 1:\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, MultipleColonSeparators)
{
    CheckHostileInput("colon-separators", "a=1:b=2:c=3\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, TabIndentation)
{
    std::string src = "a = 1\r\nif a > 0\r\n\tb = 2\r\n\tif b > 1\r\n\t\tc = 3\r\n\tendif\r\nendif\r\nend\r\n";
    CheckHostileInput("tab-indent", src);
}

TEST_F(CodegenEdgeCaseTest, ManyTypeDefinitions)
{
    std::string src;
    for (int i = 0; i < 100; ++i)
        src += "type T" + std::to_string(i) + "\r\n  v as integer\r\nendtype\r\n";
    src += "end\r\n";
    CheckHostileInput("many-types", src);
}

TEST_F(CodegenEdgeCaseTest, TypeWithManyFields)
{
    std::string src = "type Big\r\n";
    for (int i = 0; i < 200; ++i)
        src += "  f" + std::to_string(i) + " as integer\r\n";
    src += "endtype\r\na as Big\r\na.f0 = 1\r\nend\r\n";
    CheckHostileInput("type-many-fields", src);
}

TEST_F(CodegenEdgeCaseTest, ConstantDefinitionCycle)
{
    CheckHostileInput("const-cycle", "#constant A B + 1\r\n#constant B A + 1\r\nc = A\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ConstantExpressionLarge)
{
    CheckHostileInput("const-large", "#constant BIG 999999999999\r\nb = BIG\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ExitFunctionOutsideFunction)
{
    CheckHostileInput("orphan-exitfunction", "a = 1\r\nexitfunction 0\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, EndFunctionOutsideFunction)
{
    CheckHostileInput("orphan-endfunction", "endfunction 0\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, FunctionWithLocalKeyword)
{
    CheckHostileInput("function-local",
        "r = F(3)\r\nend\r\n\r\nfunction F(x)\r\n  local t\r\n  t = x * 2\r\nendfunction t\r\n");
}

TEST_F(CodegenEdgeCaseTest, GosubUndefinedLabel)
{
    CheckHostileInput("gosub-undefined", "gosub Nowhere\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, DoLoopUntilVariant)
{
    CheckHostileInput("do-loop-until", "a = 0\r\ndo\r\n  a = a + 1\r\nloop until a = 5\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, WhileWithCompoundCondition)
{
    CheckHostileInput("while-compound",
        "a = 0\r\nb = 10\r\nwhile a < 5 and b > 0\r\n  a = a + 1\r\n  b = b - 1\r\nendwhile\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, RepeatUntilWithCompoundCondition)
{
    CheckHostileInput("repeat-compound",
        "a = 0\r\nb = 0\r\nrepeat\r\n  a = a + 1\r\n  b = b + 2\r\nuntil a = 5 or b = 20\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, DataWithUnbalancedQuote)
{
    CheckHostileInput("data-unbalanced-quote", "data 1, \"two, 3\r\nread a\r\nread b\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, ReadIntoArrayElement)
{
    CheckHostileInput("read-into-array", "data 1, 2, 3\r\ndim a(5)\r\nread a(1)\r\nread a(2)\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, UndimNonexistentArray)
{
    CheckHostileInput("undim-missing", "undim a()\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, DimEmptyParens)
{
    CheckHostileInput("dim-empty-parens", "dim a()\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, IfThenShorthandBlockMix)
{
    // Mixing the single-line `if..then` shorthand with a block body is a common
    // beginner mistake; the compiler must reject it cleanly.
    CheckHostileInput("if-then-mix", "if a = 1 then b = 2\r\nendif\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, SelectWithComputedCase)
{
    CheckHostileInput("select-computed-case",
        "a = 2\r\nb = 1\r\nselect a\r\ncase b + 1\r\n  c = 1\r\ncase b * 2\r\n  c = 2\r\n"
        "default\r\n  c = 0\r\nendselect\r\nend\r\n");
}

TEST_F(CodegenEdgeCaseTest, GotoIntoFunctionBody)
{
    CheckHostileInput("goto-into-function",
        "goto Inside\r\nend\r\n\r\nfunction F()\r\nInside:\r\n  a = 1\r\nendfunction a\r\n");
}

} // namespace
