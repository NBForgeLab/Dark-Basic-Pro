#pragma once

#include "BinaryCodec.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

namespace dbp::abi {

template <std::size_t AddressBits>
struct TargetAbiTraits;

template <>
struct TargetAbiTraits<32> {
    using address_type = std::uint32_t;
    static constexpr std::size_t address_size = sizeof(address_type);
};

template <>
struct TargetAbiTraits<64> {
    using address_type = std::uint64_t;
    static constexpr std::size_t address_size = sizeof(address_type);
};

using TargetAbi32 = TargetAbiTraits<32>;
using TargetAbi64 = TargetAbiTraits<64>;

// Dark Basic Professional compiler target program ABI representation.
// The project is x64-only (x86 CI/workflows removed): varspace address slots
// (strings, arrays, UDT pointers) are 8 bytes and pointer reads/writes in
// the backend are full-width. Kept behind the alias so a 32-bit target can
// be reintroduced as a pure compile-time switch.
using ActiveTargetAbi = TargetAbi64;

static_assert(TargetAbi32::address_size == 4);
static_assert(TargetAbi64::address_size == 8);

template <typename Abi>
[[nodiscard]] std::optional<typename Abi::address_type> ReadAddress(
    const void* data,
    const std::size_t size,
    const std::size_t offset) noexcept
{
    return binary::ReadTrivial<typename Abi::address_type>(data, size, offset);
}

template <typename Abi>
[[nodiscard]] std::optional<std::uintptr_t> ToHostAddress(
    const typename Abi::address_type value) noexcept
{
    using TargetAddress = typename Abi::address_type;
    static_assert(std::is_unsigned_v<TargetAddress>);

    if constexpr (sizeof(TargetAddress) > sizeof(std::uintptr_t)) {
        if (value > static_cast<TargetAddress>(
                        (std::numeric_limits<std::uintptr_t>::max)())) {
            return std::nullopt;
        }
    }

    return static_cast<std::uintptr_t>(value);
}

template <typename Pointer, typename Abi = ActiveTargetAbi>
[[nodiscard]] std::optional<Pointer> ReadPointer(
    const void* data,
    const std::size_t size,
    const std::size_t offset) noexcept
{
    static_assert(std::is_pointer_v<Pointer>);

    const auto targetAddress = ReadAddress<Abi>(data, size, offset);
    if (!targetAddress)
        return std::nullopt;

    const auto hostAddress = ToHostAddress<Abi>(*targetAddress);
    if (!hostAddress)
        return std::nullopt;

    return reinterpret_cast<Pointer>(*hostAddress);
}

} // namespace dbp::abi
