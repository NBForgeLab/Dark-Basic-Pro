// Characterization tests for InstructionTable enum class values.
// Pins the exact numeric values of InternalInstruction and BuildTask enums.
#include <gtest/gtest.h>
#include <type_traits>
#include "InstructionTable.h"

// Verify enum properties
TEST(InstructionTableEnumTest, InternalInstructionIsEnumClassWithIntUnderlying) {
    EXPECT_TRUE(std::is_enum<InternalInstruction>::value);
    EXPECT_EQ(sizeof(InternalInstruction), sizeof(int));
}

TEST(InstructionTableEnumTest, BuildTaskIsEnumClassWithIntUnderlying) {
    EXPECT_TRUE(std::is_enum<BuildTask>::value);
    EXPECT_EQ(sizeof(BuildTask), sizeof(int));
}

// --- IT_INTERNAL_MAXCOUNT preserved as constexpr ---
TEST(InstructionTableEnumTest, InternalMaxCountValue) {
    EXPECT_EQ(IT_INTERNAL_MAXCOUNT, 1000);
}

// --- Internal control instructions ---
TEST(InstructionTableEnumTest, AllocValue)       { EXPECT_EQ(static_cast<int>(InternalInstruction::Alloc), 1); }
TEST(InstructionTableEnumTest, FreeValue)        { EXPECT_EQ(static_cast<int>(InternalInstruction::Free), 2); }
TEST(InstructionTableEnumTest, AssignLLValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::AssignLL), 3); }
TEST(InstructionTableEnumTest, AssignFFValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::AssignFF), 4); }
TEST(InstructionTableEnumTest, AssignSSValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::AssignSS), 5); }
TEST(InstructionTableEnumTest, AssignBBValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::AssignBB), 6); }
TEST(InstructionTableEnumTest, AssignYYValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::AssignYY), 7); }
TEST(InstructionTableEnumTest, AssignWWValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::AssignWW), 8); }
TEST(InstructionTableEnumTest, AssignDDValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::AssignDD), 9); }
TEST(InstructionTableEnumTest, AssignOOValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::AssignOO), 10); }
TEST(InstructionTableEnumTest, AssignRRValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::AssignRR), 11); }
TEST(InstructionTableEnumTest, AssignPPValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::AssignPP), 12); }
TEST(InstructionTableEnumTest, RelAssignLLValue) { EXPECT_EQ(static_cast<int>(InternalInstruction::RelAssignLL), 13); }
TEST(InstructionTableEnumTest, StrFreeValue)     { EXPECT_EQ(static_cast<int>(InternalInstruction::StrFree), 22); }
TEST(InstructionTableEnumTest, UserFunctionExitValue) { EXPECT_EQ(static_cast<int>(InternalInstruction::UserFunctionExit), 24); }
TEST(InstructionTableEnumTest, AssignUdtValue)   { EXPECT_EQ(static_cast<int>(InternalInstruction::AssignUdt), 25); }

// --- Integer math ---
TEST(InstructionTableEnumTest, PowerLLLValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::PowerLLL), 51); }
TEST(InstructionTableEnumTest, MulLLLValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::MulLLL), 52); }
TEST(InstructionTableEnumTest, DivLLLValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::DivLLL), 53); }
TEST(InstructionTableEnumTest, AddLLLValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::AddLLL), 54); }
TEST(InstructionTableEnumTest, SubLLLValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::SubLLL), 55); }
TEST(InstructionTableEnumTest, ModLLLValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::ModLLL), 56); }
TEST(InstructionTableEnumTest, EqualLLLValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::EqualLLL), 57); }
TEST(InstructionTableEnumTest, GreaterLLLValue)  { EXPECT_EQ(static_cast<int>(InternalInstruction::GreaterLLL), 58); }
TEST(InstructionTableEnumTest, LessLLLValue)     { EXPECT_EQ(static_cast<int>(InternalInstruction::LessLLL), 59); }
TEST(InstructionTableEnumTest, NotEqualLLLValue) { EXPECT_EQ(static_cast<int>(InternalInstruction::NotEqualLLL), 60); }
TEST(InstructionTableEnumTest, GreaterEqualLLLValue) { EXPECT_EQ(static_cast<int>(InternalInstruction::GreaterEqualLLL), 61); }
TEST(InstructionTableEnumTest, LessEqualLLLValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::LessEqualLLL), 62); }

// --- Float math ---
TEST(InstructionTableEnumTest, PowerFFFValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::PowerFFF), 71); }
TEST(InstructionTableEnumTest, EqualFFValue)     { EXPECT_EQ(static_cast<int>(InternalInstruction::EqualFF), 76); }
TEST(InstructionTableEnumTest, ModFFFValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::ModFFF), 82); }

// --- Double math ---
TEST(InstructionTableEnumTest, PowerOOOValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::PowerOOO), 91); }
TEST(InstructionTableEnumTest, LessEqualOOValue) { EXPECT_EQ(static_cast<int>(InternalInstruction::LessEqualOO), 101); }

// --- String math ---
TEST(InstructionTableEnumTest, PowerSSSValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::PowerSSS), 111); }
TEST(InstructionTableEnumTest, LessEqualSSValue) { EXPECT_EQ(static_cast<int>(InternalInstruction::LessEqualSS), 126); }

// --- Double integer math ---
TEST(InstructionTableEnumTest, PowerRRRValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::PowerRRR), 31); }
TEST(InstructionTableEnumTest, LessEqualRRValue) { EXPECT_EQ(static_cast<int>(InternalInstruction::LessEqualRR), 41); }

// --- Bitwise math ---
TEST(InstructionTableEnumTest, ShiftLLLLValue)   { EXPECT_EQ(static_cast<int>(InternalInstruction::ShiftLLLL), 141); }
TEST(InstructionTableEnumTest, BitNotLLLValue)   { EXPECT_EQ(static_cast<int>(InternalInstruction::BitNotLLL), 149); }

// --- Comparison math ---
TEST(InstructionTableEnumTest, OrLLLValue)       { EXPECT_EQ(static_cast<int>(InternalInstruction::OrLLL), 146); }
TEST(InstructionTableEnumTest, AndLLLValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::AndLLL), 147); }
TEST(InstructionTableEnumTest, NotLLLValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::NotLLL), 148); }

// --- DWORD pointer math ---
TEST(InstructionTableEnumTest, PowerDDDValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::PowerDDD), 151); }
TEST(InstructionTableEnumTest, EqualDDDValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::EqualDDD), 162); }

// --- Boolean math ---
TEST(InstructionTableEnumTest, PowerBBBValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::PowerBBB), 171); }
TEST(InstructionTableEnumTest, ModBBBValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::ModBBB), 176); }

// --- BYTE math ---
TEST(InstructionTableEnumTest, PowerYYYValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::PowerYYY), 181); }
TEST(InstructionTableEnumTest, ModYYYValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::ModYYY), 186); }

// --- WORD math ---
TEST(InstructionTableEnumTest, PowerWWWValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::PowerWWW), 191); }
TEST(InstructionTableEnumTest, ModWWWValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::ModWWW), 196); }

// --- Casting math ---
TEST(InstructionTableEnumTest, CastLToFValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::CastLToF), 201); }
TEST(InstructionTableEnumTest, CastRToOValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::CastRTOO), 277); }

// --- Internal commands ---
TEST(InstructionTableEnumTest, ReturnValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::Return), 301); }
TEST(InstructionTableEnumTest, EndValue)         { EXPECT_EQ(static_cast<int>(InternalInstruction::End), 302); }
TEST(InstructionTableEnumTest, SyncValue)        { EXPECT_EQ(static_cast<int>(InternalInstruction::Sync), 303); }
TEST(InstructionTableEnumTest, StartProgramValue){ EXPECT_EQ(static_cast<int>(InternalInstruction::StartProgram), 304); }
TEST(InstructionTableEnumTest, EndProgramValue)  { EXPECT_EQ(static_cast<int>(InternalInstruction::EndProgram), 305); }
TEST(InstructionTableEnumTest, IncVarValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::IncVar), 306); }
TEST(InstructionTableEnumTest, DecVarValue)      { EXPECT_EQ(static_cast<int>(InternalInstruction::DecVar), 307); }
TEST(InstructionTableEnumTest, PureReturnValue)  { EXPECT_EQ(static_cast<int>(InternalInstruction::PureReturn), 308); }
TEST(InstructionTableEnumTest, EndErrorValue)    { EXPECT_EQ(static_cast<int>(InternalInstruction::EndError), 309); }

// --- Build tasks (BUILD_*) ---
TEST(InstructionTableEnumTest, BuildRetValue)          { EXPECT_EQ(static_cast<int>(BuildTask::Ret), 1); }
TEST(InstructionTableEnumTest, BuildEndValue)          { EXPECT_EQ(static_cast<int>(BuildTask::End), 2); }
TEST(InstructionTableEnumTest, BuildSyncValue)         { EXPECT_EQ(static_cast<int>(BuildTask::Sync), 3); }
TEST(InstructionTableEnumTest, BuildStartProgramValue) { EXPECT_EQ(static_cast<int>(BuildTask::StartProgram), 4); }
TEST(InstructionTableEnumTest, BuildEndProgramValue)   { EXPECT_EQ(static_cast<int>(BuildTask::EndProgramAndQuit), 5); }
TEST(InstructionTableEnumTest, BuildUserFuncExitValue) { EXPECT_EQ(static_cast<int>(BuildTask::UserFunctionExit), 6); }
TEST(InstructionTableEnumTest, BuildPureRetValue)      { EXPECT_EQ(static_cast<int>(BuildTask::PureRet), 7); }
TEST(InstructionTableEnumTest, BuildCopyUdtValue)      { EXPECT_EQ(static_cast<int>(BuildTask::CopyUdt), 8); }
TEST(InstructionTableEnumTest, BuildEndErrorValue)     { EXPECT_EQ(static_cast<int>(BuildTask::EndError), 9); }

TEST(InstructionTableEnumTest, BuildPowerValue)   { EXPECT_EQ(static_cast<int>(BuildTask::Power), 101); }
TEST(InstructionTableEnumTest, BuildMulValue)     { EXPECT_EQ(static_cast<int>(BuildTask::Mul), 102); }
TEST(InstructionTableEnumTest, BuildDivValue)     { EXPECT_EQ(static_cast<int>(BuildTask::Div), 103); }
TEST(InstructionTableEnumTest, BuildAddValue)     { EXPECT_EQ(static_cast<int>(BuildTask::Add), 104); }
TEST(InstructionTableEnumTest, BuildSubValue)     { EXPECT_EQ(static_cast<int>(BuildTask::Sub), 105); }
TEST(InstructionTableEnumTest, BuildModValue)     { EXPECT_EQ(static_cast<int>(BuildTask::Mod), 106); }

TEST(InstructionTableEnumTest, BuildShrValue)     { EXPECT_EQ(static_cast<int>(BuildTask::Shr), 151); }
TEST(InstructionTableEnumTest, BuildShlValue)     { EXPECT_EQ(static_cast<int>(BuildTask::Shl), 152); }
TEST(InstructionTableEnumTest, BuildBitAndValue)  { EXPECT_EQ(static_cast<int>(BuildTask::BitAnd), 153); }
TEST(InstructionTableEnumTest, BuildBitOrValue)   { EXPECT_EQ(static_cast<int>(BuildTask::BitOr), 154); }
TEST(InstructionTableEnumTest, BuildBitXorValue)  { EXPECT_EQ(static_cast<int>(BuildTask::BitXor), 155); }
TEST(InstructionTableEnumTest, BuildBitNotValue)  { EXPECT_EQ(static_cast<int>(BuildTask::BitNot), 156); }

TEST(InstructionTableEnumTest, BuildAndValue)     { EXPECT_EQ(static_cast<int>(BuildTask::And), 161); }
TEST(InstructionTableEnumTest, BuildOrValue)      { EXPECT_EQ(static_cast<int>(BuildTask::Or), 162); }
TEST(InstructionTableEnumTest, BuildNotValue)     { EXPECT_EQ(static_cast<int>(BuildTask::Not), 163); }

TEST(InstructionTableEnumTest, BuildEqualValue)        { EXPECT_EQ(static_cast<int>(BuildTask::Equal), 201); }
TEST(InstructionTableEnumTest, BuildGreaterValue)      { EXPECT_EQ(static_cast<int>(BuildTask::Greater), 202); }
TEST(InstructionTableEnumTest, BuildLessValue)         { EXPECT_EQ(static_cast<int>(BuildTask::Less), 203); }
TEST(InstructionTableEnumTest, BuildNotEqualValue)     { EXPECT_EQ(static_cast<int>(BuildTask::NotEqual), 204); }
TEST(InstructionTableEnumTest, BuildGreaterEqualValue) { EXPECT_EQ(static_cast<int>(BuildTask::GreaterEqual), 205); }
TEST(InstructionTableEnumTest, BuildLessEqualValue)    { EXPECT_EQ(static_cast<int>(BuildTask::LessEqual), 206); }

TEST(InstructionTableEnumTest, BuildIncValue)     { EXPECT_EQ(static_cast<int>(BuildTask::Inc), 1001); }
TEST(InstructionTableEnumTest, BuildDecValue)     { EXPECT_EQ(static_cast<int>(BuildTask::Dec), 1002); }
TEST(InstructionTableEnumTest, BuildIncAddValue)  { EXPECT_EQ(static_cast<int>(BuildTask::IncAdd), 1003); }
TEST(InstructionTableEnumTest, BuildDecAddValue)  { EXPECT_EQ(static_cast<int>(BuildTask::DecAdd), 1004); }
