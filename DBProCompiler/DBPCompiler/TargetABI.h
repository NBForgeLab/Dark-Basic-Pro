#pragma once

#include "BinaryCodec.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

namespace dbp::abi {

// Dark Basic Professional targets native x64 exclusively: generated code,
// variable handles, and every persisted address are full 64-bit values.
struct TargetAbi64 {
    using address_type = std::uint64_t;
    static constexpr std::size_t address_size = sizeof(address_type);
};

static_assert(TargetAbi64::address_size == 8);

// Program ABI used by the compiler, the runtime bundle, and the tests.
using ActiveTargetAbi = TargetAbi64;

[[nodiscard]] inline std::optional<TargetAbi64::address_type> ReadAddress(
    const void* data,
    const std::size_t size,
    const std::size_t offset) noexcept
{
    return binary::ReadTrivial<TargetAbi64::address_type>(data, size, offset);
}

[[nodiscard]] inline std::optional<std::uintptr_t> ToHostAddress(
    const TargetAbi64::address_type value) noexcept
{
    static_assert(std::is_unsigned_v<TargetAbi64::address_type>);

    if constexpr (sizeof(TargetAbi64::address_type) > sizeof(std::uintptr_t)) {
        if (value > static_cast<TargetAbi64::address_type>(
                        (std::numeric_limits<std::uintptr_t>::max)())) {
            return std::nullopt;
        }
    }

    return static_cast<std::uintptr_t>(value);
}

[[nodiscard]] inline std::optional<TargetAbi64::address_type> FromHostAddress(
    const std::uintptr_t value) noexcept
{
    static_assert(std::is_unsigned_v<TargetAbi64::address_type>);

    if constexpr (sizeof(std::uintptr_t) > sizeof(TargetAbi64::address_type)) {
        if (value > static_cast<std::uintptr_t>(
                        (std::numeric_limits<TargetAbi64::address_type>::max)())) {
            return std::nullopt;
        }
    }

    return static_cast<TargetAbi64::address_type>(value);
}

template <typename Pointer>
[[nodiscard]] std::optional<Pointer> ReadPointer(
    const void* data,
    const std::size_t size,
    const std::size_t offset) noexcept
{
    static_assert(std::is_pointer_v<Pointer>);

    const auto targetAddress = ReadAddress(data, size, offset);
    if (!targetAddress)
        return std::nullopt;

    const auto hostAddress = ToHostAddress(*targetAddress);
    if (!hostAddress)
        return std::nullopt;

    return reinterpret_cast<Pointer>(*hostAddress);
}

} // namespace dbp::abi
