// test_codegen_udt_array_string_width.cpp — a UDT array-element copy must move
// its string members at pointer width.
//
// `dst(i) = src(i)` is emitted as CopyByteMemory(dst, src, sizeof(UDT)) followed
// by a compiler-generated deep-clone of every string member, so the two elements
// do not end up aliasing one heap buffer. Each clone loads the member, calls
// EquateSS to duplicate it, and stores the new pointer back.
//
// On x64 that member is an 8-byte pointer. CParseInstruction::ActOnSingleVar()
// (ParseInstruction.cpp:69/:81) passes the scalar type DBPType::String even when
// the container is an array element, so CTaskEmitter's switch(dwPType-100)
// underflows to `default:` and DetermineASMCallForREL() picks the 4-byte
// MOVR*RAXOFF4 ops. The generated `mov ecx,[rax+A]` then truncates the pointer
// to its low DWORD before EquateSS() dereferences it — which is how an
// uninitialized 0xFFFFFFFFFFFFFFFF member became strlen(0xFFFFFFFF) and an
// access violation in the FPSC map editor.

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "DBPLogger.h"
#include "codegen/CodegenHarness.h"

using namespace dbp::codegen;

namespace {

// Listing spellings of the four relevant opcodes (ASMWriter.cpp:302/:316/:617/:618).
constexpr std::string_view kLoadNarrow = "MOV ECX [RAX+A]";   // MOVRCXRAXOFF4
constexpr std::string_view kLoadWide   = "MOV RCX [RAX+A]8";  // MOVRCXRAXOFF8
constexpr std::string_view kStoreNarrow = "MOV [RAX+A] ECX";  // MOVRAXOFFRCX4
constexpr std::string_view kStoreWide   = "MOV [RAX+A]8 RCX"; // MOVRAXOFFRCX8

// Anchors: the block copy starts the UDT assignment, the string duplicate is the
// first EquateSS after it.
constexpr std::string_view kBlockCopy = "CopyByteMemory";
constexpr std::string_view kDuplicate = "EquateSS";

// Source shape lifted from the crashing FPSC statement
// `segmentprofile(segid,p)=temp(p)` (FullSourceDump.dba:10538).
constexpr std::string_view kUdtArrayCopySource = R"(
type Rec
  n as integer
  name$ as string
endtype

dim src(4) as Rec
dim dst(4) as Rec
src(0).name$ = "x"
dst(0) = src(0)
end
)";

std::vector<std::string> SplitLines(std::string_view text)
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::size_t stop = (end == std::string_view::npos) ? text.size() : end;
        std::string line(text.substr(start, stop - start));
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        lines.push_back(std::move(line));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return lines;
}

bool Contains(std::string_view haystack, std::string_view needle)
{
    return haystack.find(needle) != std::string_view::npos;
}

// Index of the first line in [from, lines.size()) containing `needle`.
std::size_t FindForward(const std::vector<std::string>& lines, std::size_t from,
                        std::string_view needle)
{
    for (std::size_t i = from; i < lines.size(); ++i) {
        if (Contains(lines[i], needle)) return i;
    }
    return lines.size();
}

// Index of the last line in [0, before) containing either needle.
std::size_t FindBackwardEither(const std::vector<std::string>& lines, std::size_t before,
                               std::string_view a, std::string_view b)
{
    for (std::size_t i = before; i-- > 0;) {
        if (Contains(lines[i], a) || Contains(lines[i], b)) return i;
    }
    return lines.size();
}

// Compiles the snippet once and locates the deep-clone of the first string
// member: the member load, the EquateSS call and the write-back.
class UdtArrayStringWidthTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        DBPLogger::Initialize("test_codegen_udt_array_string_width.log");
        EnsureEnvironment();
    }
    static void TearDownTestSuite() { spdlog::shutdown(); }

    void SetUp() override
    {
        snapshot = CompileSnippet(kUdtArrayCopySource);
        ASSERT_TRUE(snapshot.parsed) << "stage=" << snapshot.stage
                                     << " parser=" << snapshot.parserErrorMessage;
        ASSERT_TRUE(snapshot.emitted) << "stage=" << snapshot.stage
                                      << " error=" << snapshot.errorMessage;
        ASSERT_FALSE(snapshot.hasError) << snapshot.errorMessage;

        lines = SplitLines(snapshot.listing);

        const std::size_t blockCopy = FindForward(lines, 0, kBlockCopy);
        ASSERT_NE(blockCopy, lines.size())
            << "no CopyByteMemory call: the UDT array-element assignment was not "
               "emitted as a block copy, so this test no longer covers the "
               "deep-clone path it was written for";

        duplicate = FindForward(lines, blockCopy, kDuplicate);
        ASSERT_NE(duplicate, lines.size())
            << "no EquateSS call after CopyByteMemory: the generated deep-clone of "
               "the string member is missing";

        load = FindBackwardEither(lines, duplicate, kLoadWide, kLoadNarrow);
        store = FindForward(lines, duplicate + 1, kStoreWide);
        if (store == lines.size()) {
            store = FindForward(lines, duplicate + 1, kStoreNarrow);
        }
    }

    Snapshot snapshot;
    std::vector<std::string> lines;
    std::size_t duplicate = 0;
    std::size_t load = 0;
    std::size_t store = 0;
};

TEST_F(UdtArrayStringWidthTest, DeepCloneLoadsStringMemberAsFullPointer)
{
    ASSERT_NE(load, lines.size())
        << "no member load before the EquateSS call at listing line " << duplicate;
    EXPECT_TRUE(Contains(lines[load], kLoadWide))
        << "the string member of a UDT array element is an 8-byte pointer on x64; "
           "loading it with the 4-byte MOVRCXRAXOFF4 truncates it before EquateSS "
           "dereferences it.\n  listing line " << load << ": " << lines[load];
}

TEST_F(UdtArrayStringWidthTest, DeepCloneStoresDuplicatedStringMemberAsFullPointer)
{
    ASSERT_NE(store, lines.size())
        << "no member store after the EquateSS call at listing line " << duplicate;
    EXPECT_TRUE(Contains(lines[store], kStoreWide))
        << "writing the duplicated pointer back with the 4-byte MOVRAXOFFRCX4 "
           "leaves the upper half of the member slot stale.\n  listing line "
        << store << ": " << lines[store];
}

// Control: a user-written member assignment already goes through the
// `case 3:` arm of WriteASMARRtoRAX/WriteASMRAXtoARR and emits the 8-byte ops.
// Widening the deep-clone path must not disturb it.
TEST(UdtArrayStringWidthControl, DirectMemberAssignmentUsesPointerWidth)
{
    const Snapshot snapshot = CompileSnippet(R"(
type Rec
  n as integer
  name$ as string
endtype

dim src(4) as Rec
dim dst(4) as Rec
dst(0).name$ = src(0).name$
end
)");
    ASSERT_TRUE(snapshot.emitted) << "stage=" << snapshot.stage
                                  << " error=" << snapshot.errorMessage;
    ASSERT_FALSE(snapshot.hasError) << snapshot.errorMessage;

    const std::vector<std::string> lines = SplitLines(snapshot.listing);
    const std::size_t duplicate = FindForward(lines, 0, kDuplicate);
    ASSERT_NE(duplicate, lines.size()) << "no EquateSS call in the listing";

    const std::size_t load = FindBackwardEither(lines, duplicate, kLoadWide, kLoadNarrow);
    ASSERT_NE(load, lines.size()) << "no member load before the EquateSS call";
    EXPECT_TRUE(Contains(lines[load], kLoadWide))
        << "listing line " << load << ": " << lines[load];

    std::size_t store = FindForward(lines, duplicate + 1, kStoreWide);
    if (store == lines.size()) store = FindForward(lines, duplicate + 1, kStoreNarrow);
    ASSERT_NE(store, lines.size()) << "no member store after the EquateSS call";
    EXPECT_TRUE(Contains(lines[store], kStoreWide))
        << "listing line " << store << ": " << lines[store];
}

} // namespace
