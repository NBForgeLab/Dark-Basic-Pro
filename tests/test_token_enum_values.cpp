// Characterization tests for Token enum class values.
// These tests pin the exact numeric values of every token constant
// to ensure the #define -> enum class conversion preserves values.
#include <gtest/gtest.h>
#include <type_traits>
#include "Statement.h"

// Verify Token is an enum class with underlying type int
TEST(TokenEnumTest, IsEnumClassWithIntUnderlying) {
    EXPECT_TRUE(std::is_enum<Token>::value);
    EXPECT_EQ(sizeof(Token), sizeof(int));
}

// --- General tokens ---
TEST(TokenEnumTest, CommaValue)    { EXPECT_EQ(static_cast<int>(Token::Comma), 10001); }
TEST(TokenEnumTest, CrtValue)      { EXPECT_EQ(static_cast<int>(Token::Crt), 10002); }

// --- End / Exit ---
TEST(TokenEnumTest, EndValue)      { EXPECT_EQ(static_cast<int>(Token::End), 10010); }
TEST(TokenEnumTest, ExitValue)     { EXPECT_EQ(static_cast<int>(Token::Exit), 10011); }

// --- Loop tokens ---
TEST(TokenEnumTest, DoValue)       { EXPECT_EQ(static_cast<int>(Token::Do), 10021); }
TEST(TokenEnumTest, LoopValue)     { EXPECT_EQ(static_cast<int>(Token::Loop), 10022); }
TEST(TokenEnumTest, WhileValue)    { EXPECT_EQ(static_cast<int>(Token::While), 10023); }
TEST(TokenEnumTest, EndWhileValue) { EXPECT_EQ(static_cast<int>(Token::EndWhile), 10024); }
TEST(TokenEnumTest, RepeatValue)   { EXPECT_EQ(static_cast<int>(Token::Repeat), 10025); }
TEST(TokenEnumTest, UntilValue)    { EXPECT_EQ(static_cast<int>(Token::Until), 10026); }
TEST(TokenEnumTest, ForValue)      { EXPECT_EQ(static_cast<int>(Token::For), 10031); }
TEST(TokenEnumTest, NextValue)     { EXPECT_EQ(static_cast<int>(Token::Next), 10032); }

// --- User function tokens ---
TEST(TokenEnumTest, UserFunctionValue)      { EXPECT_EQ(static_cast<int>(Token::UserFunction), 10101); }
TEST(TokenEnumTest, ExitUserFunctionValue)  { EXPECT_EQ(static_cast<int>(Token::ExitUserFunction), 10102); }
TEST(TokenEnumTest, EndUserFunctionValue)   { EXPECT_EQ(static_cast<int>(Token::EndUserFunction), 10103); }
TEST(TokenEnumTest, UserFunctionCallValue)  { EXPECT_EQ(static_cast<int>(Token::UserFunctionCall), 10104); }

// --- If/Else tokens ---
TEST(TokenEnumTest, IfValue)          { EXPECT_EQ(static_cast<int>(Token::If), 10151); }
TEST(TokenEnumTest, ElseValue)        { EXPECT_EQ(static_cast<int>(Token::Else), 10152); }
TEST(TokenEnumTest, EndIfValue)       { EXPECT_EQ(static_cast<int>(Token::EndIf), 10153); }
TEST(TokenEnumTest, ElseEndIfValue)   { EXPECT_EQ(static_cast<int>(Token::ElseEndIf), 10154); }
TEST(TokenEnumTest, ElseCrtValue)     { EXPECT_EQ(static_cast<int>(Token::ElseCrt), 10155); }

// --- Jump tokens ---
TEST(TokenEnumTest, GotoValue)     { EXPECT_EQ(static_cast<int>(Token::Goto), 10201); }
TEST(TokenEnumTest, GosubValue)    { EXPECT_EQ(static_cast<int>(Token::Gosub), 10202); }

// --- Select/Case tokens ---
TEST(TokenEnumTest, SelectValue)      { EXPECT_EQ(static_cast<int>(Token::Select), 10211); }
TEST(TokenEnumTest, EndSelectValue)   { EXPECT_EQ(static_cast<int>(Token::EndSelect), 10212); }
TEST(TokenEnumTest, CaseValue)        { EXPECT_EQ(static_cast<int>(Token::Case), 10213); }
TEST(TokenEnumTest, EndCaseValue)     { EXPECT_EQ(static_cast<int>(Token::EndCase), 10214); }
TEST(TokenEnumTest, CaseDefaultValue) { EXPECT_EQ(static_cast<int>(Token::CaseDefault), 10215); }

// --- Type/Declaration tokens ---
TEST(TokenEnumTest, TypeValue)      { EXPECT_EQ(static_cast<int>(Token::Type), 10301); }
TEST(TokenEnumTest, EndTypeValue)   { EXPECT_EQ(static_cast<int>(Token::EndType), 10302); }
TEST(TokenEnumTest, GlobalValue)    { EXPECT_EQ(static_cast<int>(Token::Global), 10303); }
TEST(TokenEnumTest, LocalValue)     { EXPECT_EQ(static_cast<int>(Token::Local), 10304); }
TEST(TokenEnumTest, DimValue)       { EXPECT_EQ(static_cast<int>(Token::Dim), 10305); }
TEST(TokenEnumTest, UndimValue)     { EXPECT_EQ(static_cast<int>(Token::Undim), 10306); }
TEST(TokenEnumTest, AsteriskValue)  { EXPECT_EQ(static_cast<int>(Token::Asterisk), 10307); }

// --- Data type tokens ---
TEST(TokenEnumTest, BooleanValue)   { EXPECT_EQ(static_cast<int>(Token::Boolean), 10311); }
TEST(TokenEnumTest, ByteValue)      { EXPECT_EQ(static_cast<int>(Token::Byte), 10312); }
TEST(TokenEnumTest, WordValue)      { EXPECT_EQ(static_cast<int>(Token::Word), 10313); }
TEST(TokenEnumTest, DwordValue)     { EXPECT_EQ(static_cast<int>(Token::Dword), 10314); }
TEST(TokenEnumTest, IntegerValue)   { EXPECT_EQ(static_cast<int>(Token::Integer), 10315); }
TEST(TokenEnumTest, FloatValue)     { EXPECT_EQ(static_cast<int>(Token::Float), 10316); }
TEST(TokenEnumTest, StringValue)    { EXPECT_EQ(static_cast<int>(Token::String), 10317); }
TEST(TokenEnumTest, DoubleValue)    { EXPECT_EQ(static_cast<int>(Token::Double), 10318); }

// --- Remark tokens ---
TEST(TokenEnumTest, RemLineValue)   { EXPECT_EQ(static_cast<int>(Token::RemLine), 10501); }
TEST(TokenEnumTest, RemStartValue)  { EXPECT_EQ(static_cast<int>(Token::RemStart), 10502); }
TEST(TokenEnumTest, RemEndValue)    { EXPECT_EQ(static_cast<int>(Token::RemEnd), 10503); }

// --- Label/Data tokens ---
TEST(TokenEnumTest, LabelValue)     { EXPECT_EQ(static_cast<int>(Token::Label), 10701); }
TEST(TokenEnumTest, DataValue)      { EXPECT_EQ(static_cast<int>(Token::Data), 10702); }

// --- Statement type tokens ---
TEST(TokenEnumTest, AssignmentValue)  { EXPECT_EQ(static_cast<int>(Token::Assignment), 11001); }
TEST(TokenEnumTest, InstructionValue) { EXPECT_EQ(static_cast<int>(Token::Instruction), 11004); }
