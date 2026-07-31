// Characterization tests for ASMWriter enum class values.
// Pins the exact numeric values of ASMTask, ASMOp, and ParamMode enums.
#include <gtest/gtest.h>
#include <type_traits>
#include "ASMWriter.h"

// Verify enum properties
TEST(ASMWriterEnumTest, ASMTaskIsEnumClassWithIntUnderlying) {
    EXPECT_TRUE(std::is_enum<ASMTask>::value);
    EXPECT_EQ(sizeof(ASMTask), sizeof(int));
}

TEST(ASMWriterEnumTest, ASMOpIsEnumClassWithIntUnderlying) {
    EXPECT_TRUE(std::is_enum<ASMOp>::value);
    EXPECT_EQ(sizeof(ASMOp), sizeof(int));
}

TEST(ASMWriterEnumTest, ParamModeIsEnumClassWithIntUnderlying) {
    EXPECT_TRUE(std::is_enum<ParamMode>::value);
    EXPECT_EQ(sizeof(ParamMode), sizeof(int));
}

TEST(ASMWriterEnumTest, AsmMaxCountValue) {
    EXPECT_EQ(ASMMAXCOUNT, 300);
}

// --- ASMTASK_* key values ---
TEST(ASMWriterEnumTest, TaskAssignValue)      { EXPECT_EQ(static_cast<int>(ASMTask::Assign), 1); }
TEST(ASMWriterEnumTest, TaskTestValue)        { EXPECT_EQ(static_cast<int>(ASMTask::Test), 4); }
TEST(ASMWriterEnumTest, TaskCallValue)        { EXPECT_EQ(static_cast<int>(ASMTask::Call), 5); }
TEST(ASMWriterEnumTest, TaskPushValue)        { EXPECT_EQ(static_cast<int>(ASMTask::Push), 6); }
TEST(ASMWriterEnumTest, TaskPopEaxValue)      { EXPECT_EQ(static_cast<int>(ASMTask::PopEax), 7); }
TEST(ASMWriterEnumTest, TaskPopEbxValue)      { EXPECT_EQ(static_cast<int>(ASMTask::PopEbx), 8); }
TEST(ASMWriterEnumTest, TaskConditionValue)   { EXPECT_EQ(static_cast<int>(ASMTask::Condition), 10); }
TEST(ASMWriterEnumTest, TaskJumpValue)        { EXPECT_EQ(static_cast<int>(ASMTask::Jump), 15); }
TEST(ASMWriterEnumTest, TaskReturnValue)      { EXPECT_EQ(static_cast<int>(ASMTask::Return), 17); }
TEST(ASMWriterEnumTest, TaskPowerValue)       { EXPECT_EQ(static_cast<int>(ASMTask::Power), 101); }
TEST(ASMWriterEnumTest, TaskMulValue)         { EXPECT_EQ(static_cast<int>(ASMTask::Mul), 102); }
TEST(ASMWriterEnumTest, TaskEqualValue)       { EXPECT_EQ(static_cast<int>(ASMTask::Equal), 111); }
TEST(ASMWriterEnumTest, TaskGreaterValue)     { EXPECT_EQ(static_cast<int>(ASMTask::Greater), 112); }
TEST(ASMWriterEnumTest, TaskShlValue)         { EXPECT_EQ(static_cast<int>(ASMTask::Shl), 131); }
TEST(ASMWriterEnumTest, TaskAndValue)         { EXPECT_EQ(static_cast<int>(ASMTask::And), 133); }
TEST(ASMWriterEnumTest, TaskOrValue)          { EXPECT_EQ(static_cast<int>(ASMTask::Or), 134); }
TEST(ASMWriterEnumTest, TaskNotValue)         { EXPECT_EQ(static_cast<int>(ASMTask::Not), 135); }
TEST(ASMWriterEnumTest, TaskIncVarValue)      { EXPECT_EQ(static_cast<int>(ASMTask::IncVar), 1001); }
TEST(ASMWriterEnumTest, TaskDecVarValue)      { EXPECT_EQ(static_cast<int>(ASMTask::DecVar), 1002); }
TEST(ASMWriterEnumTest, TaskCalcArrayValue)   { EXPECT_EQ(static_cast<int>(ASMTask::CalcArrayOffset), 502); }

// --- ASM_* key values ---
TEST(ASMWriterEnumTest, OpMovEaxMem1Value)    { EXPECT_EQ(static_cast<int>(ASMOp::MOVEAXMEM1), 2); }
TEST(ASMWriterEnumTest, OpMovMemEax1Value)    { EXPECT_EQ(static_cast<int>(ASMOp::MOVMEMEAX1), 5); }
TEST(ASMWriterEnumTest, OpPushEaxValue)       { EXPECT_EQ(static_cast<int>(ASMOp::PUSHEAX), 51); }
TEST(ASMWriterEnumTest, OpPopEaxValue)        { EXPECT_EQ(static_cast<int>(ASMOp::POPEAX), 57); }
TEST(ASMWriterEnumTest, OpRetValue)           { EXPECT_EQ(static_cast<int>(ASMOp::RET), 61); }
TEST(ASMWriterEnumTest, OpAddEspValue)        { EXPECT_EQ(static_cast<int>(ASMOp::ADDESP), 62); }
TEST(ASMWriterEnumTest, OpSubEspValue)        { EXPECT_EQ(static_cast<int>(ASMOp::SUBESP), 63); }
TEST(ASMWriterEnumTest, OpUnknownValue)       { EXPECT_EQ(static_cast<int>(ASMOp::UNKNOWN), 71); }
TEST(ASMWriterEnumTest, OpJmpValue)           { EXPECT_EQ(static_cast<int>(ASMOp::JMP), 81); }
TEST(ASMWriterEnumTest, OpJneValue)           { EXPECT_EQ(static_cast<int>(ASMOp::JNE), 82); }
TEST(ASMWriterEnumTest, OpJeValue)            { EXPECT_EQ(static_cast<int>(ASMOp::JE), 83); }
TEST(ASMWriterEnumTest, OpPushadValue)        { EXPECT_EQ(static_cast<int>(ASMOp::PUSHAD), 110); }
TEST(ASMWriterEnumTest, OpPopadValue)         { EXPECT_EQ(static_cast<int>(ASMOp::POPAD), 111); }
TEST(ASMWriterEnumTest, OpLoopValue)          { EXPECT_EQ(static_cast<int>(ASMOp::LOOP), 112); }
TEST(ASMWriterEnumTest, OpCdqValue)           { EXPECT_EQ(static_cast<int>(ASMOp::CDQ), 168); }
TEST(ASMWriterEnumTest, OpSetEValue)          { EXPECT_EQ(static_cast<int>(ASMOp::SETE), 172); }
TEST(ASMWriterEnumTest, OpMovEaxEbp4Value)    { EXPECT_EQ(static_cast<int>(ASMOp::MOVEAXEBP4), 89); }

// --- PMODE_* values ---
TEST(ASMWriterEnumTest, PModeNoneValue)       { EXPECT_EQ(static_cast<int>(ParamMode::None), 0); }
TEST(ASMWriterEnumTest, PModeImmValue)        { EXPECT_EQ(static_cast<int>(ParamMode::Imm), 1); }
TEST(ASMWriterEnumTest, PModeMemValue)        { EXPECT_EQ(static_cast<int>(ParamMode::Mem), 2); }
TEST(ASMWriterEnumTest, PModeEbpValue)        { EXPECT_EQ(static_cast<int>(ParamMode::Ebp), 3); }
TEST(ASMWriterEnumTest, PModeMemOffValue)     { EXPECT_EQ(static_cast<int>(ParamMode::MemOff), 4); }
TEST(ASMWriterEnumTest, PModeEbpOffValue)     { EXPECT_EQ(static_cast<int>(ParamMode::EbpOff), 5); }
TEST(ASMWriterEnumTest, PModeMemArrValue)     { EXPECT_EQ(static_cast<int>(ParamMode::MemArr), 6); }
TEST(ASMWriterEnumTest, PModeEbpArrValue)     { EXPECT_EQ(static_cast<int>(ParamMode::EbpArr), 7); }
TEST(ASMWriterEnumTest, PModeStackValue)      { EXPECT_EQ(static_cast<int>(ParamMode::Stack), 8); }
TEST(ASMWriterEnumTest, PModeMemRelValue)     { EXPECT_EQ(static_cast<int>(ParamMode::MemRel), 9); }
TEST(ASMWriterEnumTest, PModeEbpRelValue)     { EXPECT_EQ(static_cast<int>(ParamMode::EbpRel), 10); }
