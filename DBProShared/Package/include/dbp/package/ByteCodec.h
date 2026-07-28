#pragma once

#include "dbp/package/PackageError.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dbp::package {

class ByteWriter {
public:
    void WriteUInt16(std::uint16_t value);
    void WriteUInt32(std::uint32_t value);
    void WriteUInt64(std::uint64_t value);
    void WriteBytes(const std::uint8_t* bytes, std::size_t size);

    const std::vector<std::uint8_t>& Bytes() const noexcept {
        return bytes_;
    }

private:
    std::vector<std::uint8_t> bytes_;
};

class ByteReader {
public:
    ByteReader(const std::uint8_t* bytes, std::size_t size) noexcept;
    explicit ByteReader(const std::vector<std::uint8_t>& bytes) noexcept;

    PackageResult<std::uint16_t> ReadUInt16();
    PackageResult<std::uint32_t> ReadUInt32();
    PackageResult<std::uint64_t> ReadUInt64();
    PackageResult<std::vector<std::uint8_t>> ReadBytes(std::size_t size);

    std::size_t Position() const noexcept {
        return position_;
    }

    std::size_t Remaining() const noexcept {
        return size_ - position_;
    }

private:
    bool CanRead(std::size_t size) const noexcept;
    PackageError UnexpectedEnd(std::size_t requested) const;

    const std::uint8_t* bytes_;
    std::size_t size_;
    std::size_t position_ = 0;
};

PackageResult<std::uint64_t> CheckedAdd(
    std::uint64_t left,
    std::uint64_t right);
PackageResult<std::uint64_t> CheckedMultiply(
    std::uint64_t left,
    std::uint64_t right);

} // namespace dbp::package
