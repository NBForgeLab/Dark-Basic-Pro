#include <gtest/gtest.h>

#include "dbp/package/ByteCodec.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace {

using dbp::package::ByteReader;
using dbp::package::ByteWriter;
using dbp::package::CheckedAdd;
using dbp::package::CheckedMultiply;

TEST(PackageByteCodecTest, WritesFixedWidthIntegersInLittleEndianOrder) {
    ByteWriter writer;

    writer.WriteUInt16(0x1234U);
    writer.WriteUInt32(0x89ABCDEFU);
    writer.WriteUInt64(0x0123456789ABCDEFULL);

    const std::vector<std::uint8_t> expected{
        0x34, 0x12,
        0xEF, 0xCD, 0xAB, 0x89,
        0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01,
    };
    EXPECT_EQ(writer.Bytes(), expected);
}

TEST(PackageByteCodecTest, ReadsFixedWidthIntegersInLittleEndianOrder) {
    const std::vector<std::uint8_t> bytes{
        0x34, 0x12,
        0xEF, 0xCD, 0xAB, 0x89,
        0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01,
    };
    ByteReader reader(bytes);

    const auto value16 = reader.ReadUInt16();
    const auto value32 = reader.ReadUInt32();
    const auto value64 = reader.ReadUInt64();

    ASSERT_TRUE(value16);
    ASSERT_TRUE(value32);
    ASSERT_TRUE(value64);
    EXPECT_EQ(value16.value(), 0x1234U);
    EXPECT_EQ(value32.value(), 0x89ABCDEFU);
    EXPECT_EQ(value64.value(), 0x0123456789ABCDEFULL);
    EXPECT_EQ(reader.Position(), bytes.size());
}

TEST(PackageByteCodecTest, TruncatedReadDoesNotAdvanceCursor) {
    const std::vector<std::uint8_t> bytes{0x34, 0x12, 0xEF};
    ByteReader reader(bytes);

    ASSERT_TRUE(reader.ReadUInt16());
    const auto positionBeforeFailure = reader.Position();
    const auto result = reader.ReadUInt32();

    EXPECT_FALSE(result);
    EXPECT_EQ(reader.Position(), positionBeforeFailure);
}

TEST(PackageByteCodecTest, CheckedArithmeticRejectsOverflow) {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();

    EXPECT_FALSE(CheckedAdd(maximum, 1U));
    EXPECT_FALSE(CheckedMultiply(maximum, 2U));

    const auto sum = CheckedAdd(40U, 2U);
    const auto product = CheckedMultiply(6U, 7U);
    ASSERT_TRUE(sum);
    ASSERT_TRUE(product);
    EXPECT_EQ(sum.value(), 42U);
    EXPECT_EQ(product.value(), 42U);
}

} // namespace
