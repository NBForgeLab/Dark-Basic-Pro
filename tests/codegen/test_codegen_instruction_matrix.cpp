// test_codegen_instruction_matrix.cpp — coverage of the instruction set.
//
// Two independent sweeps:
//
//  A. Database sweep: walk every one of the IT_INTERNAL_MAXCOUNT internal
//     instruction slots, verify the registered entries are well formed and
//     resolvable, and detect duplicate registrations (an ambiguous overload
//     is a classic source of "the wrong command gets called at runtime").
//
//  B. Emission sweep: a table of DarkBASIC snippets, one per instruction
//     family reachable from source, each compiled end-to-end and checked for
//     clean emission, non-empty machine code and fully resolved references.
//
// Regenerating the language does not require touching this file: add the
// instruction to the InternalInstruction enum and its family to the table.

#include <gtest/gtest.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "DBPLogger.h"
#include "InstructionTable.h"
#include "codegen/CodegenHarness.h"

using namespace dbp::codegen;

extern CInstructionTable* g_pInstructionTable;

namespace {

// ---------------------------------------------------------------------------
// A. Instruction-database sweep
// ---------------------------------------------------------------------------

struct DatabaseSweep {
    int totalSlots = 0;
    int registered = 0;
    int emptyNames = 0;
    int excessiveParams = 0;
    int duplicates = 0;
    std::vector<std::string> duplicateKeys;
    std::vector<std::string> unresolvable;
};

DatabaseSweep SweepInstructionDatabase()
{
    DatabaseSweep sweep;
    std::map<std::string, int> seen;

    for (DWORD index = 0; index < IT_INTERNAL_MAXCOUNT; ++index) {
        ++sweep.totalSlots;
        CInstructionTableEntry* entry = g_pInstructionTable->GetRef(index);
        if (entry == nullptr) {
            continue;
        }
        ++sweep.registered;

        const std::string name(entry->GetNameView());
        if (name.empty()) {
            ++sweep.emptyNames;
        }

        // The x64 command-call ABI marshals arguments into four registers plus
        // a stack area; AddCommandCore2 caps the count. Anything above the
        // widest internal command is suspicious.
        if (entry->GetParamMax() > 11) {
            ++sweep.excessiveParams;
        }

        const std::string signature = name + "/" + std::string(entry->GetParamTypesView());
        if (++seen[signature] > 1) {
            ++sweep.duplicates;
            if (sweep.duplicateKeys.size() < 20) {
                sweep.duplicateKeys.push_back(signature);
            }
        }

        // The name must be resolvable back through the public lookup path.
        DWORD data = 0;
        DWORD paramMax = 0;
        DWORD length = 0;
        CInstructionTableEntry* resolved = nullptr;
        if (!g_pInstructionTable->FindInstruction(false, name.c_str(), 0, &data, &paramMax,
                                                  &length, &resolved)) {
            if (sweep.unresolvable.size() < 20) {
                sweep.unresolvable.push_back(name);
            }
        }
    }
    return sweep;
}

class CodegenInstructionDatabaseTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        DBPLogger::Initialize("test_codegen_instruction_matrix.log");
        EnsureEnvironment();
    }
    static void TearDownTestSuite() { spdlog::shutdown(); }
};

TEST_F(CodegenInstructionDatabaseTest, InternalDatabaseIsPopulated)
{
    const DatabaseSweep sweep = SweepInstructionDatabase();
    EXPECT_GE(sweep.registered, 150)
        << "only " << sweep.registered << " internal instructions are registered; the "
           "database failed to initialize";
    RecordProperty("registered_instructions", std::to_string(sweep.registered));
}

TEST_F(CodegenInstructionDatabaseTest, EveryEntryHasAName)
{
    const DatabaseSweep sweep = SweepInstructionDatabase();
    EXPECT_EQ(sweep.emptyNames, 0)
        << sweep.emptyNames << " registered instruction entries have an empty name";
}

TEST_F(CodegenInstructionDatabaseTest, NoEntryExceedsTheParameterLimit)
{
    const DatabaseSweep sweep = SweepInstructionDatabase();
    EXPECT_EQ(sweep.excessiveParams, 0)
        << sweep.excessiveParams << " instruction entries declare more than 11 parameters, "
           "which the x64 call marshaller cannot encode";
}

TEST_F(CodegenInstructionDatabaseTest, NoAmbiguousDuplicateRegistrations)
{
    const DatabaseSweep sweep = SweepInstructionDatabase();
    if (!sweep.duplicateKeys.empty()) {
        std::string joined;
        for (const auto& key : sweep.duplicateKeys) {
            joined += "\n  " + key;
        }
        ADD_FAILURE() << sweep.duplicates << " instruction signatures are registered more "
                      << "than once, making overload resolution ambiguous:" << joined;
    }
}

TEST_F(CodegenInstructionDatabaseTest, EveryRegisteredNameResolves)
{
    const DatabaseSweep sweep = SweepInstructionDatabase();
    if (!sweep.unresolvable.empty()) {
        std::string joined;
        for (const auto& name : sweep.unresolvable) {
            joined += "\n  " + name;
        }
        ADD_FAILURE() << sweep.unresolvable.size()
                      << " registered instruction names cannot be found by "
                         "FindInstruction():" << joined;
    }
}

// ---------------------------------------------------------------------------
// B. Emission sweep — one source snippet per instruction family.
//
// Columns: family name, the snippet body (an `end` is appended), and whether
// the construct is expected to compile.
// ---------------------------------------------------------------------------

struct InstructionCase {
    const char* family;
    const char* source;
};

const InstructionCase kInstructionCases[] = {
    // --- integer arithmetic ------------------------------------------------
    {"IntAdd",       "a = 1\r\nb = 2\r\nc = a + b\r\n"},
    {"IntSub",       "a = 1\r\nb = 2\r\nc = a - b\r\n"},
    {"IntMul",       "a = 1\r\nb = 2\r\nc = a * b\r\n"},
    {"IntDiv",       "a = 1\r\nb = 2\r\nc = a / b\r\n"},
    {"IntMod",       "a = 1\r\nb = 2\r\nc = a mod b\r\n"},
    {"IntPower",     "a = 2\r\nc = a ^ 3\r\n"},
    // --- float arithmetic --------------------------------------------------
    {"FloatAdd",     "a# = 1.5\r\nb# = 2.5\r\nc# = a# + b#\r\n"},
    {"FloatSub",     "a# = 1.5\r\nb# = 2.5\r\nc# = a# - b#\r\n"},
    {"FloatMul",     "a# = 1.5\r\nb# = 2.5\r\nc# = a# * b#\r\n"},
    {"FloatDiv",     "a# = 1.5\r\nb# = 2.5\r\nc# = a# / b#\r\n"},
    {"FloatPower",   "a# = 2.0\r\nc# = a# ^ 0.5\r\n"},
    // --- double-float arithmetic -------------------------------------------
    {"DoubleAdd",    "a as double float\r\nb as double float\r\na = 1\r\nb = 2\r\nc = a + b\r\n"},
    {"DoubleMul",    "a as double float\r\nb as double float\r\na = 1\r\nb = 2\r\nc = a * b\r\n"},
    // --- double-integer arithmetic -----------------------------------------
    {"DintAdd",      "a as double integer\r\nb as double integer\r\na = 1\r\nb = 2\r\nc = a + b\r\n"},
    {"DintMul",      "a as double integer\r\nb as double integer\r\na = 1\r\nb = 2\r\nc = a * b\r\n"},
    // --- byte / word / dword arithmetic ------------------------------------
    {"ByteAdd",      "a as byte\r\nb as byte\r\na = 1\r\nb = 2\r\nc = a + b\r\n"},
    {"ByteMul",      "a as byte\r\nb as byte\r\na = 1\r\nb = 2\r\nc = a * b\r\n"},
    {"WordAdd",      "a as word\r\nb as word\r\na = 1\r\nb = 2\r\nc = a + b\r\n"},
    {"WordMul",      "a as word\r\nb as word\r\na = 1\r\nb = 2\r\nc = a * b\r\n"},
    {"DwordAdd",     "a as dword\r\nb as dword\r\na = 1\r\nb = 2\r\nc = a + b\r\n"},
    {"DwordMul",     "a as dword\r\nb as dword\r\na = 1\r\nb = 2\r\nc = a * b\r\n"},
    {"DwordMod",     "a as dword\r\nb as dword\r\na = 7\r\nb = 2\r\nc = a mod b\r\n"},
    // --- boolean arithmetic ------------------------------------------------
    {"BoolAnd",      "a as boolean\r\nb as boolean\r\na = 1\r\nb = 1\r\nc = a and b\r\n"},
    {"BoolOr",       "a as boolean\r\nb as boolean\r\na = 1\r\nb = 0\r\nc = a or b\r\n"},
    {"BoolNot",      "a as boolean\r\na = 1\r\nb = not a\r\n"},
    // --- comparison families ------------------------------------------------
    {"CmpIntEq",     "a = 1\r\nb = 2\r\nc = (a = b)\r\n"},
    {"CmpIntNe",     "a = 1\r\nb = 2\r\nc = (a <> b)\r\n"},
    {"CmpIntLt",     "a = 1\r\nb = 2\r\nc = (a < b)\r\n"},
    {"CmpIntGt",     "a = 1\r\nb = 2\r\nc = (a > b)\r\n"},
    {"CmpIntLe",     "a = 1\r\nb = 2\r\nc = (a <= b)\r\n"},
    {"CmpIntGe",     "a = 1\r\nb = 2\r\nc = (a >= b)\r\n"},
    {"CmpFloatEq",   "a# = 1.0\r\nb# = 2.0\r\nc = (a# = b#)\r\n"},
    {"CmpFloatLt",   "a# = 1.0\r\nb# = 2.0\r\nc = (a# < b#)\r\n"},
    {"CmpStringEq",  "a$ = \"x\"\r\nb$ = \"y\"\r\nc = (a$ = b$)\r\n"},
    {"CmpStringLt",  "a$ = \"x\"\r\nb$ = \"y\"\r\nc = (a$ < b$)\r\n"},
    {"CmpDwordLt",   "a as dword\r\nb as dword\r\na = 1\r\nb = 2\r\nc = (a < b)\r\n"},
    {"CmpDoubleLt",  "a as double float\r\nb as double float\r\na = 1\r\nb = 2\r\nc = (a < b)\r\n"},
    // --- bitwise families ---------------------------------------------------
    {"Shl",          "a = 1\r\nb = a << 3\r\n"},
    {"Shr",          "a = 8\r\nb = a >> 3\r\n"},
    {"BitAnd",       "a = 12\r\nb = 10\r\nc = a && b\r\n"},
    {"BitOr",        "a = 12\r\nb = 10\r\nc = a || b\r\n"},
    {"BitXor",       "a = 12\r\nb = 10\r\nc = a ~~ b\r\n"},
    {"BitNot",       "a = 12\r\nb = ~~a\r\n"},
    // --- cast families -------------------------------------------------------
    {"CastIntToFloat",   "a = 3\r\nb# = a\r\n"},
    {"CastFloatToInt",   "a# = 3.7\r\nb = a#\r\n"},
    {"CastIntToDword",   "a = 3\r\nb as dword\r\nb = a\r\n"},
    {"CastDwordToInt",   "a as dword\r\na = 3\r\nb = a\r\n"},
    {"CastIntToDouble",  "a = 3\r\nb as double float\r\nb = a\r\n"},
    {"CastDoubleToInt",  "a as double float\r\na = 3\r\nb = a\r\n"},
    {"CastIntToByte",    "a = 3\r\nb as byte\r\nb = a\r\n"},
    {"CastByteToInt",    "a as byte\r\na = 3\r\nb = a\r\n"},
    {"CastIntToWord",    "a = 3\r\nb as word\r\nb = a\r\n"},
    {"CastWordToInt",    "a as word\r\na = 3\r\nb = a\r\n"},
    {"CastIntToDint",    "a = 3\r\nb as double integer\r\nb = a\r\n"},
    {"CastDintToInt",    "a as double integer\r\na = 3\r\nb = a\r\n"},
    // --- assignment families -------------------------------------------------
    {"AssignInt",    "a = 1\r\n"},
    {"AssignFloat",  "a# = 1.5\r\n"},
    {"AssignString", "a$ = \"text\"\r\n"},
    {"AssignBool",   "a as boolean\r\na = 1\r\n"},
    {"AssignByte",   "a as byte\r\na = 1\r\n"},
    {"AssignWord",   "a as word\r\na = 1\r\n"},
    {"AssignDword",  "a as dword\r\na = 1\r\n"},
    {"AssignDouble", "a as double float\r\na = 1\r\n"},
    {"AssignDint",   "a as double integer\r\na = 1\r\n"},
    // --- inc / dec ------------------------------------------------------------
    {"IncInt",       "a = 1\r\ninc a\r\n"},
    {"DecInt",       "a = 1\r\ndec a\r\n"},
    {"IncQuantity",  "a = 1\r\ninc a, 5\r\n"},
    {"DecQuantity",  "a = 10\r\ndec a, 5\r\n"},
    {"IncFloat",     "a# = 1.0\r\ninc a#\r\n"},
    {"DecFloat",     "a# = 1.0\r\ndec a#\r\n"},
    // --- allocation -----------------------------------------------------------
    {"ArrayAlloc",   "dim a(10)\r\na(0) = 1\r\n"},
    {"ArrayFree",    "dim a(10)\r\nundim a()\r\n"},
    {"ArrayAlloc2D", "dim a(3, 4)\r\na(1, 2) = 1\r\n"},
    // --- udt assignment --------------------------------------------------------
    {"UdtAssign",    "type T\r\n  v as integer\r\nendtype\r\na as T\r\nb as T\r\n"
                     "a.v = 1\r\nb = a\r\n"},
    // --- string runtime helpers -------------------------------------------------
    {"StringConcat", "a$ = \"x\" + \"y\"\r\n"},
    {"StringCopy",   "a$ = \"x\"\r\nb$ = a$\r\n"},
    // --- control-flow primitives ---------------------------------------------------
    {"Return",       "gosub S\r\nend\r\n\r\nS:\r\n  a = 1\r\nreturn\r\n"},
    {"PureReturn",   "end\r\n"},
    {"Sync",         "sync\r\nend\r\n"},
    {"EndProgram",   "end\r\n"},
    {"UserFunctionExit", "r = F(1)\r\nend\r\n\r\nfunction F(x)\r\nendfunction x\r\n"},
};

class CodegenInstructionEmissionTest
    : public ::testing::TestWithParam<InstructionCase> {
protected:
    static void SetUpTestSuite()
    {
        DBPLogger::Initialize("test_codegen_instruction_matrix.log");
        EnsureEnvironment();
    }
    static void TearDownTestSuite() { spdlog::shutdown(); }
};

TEST_P(CodegenInstructionEmissionTest, EmitsCleanMachineCode)
{
    const InstructionCase& testCase = GetParam();
    const std::string source = std::string(testCase.source) + "end\r\n";

    const Snapshot snapshot = CompileSnippet(source);

    EXPECT_TRUE(snapshot.parsed)
        << testCase.family << ": parsing failed at " << snapshot.stage << " ("
        << snapshot.errorMessage << ")";
    EXPECT_TRUE(snapshot.emitted)
        << testCase.family << ": emission stopped at " << snapshot.stage << " ("
        << snapshot.errorMessage << ")";
    EXPECT_TRUE(snapshot.relocated)
        << testCase.family << ": relocation failed (" << snapshot.errorMessage << ")";
    EXPECT_FALSE(snapshot.bytes.empty())
        << testCase.family << ": the instruction generated no machine code";

    for (const auto& ref : snapshot.refs) {
        if (ref.slotBytes == 4) {
            EXPECT_NE(ref.label, "0")
                << testCase.family << ": unresolved leap placeholder at MCB+" << ref.offset;
        }
    }
    for (std::size_t i = 0; i < snapshot.guardBytes.size(); ++i) {
        EXPECT_EQ(snapshot.guardBytes[i], 0xC3u)
            << testCase.family << ": machine-code buffer canary overwritten";
    }
}

TEST_P(CodegenInstructionEmissionTest, EmissionIsReproducible)
{
    const InstructionCase& testCase = GetParam();
    const std::string source = std::string(testCase.source) + "end\r\n";

    const Snapshot first = CompileSnippet(source);
    const Snapshot second = CompileSnippet(source);

    EXPECT_EQ(first.bytes, second.bytes)
        << testCase.family << ": machine code is not reproducible";
}

INSTANTIATE_TEST_SUITE_P(
    CodegenInstructions,
    CodegenInstructionEmissionTest,
    ::testing::ValuesIn(kInstructionCases),
    [](const ::testing::TestParamInfo<InstructionCase>& info) {
        return std::string(info.param.family);
    });

} // namespace
