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
    EXPECT_FALSE(stmt.HasObject());
    EXPECT_EQ(stmt.GetObject<CParseInstruction>(), nullptr);
}

// --- Test: SetObject with CParseInstruction stores correct type ---
TEST(StatementObjectTest, SetInstructionObjectReportsCorrectType) {
    CStatement stmt;
    auto* p = new CParseInstruction();
    stmt.SetObject(p);
    EXPECT_TRUE(stmt.HasObject());
    EXPECT_EQ(stmt.GetObject<CParseInstruction>(), p);
}

// --- Test: SetObject with CParseLoop stores correct type ---
TEST(StatementObjectTest, SetLoopObjectReportsCorrectType) {
    CStatement stmt;
    auto* p = new CParseLoop();
    stmt.SetObject(p);
    EXPECT_TRUE(stmt.HasObject());
    EXPECT_EQ(stmt.GetObject<CParseLoop>(), p);
}

// --- Test: SetObject with CParseJump stores correct type ---
TEST(StatementObjectTest, SetJumpObjectReportsCorrectType) {
    CStatement stmt;
    auto* p = new CParseJump();
    stmt.SetObject(p);
    EXPECT_TRUE(stmt.HasObject());
    EXPECT_EQ(stmt.GetObject<CParseJump>(), p);
}

// --- Test: SetObject with CParseType stores correct type ---
TEST(StatementObjectTest, SetTypeObjectReportsCorrectType) {
    CStatement stmt;
    auto* p = new CParseType();
    stmt.SetObject(p);
    EXPECT_TRUE(stmt.HasObject());
    EXPECT_EQ(stmt.GetObject<CParseType>(), p);
}

// --- Test: SetObject with CParseInit stores correct type ---
TEST(StatementObjectTest, SetInitObjectReportsCorrectType) {
    CStatement stmt;
    auto* p = new CParseInit();
    stmt.SetObject(p);
    EXPECT_TRUE(stmt.HasObject());
    EXPECT_EQ(stmt.GetObject<CParseInit>(), p);
}

// --- Test: SetObject with CParseUserFunction stores correct type ---
TEST(StatementObjectTest, SetUserFunctionObjectReportsCorrectType) {
    CStatement stmt;
    auto* p = new CParseUserFunction();
    stmt.SetObject(p);
    EXPECT_TRUE(stmt.HasObject());
    EXPECT_EQ(stmt.GetObject<CParseUserFunction>(), p);
}

// --- Test: SetObject with CParseFunction stores correct type ---
TEST(StatementObjectTest, SetFunctionObjectReportsCorrectType) {
    CStatement stmt;
    auto* p = new CParseFunction();
    stmt.SetObject(p);
    EXPECT_TRUE(stmt.HasObject());
    EXPECT_EQ(stmt.GetObject<CParseFunction>(), p);
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
    auto* pJump = new CParseJump();
    stmt.SetObject(pJump); // previous CParseLoop freed
    EXPECT_EQ(stmt.GetObject<CParseJump>(), pJump);
    EXPECT_EQ(stmt.GetObject<CParseLoop>(), nullptr);
}

// --- Test: ClearObject resets to empty ---
TEST(StatementObjectTest, ClearObjectResetsToEmpty) {
    CStatement stmt;
    stmt.SetObject(new CParseInstruction());
    stmt.ClearObject();
    EXPECT_FALSE(stmt.HasObject());
    EXPECT_EQ(stmt.GetObject<CParseInstruction>(), nullptr);
}

// --- Test: SetData template sets line and object ---
TEST(StatementObjectTest, SetDataSetsLineAndObject) {
    CStatement stmt;
    auto* pLoop = new CParseLoop();
    stmt.SetData(42, std::unique_ptr<CParseLoop>(pLoop));
    EXPECT_EQ(stmt.GetLineNumber(), 42u);
    EXPECT_EQ(stmt.GetObject<CParseLoop>(), pLoop);
    EXPECT_TRUE(stmt.HasObject());
}
