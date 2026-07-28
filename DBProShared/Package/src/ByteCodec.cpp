#include "dbp/package/ByteCodec.h"

#include <limits>
#include <sstream>

namespace dbp::package {

namespace {

template <typename Integer>
void WriteLittleEndian(std::vector<std::uint8_t>& output, Integer value) {
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        output.push_back(static_cast<std::uint8_t>(
            (value >> (index * 8U)) & static_cast<Integer>(0xFFU)));
    }
}

PackageError OverflowError(const char* operation) {
    return {
        PackageErrorCode::ArithmeticOverflow,
        std::string("Unsigned 64-bit ") + operation + " overflow.",
        std::nullopt,
    };
}

} // namespace

void ByteWriter::WriteUInt16(const std::uint16_t value) {
    WriteLittleEndian(bytes_, value);
}

void ByteWriter::WriteUInt32(const std::uint32_t value) {
    WriteLittleEndian(bytes_, value);
}

void ByteWriter::WriteUInt64(const std::uint64_t value) {
    WriteLittleEndian(bytes_, value);
}

void ByteWriter::WriteBytes(
    const std::uint8_t* const bytes,
    const std::size_t size) {
    if (size == 0) {
        return;
    }
    bytes_.insert(bytes_.end(), bytes, bytes + size);
}

ByteReader::ByteReader(
    const std::uint8_t* const bytes,
    const std::size_t size) noexcept
    : bytes_(bytes),
      size_(size) {}

ByteReader::ByteReader(const std::vector<std::uint8_t>& bytes) noexcept
    : ByteReader(bytes.data(), bytes.size()) {}

bool ByteReader::CanRead(const std::size_t size) const noexcept {
    return size <= Remaining();
}

PackageError ByteReader::UnexpectedEnd(const std::size_t requested) const {
    std::ostringstream message;
    message << "Requested " << requested << " bytes with "
            << Remaining() << " bytes remaining.";
    return {
        PackageErrorCode::UnexpectedEnd,
        message.str(),
        static_cast<std::uint64_t>(position_),
    };
}

PackageResult<std::uint16_t> ByteReader::ReadUInt16() {
    constexpr std::size_t width = sizeof(std::uint16_t);
    if (!CanRead(width)) {
        return PackageResult<std::uint16_t>::Failure(UnexpectedEnd(width));
    }

    std::uint16_t value = 0;
    for (std::size_t index = 0; index < width; ++index) {
        value |= static_cast<std::uint16_t>(bytes_[position_ + index])
            << (index * 8U);
    }
    position_ += width;
    return PackageResult<std::uint16_t>::Success(value);
}

PackageResult<std::uint32_t> ByteReader::ReadUInt32() {
    constexpr std::size_t width = sizeof(std::uint32_t);
    if (!CanRead(width)) {
        return PackageResult<std::uint32_t>::Failure(UnexpectedEnd(width));
    }

    std::uint32_t value = 0;
    for (std::size_t index = 0; index < width; ++index) {
        value |= static_cast<std::uint32_t>(bytes_[position_ + index])
            << (index * 8U);
    }
    position_ += width;
    return PackageResult<std::uint32_t>::Success(value);
}

PackageResult<std::uint64_t> ByteReader::ReadUInt64() {
    constexpr std::size_t width = sizeof(std::uint64_t);
    if (!CanRead(width)) {
        return PackageResult<std::uint64_t>::Failure(UnexpectedEnd(width));
    }

    std::uint64_t value = 0;
    for (std::size_t index = 0; index < width; ++index) {
        value |= static_cast<std::uint64_t>(bytes_[position_ + index])
            << (index * 8U);
    }
    position_ += width;
    return PackageResult<std::uint64_t>::Success(value);
}

PackageResult<std::vector<std::uint8_t>> ByteReader::ReadBytes(
    const std::size_t size) {
    if (!CanRead(size)) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            UnexpectedEnd(size));
    }

    std::vector<std::uint8_t> result;
    result.reserve(size);
    result.insert(
        result.end(),
        bytes_ + position_,
        bytes_ + position_ + size);
    position_ += size;
    return PackageResult<std::vector<std::uint8_t>>::Success(
        std::move(result));
}

PackageResult<std::uint64_t> CheckedAdd(
    const std::uint64_t left,
    const std::uint64_t right) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return PackageResult<std::uint64_t>::Failure(OverflowError("addition"));
    }
    return PackageResult<std::uint64_t>::Success(left + right);
}

PackageResult<std::uint64_t> CheckedMultiply(
    const std::uint64_t left,
    const std::uint64_t right) {
    if (left != 0 &&
        right > std::numeric_limits<std::uint64_t>::max() / left) {
        return PackageResult<std::uint64_t>::Failure(
            OverflowError("multiplication"));
    }
    return PackageResult<std::uint64_t>::Success(left * right);
}

} // namespace dbp::package
