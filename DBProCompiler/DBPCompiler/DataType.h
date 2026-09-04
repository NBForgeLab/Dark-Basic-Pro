#pragma once
#include <cstdint>
#include <cstddef>
#include <string_view>

/**
 * @file DataType.h
 * @brief Strongly typed enumeration of DarkBasic Pro data types and their properties.
 */

enum class DBPType : uint32_t {
    Unknown               = 0,
    Integer               = 1,
    Float                 = 2,
    String                = 3,
    Boolean               = 4,
    Byte                  = 5,
    Word                  = 6,
    Dword                 = 7,
    DoubleFloat           = 8,
    DoubleInteger         = 9,
    Label                 = 10,
    Dabel                 = 20,
    IntegerArray          = 101,
    FloatArray            = 102,
    StringArray           = 103,
    BooleanArray          = 104,
    ByteArray             = 105,
    WordArray             = 106,
    DwordArray            = 107,
    DoubleFloatArray      = 108,
    DoubleIntegerArray    = 109,
    AnyTypeNonCasted      = 501,
    UserDefinedPtr        = 1001,
    UserDefinedArrayPtr   = 1101
};

// Legacy compatibility enum
enum class DataType : uint8_t {
    Unknown = 0,
    Array   = 1,
    Integer = 2,
    String  = 3,
    Float   = 4
};

[[nodiscard]] constexpr bool IsArrayType(DBPType type) noexcept {
    const auto val = static_cast<uint32_t>(type);
    return (val >= 101 && val <= 109) || val == 1101;
}

[[nodiscard]] constexpr bool IsUserDefinedType(DBPType type) noexcept {
    const auto val = static_cast<uint32_t>(type);
    return val == 1001 || val == 1101;
}

[[nodiscard]] constexpr bool IsPointerOrHandleType(DBPType type) noexcept {
    switch (type) {
        case DBPType::String:
        case DBPType::Dabel:
        case DBPType::UserDefinedPtr:
        case DBPType::UserDefinedArrayPtr:
        case DBPType::IntegerArray:
        case DBPType::FloatArray:
        case DBPType::StringArray:
        case DBPType::BooleanArray:
        case DBPType::ByteArray:
        case DBPType::WordArray:
        case DBPType::DwordArray:
        case DBPType::DoubleFloatArray:
        case DBPType::DoubleIntegerArray:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] constexpr bool IsPointerOrHandleType(uint32_t typeVal) noexcept {
    return IsPointerOrHandleType(static_cast<DBPType>(typeVal));
}

/**
 * @brief True when the value being moved is itself a pointer, so it needs a
 *        pointer-width operand.
 *
 * Distinct from IsPointerOrHandleType, which asks whether the variable slot
 * holds a pointer and is therefore true for every array type (the handle is
 * 64-bit even when the elements are bytes). This asks about the element value
 * instead: a numeric array element is a value, while a string or user-defined
 * element is an address.
 */
[[nodiscard]] constexpr bool IsPointerElementValue(DBPType type) noexcept {
    switch (type) {
        case DBPType::String:
        case DBPType::StringArray:
        case DBPType::UserDefinedPtr:
        case DBPType::UserDefinedArrayPtr:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] constexpr bool IsPointerElementValue(uint32_t typeVal) noexcept {
    return IsPointerElementValue(static_cast<DBPType>(typeVal));
}

[[nodiscard]] constexpr bool IsNumericType(DBPType type) noexcept {
    switch (type) {
        case DBPType::Integer:
        case DBPType::Float:
        case DBPType::Boolean:
        case DBPType::Byte:
        case DBPType::Word:
        case DBPType::Dword:
        case DBPType::DoubleFloat:
        case DBPType::DoubleInteger:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] constexpr bool IsFloatingPointType(DBPType type) noexcept {
    return type == DBPType::Float || type == DBPType::DoubleFloat;
}

[[nodiscard]] constexpr bool IsIntegerLikeType(DBPType type) noexcept {
    switch (type) {
        case DBPType::Integer:
        case DBPType::Boolean:
        case DBPType::Byte:
        case DBPType::Word:
        case DBPType::Dword:
        case DBPType::DoubleInteger:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] constexpr size_t GetTypeSizeInBytes(DBPType type, size_t pointerSize = 8) noexcept {
    switch (type) {
        case DBPType::Boolean:
        case DBPType::Byte:
            return 1;
        case DBPType::Word:
            return 2;
        case DBPType::Integer:
        case DBPType::Float:
        case DBPType::Dword:
        case DBPType::Label:
        case DBPType::Dabel:
        case DBPType::AnyTypeNonCasted:
            return 4;
        case DBPType::DoubleFloat:
        case DBPType::DoubleInteger:
            return 8;
        case DBPType::String:
        case DBPType::UserDefinedPtr:
        case DBPType::IntegerArray:
        case DBPType::FloatArray:
        case DBPType::StringArray:
        case DBPType::BooleanArray:
        case DBPType::ByteArray:
        case DBPType::WordArray:
        case DBPType::DwordArray:
        case DBPType::DoubleFloatArray:
        case DBPType::DoubleIntegerArray:
        case DBPType::UserDefinedArrayPtr:
            return pointerSize;
        default:
            return 4;
    }
}

[[nodiscard]] constexpr char GetTypeCharCode(DBPType type) noexcept {
    switch (type) {
        case DBPType::Integer:             return 'L';
        case DBPType::Float:               return 'F';
        case DBPType::String:              return 'S';
        case DBPType::Boolean:             return 'B';
        case DBPType::Byte:                return 'Y';
        case DBPType::Word:                return 'W';
        case DBPType::Dword:               return 'D';
        case DBPType::DoubleFloat:         return 'O';
        case DBPType::DoubleInteger:       return 'R';
        case DBPType::Label:               return 'P';
        case DBPType::Dabel:               return 'Q';
        case DBPType::IntegerArray:        return 'm';
        case DBPType::FloatArray:          return 'g';
        case DBPType::StringArray:         return 't';
        case DBPType::BooleanArray:        return 'c';
        case DBPType::ByteArray:           return 'z';
        case DBPType::WordArray:           return 'x';
        case DBPType::DwordArray:          return 'e';
        case DBPType::DoubleFloatArray:    return 'u';
        case DBPType::DoubleIntegerArray:  return 'v';
        case DBPType::AnyTypeNonCasted:    return 'X';
        case DBPType::UserDefinedPtr:      return 'E';
        case DBPType::UserDefinedArrayPtr: return 'e';
        default:                           return '?';
    }
}

[[nodiscard]] constexpr std::string_view GetTypeNameString(DBPType type) noexcept {
    switch (type) {
        case DBPType::Integer:             return "integer";
        case DBPType::Float:               return "float";
        case DBPType::String:              return "string";
        case DBPType::Boolean:             return "boolean";
        case DBPType::Byte:                return "byte";
        case DBPType::Word:                return "word";
        case DBPType::Dword:               return "dword";
        case DBPType::DoubleFloat:         return "double float";
        case DBPType::DoubleInteger:       return "double integer";
        case DBPType::Label:               return "label";
        case DBPType::Dabel:               return "dabel";
        case DBPType::IntegerArray:        return "integer array";
        case DBPType::FloatArray:          return "float array";
        case DBPType::StringArray:         return "string array";
        case DBPType::BooleanArray:        return "boolean array";
        case DBPType::ByteArray:           return "byte array";
        case DBPType::WordArray:           return "word array";
        case DBPType::DwordArray:          return "dword array";
        case DBPType::DoubleFloatArray:    return "double float array";
        case DBPType::DoubleIntegerArray:  return "double integer array";
        case DBPType::AnyTypeNonCasted:    return "anytype non casted";
        case DBPType::UserDefinedPtr:      return "userdefined var ptr";
        case DBPType::UserDefinedArrayPtr: return "userdefined array ptr";
        default:                           return "unknown";
    }
}
