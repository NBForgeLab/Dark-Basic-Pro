// Characterization and correctness tests for DBPType enum and type metadata.
#include <gtest/gtest.h>
#include <type_traits>
#include "DataType.h"

TEST(DataTypeEnumTest, DBPTypeIsEnumClassWithUint32Underlying) {
    EXPECT_TRUE(std::is_enum<DBPType>::value);
    EXPECT_EQ(sizeof(DBPType), sizeof(uint32_t));
}

TEST(DataTypeEnumTest, FundamentalTypeValues) {
    EXPECT_EQ(static_cast<uint32_t>(DBPType::Integer), 1u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::Float), 2u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::String), 3u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::Boolean), 4u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::Byte), 5u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::Word), 6u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::Dword), 7u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::DoubleFloat), 8u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::DoubleInteger), 9u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::Label), 10u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::Dabel), 20u);
}

TEST(DataTypeEnumTest, ArrayTypeValues) {
    EXPECT_EQ(static_cast<uint32_t>(DBPType::IntegerArray), 101u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::FloatArray), 102u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::StringArray), 103u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::BooleanArray), 104u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::ByteArray), 105u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::WordArray), 106u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::DwordArray), 107u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::DoubleFloatArray), 108u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::DoubleIntegerArray), 109u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::UserDefinedArrayPtr), 1101u);
}

TEST(DataTypeEnumTest, UserDefinedTypeValues) {
    EXPECT_EQ(static_cast<uint32_t>(DBPType::UserDefinedPtr), 1001u);
    EXPECT_EQ(static_cast<uint32_t>(DBPType::UserDefinedArrayPtr), 1101u);
}

TEST(DataTypeEnumTest, IsArrayTypeQueries) {
    EXPECT_TRUE(IsArrayType(DBPType::IntegerArray));
    EXPECT_TRUE(IsArrayType(DBPType::FloatArray));
    EXPECT_TRUE(IsArrayType(DBPType::StringArray));
    EXPECT_TRUE(IsArrayType(DBPType::BooleanArray));
    EXPECT_TRUE(IsArrayType(DBPType::ByteArray));
    EXPECT_TRUE(IsArrayType(DBPType::WordArray));
    EXPECT_TRUE(IsArrayType(DBPType::DwordArray));
    EXPECT_TRUE(IsArrayType(DBPType::DoubleFloatArray));
    EXPECT_TRUE(IsArrayType(DBPType::DoubleIntegerArray));
    EXPECT_TRUE(IsArrayType(DBPType::UserDefinedArrayPtr));

    EXPECT_FALSE(IsArrayType(DBPType::Integer));
    EXPECT_FALSE(IsArrayType(DBPType::Float));
    EXPECT_FALSE(IsArrayType(DBPType::String));
    EXPECT_FALSE(IsArrayType(DBPType::UserDefinedPtr));
}

TEST(DataTypeEnumTest, IsUserDefinedTypeQueries) {
    EXPECT_TRUE(IsUserDefinedType(DBPType::UserDefinedPtr));
    EXPECT_TRUE(IsUserDefinedType(DBPType::UserDefinedArrayPtr));

    EXPECT_FALSE(IsUserDefinedType(DBPType::Integer));
    EXPECT_FALSE(IsUserDefinedType(DBPType::Float));
    EXPECT_FALSE(IsUserDefinedType(DBPType::String));
    EXPECT_FALSE(IsUserDefinedType(DBPType::IntegerArray));
}

TEST(DataTypeEnumTest, IsNumericTypeQueries) {
    EXPECT_TRUE(IsNumericType(DBPType::Integer));
    EXPECT_TRUE(IsNumericType(DBPType::Float));
    EXPECT_TRUE(IsNumericType(DBPType::Boolean));
    EXPECT_TRUE(IsNumericType(DBPType::Byte));
    EXPECT_TRUE(IsNumericType(DBPType::Word));
    EXPECT_TRUE(IsNumericType(DBPType::Dword));
    EXPECT_TRUE(IsNumericType(DBPType::DoubleFloat));
    EXPECT_TRUE(IsNumericType(DBPType::DoubleInteger));

    EXPECT_FALSE(IsNumericType(DBPType::String));
    EXPECT_FALSE(IsNumericType(DBPType::Label));
    EXPECT_FALSE(IsNumericType(DBPType::UserDefinedPtr));
}

TEST(DataTypeEnumTest, TypeSizesInNativeX64) {
    EXPECT_EQ(GetTypeSizeInBytes(DBPType::Boolean, 8), 1u);
    EXPECT_EQ(GetTypeSizeInBytes(DBPType::Byte, 8), 1u);
    EXPECT_EQ(GetTypeSizeInBytes(DBPType::Word, 8), 2u);
    EXPECT_EQ(GetTypeSizeInBytes(DBPType::Integer, 8), 4u);
    EXPECT_EQ(GetTypeSizeInBytes(DBPType::Float, 8), 4u);
    EXPECT_EQ(GetTypeSizeInBytes(DBPType::Dword, 8), 4u);
    EXPECT_EQ(GetTypeSizeInBytes(DBPType::DoubleFloat, 8), 8u);
    EXPECT_EQ(GetTypeSizeInBytes(DBPType::DoubleInteger, 8), 8u);
    EXPECT_EQ(GetTypeSizeInBytes(DBPType::String, 8), 8u);
    EXPECT_EQ(GetTypeSizeInBytes(DBPType::UserDefinedPtr, 8), 8u);
    EXPECT_EQ(GetTypeSizeInBytes(DBPType::IntegerArray, 8), 8u);
}

TEST(DataTypeEnumTest, TypeCharacterCodes) {
    EXPECT_EQ(GetTypeCharCode(DBPType::Integer), 'L');
    EXPECT_EQ(GetTypeCharCode(DBPType::Float), 'F');
    EXPECT_EQ(GetTypeCharCode(DBPType::String), 'S');
    EXPECT_EQ(GetTypeCharCode(DBPType::Boolean), 'B');
    EXPECT_EQ(GetTypeCharCode(DBPType::Byte), 'Y');
    EXPECT_EQ(GetTypeCharCode(DBPType::Word), 'W');
    EXPECT_EQ(GetTypeCharCode(DBPType::Dword), 'D');
    EXPECT_EQ(GetTypeCharCode(DBPType::DoubleFloat), 'O');
    EXPECT_EQ(GetTypeCharCode(DBPType::DoubleInteger), 'R');
    EXPECT_EQ(GetTypeCharCode(DBPType::Label), 'P');
    EXPECT_EQ(GetTypeCharCode(DBPType::Dabel), 'Q');
    EXPECT_EQ(GetTypeCharCode(DBPType::IntegerArray), 'm');
    EXPECT_EQ(GetTypeCharCode(DBPType::FloatArray), 'g');
    EXPECT_EQ(GetTypeCharCode(DBPType::StringArray), 't');
    EXPECT_EQ(GetTypeCharCode(DBPType::UserDefinedPtr), 'E');
    EXPECT_EQ(GetTypeCharCode(DBPType::UserDefinedArrayPtr), 'e');
}

TEST(DataTypeEnumTest, TypeNameStrings) {
    EXPECT_EQ(GetTypeNameString(DBPType::Integer), "integer");
    EXPECT_EQ(GetTypeNameString(DBPType::Float), "float");
    EXPECT_EQ(GetTypeNameString(DBPType::String), "string");
    EXPECT_EQ(GetTypeNameString(DBPType::Boolean), "boolean");
    EXPECT_EQ(GetTypeNameString(DBPType::Byte), "byte");
    EXPECT_EQ(GetTypeNameString(DBPType::Word), "word");
    EXPECT_EQ(GetTypeNameString(DBPType::Dword), "dword");
    EXPECT_EQ(GetTypeNameString(DBPType::DoubleFloat), "double float");
    EXPECT_EQ(GetTypeNameString(DBPType::DoubleInteger), "double integer");
    EXPECT_EQ(GetTypeNameString(DBPType::UserDefinedPtr), "userdefined var ptr");
    EXPECT_EQ(GetTypeNameString(DBPType::UserDefinedArrayPtr), "userdefined array ptr");
}
