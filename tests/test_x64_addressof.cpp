// test_x64_addressof.cpp
//
// Wave 14 — the `&var` (address-of) operator must produce a full 8-byte
// address on x64 instead of a truncated 4-byte DWORD.
//
//   * `&b` results get the distinct "DWORD POINTER" type 107 (address value,
//     not an array element) so the emitter can widen the companion read;
//   * the companion value read/store moves a full QWORD (MOV RCX,[RAX+0] /
//     MOV RAX,RCX — REX.W), never MOV ECX,[RAX] / MOV EAX,ECX;
//   * `a = &b` for an int64 target stores MOV [@a],RAX directly — no
//     ?CastLtoR@@... dbprocore.dll call (the old path widened an already
//     truncated 4-byte value through a DLL).
//
// All assertions below fail against the pre-wave emitter.
//
// Design: docs/superpowers/specs/2026-08-11-x64-addressof-design.md

#include "gtest/gtest.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <windows.h>

#include "CompilerContext.h"
#include "ASMWriter.h"
#include "DBPCompiler.h"
#include "DBPLogger.h"
#include "EXEBlock.h"
#include "InstructionTable.h"
#include "ReferenceTracker.h"
#include "Statement.h"
#include "StatementList.h"
#include "Str.h"
#include "StructTable.h"

extern CStructTable* g_pStructTable;
extern CInstructionTable* g_pInstructionTable;
extern CStatementList* g_pStatementList;
extern ICodeGenerator* g_pASMWriter;
extern CDBPCompiler* g_pDBPCompiler;

namespace
{
std::vector<uint8_t> AsBytes(const char* raw, std::size_t length)
{
    const auto* bytes = reinterpret_cast<const uint8_t*>(raw);
    return std::vector<uint8_t>(bytes, bytes + length);
}

bool Contains(const std::vector<uint8_t>& bytes,
              const std::vector<uint8_t>& needle)
{
    return std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end())
           != bytes.end();
}

// A 4-byte truncating read is MOV ECX,[RAX+disp] = 8B 88 — the REX.W
// variant (48 8B 88) must not count. Leap-marker displacement placeholders
// (0xFFFFFFFF) are unresolved at task level, so only the opcode prefix is
// compared.
bool ContainsTruncatingRead(const std::vector<uint8_t>& bytes)
{
    for (std::size_t i = 0; i + 1 < bytes.size(); ++i)
    {
        if (bytes[i] == 0x8B && bytes[i + 1] == 0x88 &&
            (i == 0 || bytes[i - 1] != 0x48))
            return true;
    }
    return false;
}
} // namespace

class X64AddressofTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_addressof.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();
        g_pStructTable->SetStructDefaults();
        ASSERT_TRUE(g_pInstructionTable->SetInternalInstructionDatabase());
        m_pWriter = static_cast<CASMWriter*>(g_pASMWriter);
        ASSERT_TRUE(g_pASMWriter->CreateASMHeader());
    }

    void TearDown() override {
        if (m_pContext) {
            m_pContext->Cleanup();
            delete m_pContext;
            m_pContext = nullptr;
        }
        spdlog::shutdown();
    }

    std::vector<uint8_t> BytesSince(std::size_t before) const {
        const auto after = m_pWriter->GetCurrentMCPosition();
        const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
        return AsBytes(raw + before, after - before);
    }

    void PatchAll(std::vector<uint8_t>& code) const {
        std::vector<uintptr_t> values;
        std::vector<DWORD> positions;
        std::vector<DWORD> types;
        for (const auto& record : m_pWriter->GetReferenceTracker().GetRecords())
        {
            const auto parsed = ParseReferenceLabel(record.label);
            ASSERT_TRUE(parsed.has_value()) << "unparseable reference: " << record.label;
            values.push_back(parsed->index);
            positions.push_back(record.machineCodeOffset);
            types.push_back(static_cast<DWORD>(parsed->kind));
        }
        CEXEBlock::PatchReferenceValues(
            values.data(), values.size(),
            positions.data(), types.data(),
            reinterpret_cast<char*>(code.data()));
    }

    std::vector<uint8_t> Compile() {
        char compilerPath[] = "DBPCompiler.exe";
        CDBPCompiler compiler(compilerPath);
        CDBPCompiler* const previousCompiler = g_pDBPCompiler;
        g_pDBPCompiler = &compiler;
        g_pStatementList->SetWriteStarted(true);
        EXPECT_TRUE(g_pStatementList->GetProgramStatements()->WriteDBM());
        EXPECT_TRUE(g_pStatementList->GetPreScanStatements()->WriteDBM());
        g_pDBPCompiler = previousCompiler;

        const auto* raw = m_pWriter->GetMachineCodeBuffer().GetProgramStart();
        std::vector<uint8_t> stream(raw, raw + m_pWriter->GetCurrentMCPosition());
        PatchAll(stream);
        return stream;
    }

    bool AnyReferenceContains(const char* needle) const {
        const auto& records = m_pWriter->GetReferenceTracker().GetRecords();
        return std::any_of(records.begin(), records.end(),
            [needle](const CReferenceTracker::Record& r) {
                return strstr(r.label.c_str(), needle) != nullptr;
            });
    }
};

// ---------------------------------------------------------------------------
// Task-level: a type-107 MemArr operand must read the companion value as 8 bytes
// ---------------------------------------------------------------------------

TEST_F(X64AddressofTest, TaskMemArrType107Reads8Bytes)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr a(const_cast<char*>("@a")), p(const_cast<char*>("@&b")), off(const_cast<char*>("0"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::Assign),
        &a, nullptr, 9, 0, &p, &off, 107, 0, nullptr, nullptr, 0, 0));
    const auto bytes = BytesSince(before);
    // 48 8B 88 <disp32>  : MOV RCX,[RAX+disp32]  (REX.W 8-byte read)
    // 48 8B C1            : MOV RAX,RCX
    EXPECT_TRUE(Contains(bytes, {0x48, 0x8B, 0x88}))
        << "type-107 companion read must move a full QWORD (REX.W)";
    EXPECT_TRUE(Contains(bytes, {0x48, 0x8B, 0xC1}))
        << "type-107 companion read must move RAX,RCX at full width";
    EXPECT_FALSE(ContainsTruncatingRead(bytes))
        << "type-107 companion read must not truncate to 4 bytes";
}

// ---------------------------------------------------------------------------
// Compiled end-to-end
// ---------------------------------------------------------------------------

TEST_F(X64AddressofTest, CompiledIntAddressofReads8Bytes)
{
    char prog[] = "dim b as integer\r\ndim a as integer\r\na=&b\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();

    // 8-byte read of the companion element + 8-byte reg move.
    EXPECT_TRUE(Contains(stream, {0x48, 0x8B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xC1}))
        << "a=&b must read the address as a full QWORD";
    // The legacy 4-byte truncation (MOV ECX,[RAX]; MOV EAX,ECX) must be gone.
    EXPECT_FALSE(Contains(stream, {0x8B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x8B, 0xC1}))
        << "a=&b must not truncate the address to 4 bytes";
    // No spurious cast DLL for an integer target.
    EXPECT_FALSE(AnyReferenceContains("CastDtoL"));
    EXPECT_FALSE(AnyReferenceContains("CastLtoD"));
}

TEST_F(X64AddressofTest, CompiledInt64AddressofStoresFullAddressNoDll)
{
    char prog[] = "dim b as integer\r\ndim a as double integer\r\na=&b\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();

    // Full 8-byte read...
    EXPECT_TRUE(Contains(stream, {0x48, 0x8B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xC1}))
        << "a(int64)=&b must read the address as a full QWORD";
    // ...stored as a single 8-byte MOV [@a],RAX (48 A3 moffs64).
    EXPECT_TRUE(Contains(stream, {0x48, 0xA3}))
        << "a(int64)=&b must store the full 8-byte address";
    // The old path widened an already-truncated value via ?CastLtoR@@... —
    // with a full-width read no cast DLL may appear.
    EXPECT_FALSE(AnyReferenceContains("CastLtoR"));
}

TEST_F(X64AddressofTest, CompiledDwordAddressofReads8Bytes)
{
    char prog[] = "dim b as integer\r\ndim a as dword\r\na=&b\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x48, 0x8B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xC1}))
        << "a(dword)=&b must read the address as a full QWORD";
    EXPECT_FALSE(Contains(stream, {0x8B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x8B, 0xC1}))
        << "a(dword)=&b must not truncate the address to 4 bytes";
    EXPECT_FALSE(AnyReferenceContains("CastDtoL"));
}

TEST_F(X64AddressofTest, CompiledByteAddressofReads8Bytes)
{
    char prog[] = "dim b as integer\r\ndim a as byte\r\na=&b\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x48, 0x8B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xC1}))
        << "a(byte)=&b must read the address as a full QWORD";
    EXPECT_FALSE(Contains(stream, {0x8B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x8B, 0xC1}))
        << "a(byte)=&b must not truncate the address to 4 bytes";
}

TEST_F(X64AddressofTest, CompiledRegularArrayElementStillReads4Bytes)
{
    // b(0) is a regular int array element — its value must stay 4 bytes wide.
    char prog[] = "dim b(5) as integer\r\ndim a as integer\r\na=b(0)\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x8B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x8B, 0xC1}))
        << "b(0) int element must still read 4 bytes";
    EXPECT_FALSE(Contains(stream, {0x48, 0x8B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xC1}))
        << "b(0) int element must not be widened to 8 bytes";
}
