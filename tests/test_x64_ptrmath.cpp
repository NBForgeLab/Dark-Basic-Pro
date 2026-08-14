// test_x64_ptrmath.cpp
//
// Wave 15 — address arithmetic (`x = &b + n`) must run at int64 width in the
// emitter instead of DWORD (4-byte) mode:
//   * type-107 (address-of) operands force int64 math mode (ADD/SUB RAX,RBX);
//   * the operand casts int->int64 and dword/address->int64 become internal
//     emitter tasks (MOVSXD RAX,EAX / zero-extension) — no ?CastLtoR@@... or
//     ?CastDtoR@@... dbprocore.dll calls, including the wave-8b literal gap
//     (`a(int64) = b + 5`);
//   * comparisons on addresses use the int64 CMP RDX,RBX + SETcc path.
//
// All assertions fail against the pre-wave emitter (4-byte ADD EAX,EBX and
// DLL cast calls).
//
// Design: docs/superpowers/specs/2026-08-11-x64-address-math-design.md

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
} // namespace

class X64PtrMathTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext = nullptr;
    CASMWriter*      m_pWriter = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_x64_ptrmath.log");

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
// Task-level: native int64 widening casts
// ---------------------------------------------------------------------------

TEST_F(X64PtrMathTest, TaskCastIntToInt64EmitsMovsxd)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr src(const_cast<char*>("@n")), res(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastIntToInt64),
        &src, nullptr, 1, 0, nullptr, nullptr, 0, 0, &res, nullptr, 9, 0));
    const auto bytes = BytesSince(before);
    // 48 63 C0 : MOVSXD RAX,EAX (sign-extend int -> int64)
    EXPECT_TRUE(Contains(bytes, {0x48, 0x63, 0xC0}))
        << "int->int64 cast must emit MOVSXD RAX,EAX";
}

TEST_F(X64PtrMathTest, TaskCastDwordToInt64With107SourceReads8Bytes)
{
    const auto before = m_pWriter->GetCurrentMCPosition();
    CStr src(const_cast<char*>("@&b")), off(const_cast<char*>("0")), res(const_cast<char*>("@r"));
    ASSERT_TRUE(m_pWriter->WriteASMTaskCore(
        1u, static_cast<DWORD>(ASMTask::CastDwordToInt64),
        &src, &off, 107, 0, nullptr, nullptr, 0, 0, &res, nullptr, 9, 0));
    const auto bytes = BytesSince(before);
    // The 107 source must be read at full QWORD width (wave 14) and stored
    // at full QWORD width to the int64 result.
    EXPECT_TRUE(Contains(bytes, {0x48, 0x8B, 0x88}))
        << "address->int64 cast must read the 107 source as 8 bytes";
}

// ---------------------------------------------------------------------------
// Compiled end-to-end
// ---------------------------------------------------------------------------

TEST_F(X64PtrMathTest, CompiledPtrPlusLiteralEmitsRexWAddNoDll)
{
    char prog[] = "dim b as integer\r\ndim a as double integer\r\na=&b+4\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();

    EXPECT_TRUE(Contains(stream, {0x48, 0x01, 0xD8})) << "ADD RAX,RBX missing";
    EXPECT_TRUE(Contains(stream, {0x48, 0x8B, 0x88}))
        << "&b must be read as a full QWORD";
    EXPECT_FALSE(Contains(stream, {0x01, 0xD8, 0xA3}))
        << "4-byte ADD EAX,EBX followed by 4-byte store must be gone";
    EXPECT_FALSE(AnyReferenceContains("CastLtoR"));
    EXPECT_FALSE(AnyReferenceContains("CastDtoR"));
}

TEST_F(X64PtrMathTest, CompiledPtrMinusLiteralEmitsRexWSubNoDll)
{
    char prog[] = "dim b as integer\r\ndim a as double integer\r\na=&b-4\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x48, 0x29, 0xD8})) << "SUB RAX,RBX missing";
    EXPECT_FALSE(AnyReferenceContains("CastLtoR"));
    EXPECT_FALSE(AnyReferenceContains("CastDtoR"));
}

TEST_F(X64PtrMathTest, CompiledPtrPlusIntVarEmitsRexWAddNoDll)
{
    char prog[] = "dim b as integer\r\ndim n as integer\r\ndim a as double integer\r\na=&b+n\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x48, 0x01, 0xD8})) << "ADD RAX,RBX missing";
    EXPECT_FALSE(AnyReferenceContains("CastLtoR"));
    EXPECT_FALSE(AnyReferenceContains("CastDtoR"));
}

TEST_F(X64PtrMathTest, CompiledPtrPlusDwordVarEmitsRexWAddNoDll)
{
    char prog[] = "dim b as integer\r\ndim n as dword\r\ndim a as double integer\r\na=&b+n\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x48, 0x01, 0xD8})) << "ADD RAX,RBX missing";
    EXPECT_FALSE(AnyReferenceContains("CastDtoR"));
    EXPECT_FALSE(AnyReferenceContains("CastLtoR"));
}

TEST_F(X64PtrMathTest, CompiledInt64LiteralAddEmitsRexWAddNoDll)
{
    // Wave-8b gap: int64 + int literal used to route the literal through a
    // ?CastLtoR@@... DLL call. Now native MOVSXD + ADD RAX,RBX.
    char prog[] = "dim b as double integer\r\ndim a as double integer\r\na=b+5\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x48, 0x01, 0xD8})) << "ADD RAX,RBX missing";
    EXPECT_TRUE(Contains(stream, {0x48, 0x63, 0xC0})) << "MOVSXD RAX,EAX missing";
    EXPECT_FALSE(AnyReferenceContains("CastLtoR"));
}

TEST_F(X64PtrMathTest, CompiledPtrCompareEmitsRexWCmpNoDll)
{
    char prog[] = "dim b as integer\r\ndim r as integer\r\nr=&b>5\r\nend\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    const auto stream = Compile();
    EXPECT_TRUE(Contains(stream, {0x48, 0x3B, 0xDA})) << "CMP RDX,RBX missing";
    EXPECT_FALSE(AnyReferenceContains("CastLtoR"));
    EXPECT_FALSE(AnyReferenceContains("CastDtoR"));
}
