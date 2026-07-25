// test_statement_object.cpp — TDD tests for type-safe StatementObject variant
//
// These tests define the contract for the new std::variant-based object storage
// in CStatement, replacing the unsafe void* m_pObjectClass pattern.

#include <gtest/gtest.h>
#include <memory>
#include <variant>

#include "Statement.h"
#include "ParseLoop.h"
#include "ParseJump.h"
#include "ParseType.h"
#include "ParseInit.h"
#include "ParseUserFunction.h"
#include "ParseInstruction.h"
#include "ParseFunction.h"

// --- Test: Default-constructed CStatement holds no object ---
TEST(StatementObjectTest, DefaultConstructedHoldsNoObject) {
    CStatement stmt;
    EXPECT_EQ(stmt.GetObjectType(), 0u);
    EXPECT_FALSE(stmt.HasObject());
}

// --- Test: SetObject with CParseInstruction sets correct type ---
TEST(StatementObjectTest, SetInstructionObjectReportsCorrectType) {
    CStatement stmt;
    stmt.SetObject(new CParseInstruction());
    EXPECT_EQ(stmt.GetObjectType(), 11u);
    EXPECT_TRUE(stmt.HasObject());
}

// --- Test: SetObject with CParseLoop sets correct type ---
TEST(StatementObjectTest, SetLoopObjectReportsCorrectType) {
    CStatement stmt;
    stmt.SetObject(new CParseLoop());
    EXPECT_EQ(stmt.GetObjectType(), 1u);
    EXPECT_TRUE(stmt.HasObject());
}

// --- Test: SetObject with CParseJump sets correct type ---
TEST(StatementObjectTest, SetJumpObjectReportsCorrectType) {
    CStatement stmt;
    stmt.SetObject(new CParseJump());
    EXPECT_EQ(stmt.GetObjectType(), 8u);
    EXPECT_TRUE(stmt.HasObject());
}

// --- Test: SetObject with CParseType sets correct type ---
TEST(StatementObjectTest, SetTypeObjectReportsCorrectType) {
    CStatement stmt;
    stmt.SetObject(new CParseType());
    EXPECT_EQ(stmt.GetObjectType(), 2u);
    EXPECT_TRUE(stmt.HasObject());
}

// --- Test: SetObject with CParseInit sets correct type ---
TEST(StatementObjectTest, SetInitObjectReportsCorrectType) {
    CStatement stmt;
    stmt.SetObject(new CParseInit());
    EXPECT_EQ(stmt.GetObjectType(), 3u);
    EXPECT_TRUE(stmt.HasObject());
}

// --- Test: SetObject with CParseUserFunction sets correct type ---
TEST(StatementObjectTest, SetUserFunctionObjectReportsCorrectType) {
    CStatement stmt;
    stmt.SetObject(new CParseUserFunction());
    EXPECT_EQ(stmt.GetObjectType(), 6u);
    EXPECT_TRUE(stmt.HasObject());
}

// --- Test: SetObject with CParseFunction sets correct type ---
TEST(StatementObjectTest, SetFunctionObjectReportsCorrectType) {
    CStatement stmt;
    stmt.SetObject(new CParseFunction());
    EXPECT_EQ(stmt.GetObjectType(), 12u);
    EXPECT_TRUE(stmt.HasObject());
}

// --- Test: GetObject returns correct typed pointer ---
TEST(StatementObjectTest, GetObjectReturnsTypedPointer) {
    CStatement stmt;
    CParseInstruction* rawPtr = new CParseInstruction();
    stmt.SetObject(rawPtr);
    EXPECT_EQ(stmt.GetObject<CParseInstruction>(), rawPtr);
}

// --- Test: GetObject with wrong type returns nullptr ---
TEST(StatementObjectTest, GetObjectWithWrongTypeReturnsNull) {
    CStatement stmt;
    stmt.SetObject(new CParseInstruction());
    EXPECT_EQ(stmt.GetObject<CParseLoop>(), nullptr);
}

// --- Test: Destruction automatically frees the object ---
TEST(StatementObjectTest, DestructionFreesObject) {
    // If this doesn't crash or leak (ASAN), the variant is cleaning up correctly
    auto stmt = std::make_unique<CStatement>();
    stmt->SetObject(new CParseInstruction());
    stmt.reset(); // should not leak or crash
}

// --- Test: Replacing object frees the previous one ---
TEST(StatementObjectTest, ReplacingObjectFreesPrevious) {
    CStatement stmt;
    stmt.SetObject(new CParseLoop());
    stmt.SetObject(new CParseJump()); // previous CParseLoop freed
    EXPECT_EQ(stmt.GetObjectType(), 8u);
}

// --- Test: ClearObject resets to empty ---
TEST(StatementObjectTest, ClearObjectResetsToEmpty) {
    CStatement stmt;
    stmt.SetObject(new CParseInstruction());
    stmt.ClearObject();
    EXPECT_EQ(stmt.GetObjectType(), 0u);
    EXPECT_FALSE(stmt.HasObject());
}

// --- Test: Legacy SetData overload works for backward compatibility ---
TEST(StatementObjectTest, LegacySetDataSetsLineAndObject) {
    CStatement stmt;
    auto pLoop = new CParseLoop();
    stmt.SetData(42, std::unique_ptr<CParseLoop>(pLoop));
    EXPECT_EQ(stmt.GetLineNumber(), 42u);
    EXPECT_EQ(stmt.GetObjectType(), 1u);
    EXPECT_EQ(stmt.GetObject<CParseLoop>(), pLoop);
}
