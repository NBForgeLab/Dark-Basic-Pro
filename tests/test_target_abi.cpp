#include <gtest/gtest.h>

#include "TargetABI.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace {

TEST(TargetAbiTest, ActiveTargetIsPe32Plus) {
    // Wave 5: the project is x64-only — varspace address slots (strings,
    // arrays, UDT pointers) and backend pointer reads are 8 bytes.
    static_assert(std::is_same_v<dbp::abi::ActiveTargetAbi, dbp::abi::TargetAbi64>);
    EXPECT_EQ(dbp::abi::ActiveTargetAbi::address_size, 8U);
}

TEST(TargetAbiTest, DefaultPointerReadIsFullWidth) {
    std::array<std::byte, 8> bytes{};
    const std::uint64_t storedAddress = 0x7FFF000088889999ULL;
    std::memcpy(bytes.data(), &storedAddress, sizeof(storedAddress));

    const auto pointer =
        dbp::abi::ReadPointer<char*>(bytes.data(), bytes.size(), 0);

    ASSERT_TRUE(pointer.has_value());
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(*pointer), storedAddress);
}

TEST(TargetAbiTest, ReadsUnalignedPe32AddressWithoutAdjacentBytes) {
    std::array<std::byte, 9> bytes{};
    const std::uint32_t expected = 0x12345678U;
    const std::uint32_t adjacent = 0xDEADBEEFU;
    std::memcpy(bytes.data() + 1, &expected, sizeof(expected));
    std::memcpy(bytes.data() + 5, &adjacent, sizeof(adjacent));

    const auto value = dbp::abi::ReadAddress<dbp::abi::TargetAbi32>(
        bytes.data(), bytes.size(), 1);

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, expected);
}

TEST(TargetAbiTest, RejectsTruncatedAndOutOfRangeSlots) {
    std::array<std::byte, 4> bytes{};
    EXPECT_FALSE(dbp::abi::ReadAddress<dbp::abi::TargetAbi32>(
        bytes.data(), bytes.size(), 1));
    EXPECT_FALSE(dbp::abi::ReadAddress<dbp::abi::TargetAbi32>(
        bytes.data(), bytes.size(), bytes.size() + 1));
    EXPECT_FALSE(dbp::abi::ReadAddress<dbp::abi::TargetAbi32>(
        nullptr, bytes.size(), 0));
}

TEST(TargetAbiTest, ModelsFullPe32PlusAddressOnAnyHost) {
    std::array<std::byte, 8> bytes{};
    const std::uint64_t expected = 0x7FFF000088889999ULL;
    std::memcpy(bytes.data(), &expected, sizeof(expected));

    const auto value = dbp::abi::ReadAddress<dbp::abi::TargetAbi64>(
        bytes.data(), bytes.size(), 0);

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, expected);
}

TEST(TargetAbiTest, HostConversionRejectsNarrowing) {
    const auto converted = dbp::abi::ToHostAddress<dbp::abi::TargetAbi64>(
        std::numeric_limits<std::uint64_t>::max());
    if constexpr (sizeof(std::uintptr_t) < sizeof(std::uint64_t)) {
        EXPECT_FALSE(converted.has_value());
    } else {
        ASSERT_TRUE(converted.has_value());
        EXPECT_EQ(*converted, std::numeric_limits<std::uintptr_t>::max());
    }
}

TEST(TargetAbiTest, ReadsNullPointerAsValidSlot) {
    std::array<std::byte, 4> bytes{};

    const auto pointer =
        dbp::abi::ReadPointer<char*, dbp::abi::TargetAbi32>(
            bytes.data(), bytes.size(), 0);

    ASSERT_TRUE(pointer.has_value());
    EXPECT_EQ(*pointer, nullptr);
}

TEST(TargetAbiTest, ReadsRepresentablePointerValue) {
    std::array<std::byte, 4> bytes{};
    const std::uint32_t storedAddress = 0x00123456U;
    std::memcpy(bytes.data(), &storedAddress, sizeof(storedAddress));

    const auto pointer =
        dbp::abi::ReadPointer<char*, dbp::abi::TargetAbi32>(
            bytes.data(), bytes.size(), 0);

    ASSERT_TRUE(pointer.has_value());
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(*pointer), storedAddress);
}

TEST(TargetAbiTest, RejectsTruncatedPointerSlot) {
    std::array<std::byte, 3> bytes{};
    EXPECT_FALSE((dbp::abi::ReadPointer<char*, dbp::abi::TargetAbi32>(
        bytes.data(), bytes.size(), 0)));
}

} // namespace
