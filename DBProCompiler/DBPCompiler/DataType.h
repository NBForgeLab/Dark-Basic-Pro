#pragma once
#include <cstdint>

enum class DataType : uint8_t {
    Unknown = 0,
    Array   = 1,
    Integer = 2,
    String  = 3,
    Float   = 4
};
