#include "dbp/package/SecureBuffer.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <utility>

namespace dbp::package {

SecureBuffer::SecureBuffer() noexcept
    : eraser_(&DefaultErase) {}

SecureBuffer::SecureBuffer(
    std::vector<std::uint8_t> bytes,
    const EraseFunction eraser) noexcept
    : bytes_(std::move(bytes)),
      eraser_(eraser == nullptr ? &DefaultErase : eraser) {}

SecureBuffer::~SecureBuffer() {
    Erase();
}

SecureBuffer::SecureBuffer(SecureBuffer&& other) noexcept
    : bytes_(std::move(other.bytes_)),
      eraser_(other.eraser_) {
    other.eraser_ = &DefaultErase;
}

SecureBuffer& SecureBuffer::operator=(SecureBuffer&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    Erase();
    bytes_ = std::move(other.bytes_);
    eraser_ = other.eraser_;
    other.eraser_ = &DefaultErase;
    return *this;
}

SecureBuffer SecureBuffer::FromBytes(std::vector<std::uint8_t> bytes) {
    return SecureBuffer(std::move(bytes), &DefaultErase);
}

void SecureBuffer::DefaultErase(
    void* const memory,
    const std::size_t size) noexcept {
    if (memory != nullptr && size != 0) {
        SecureZeroMemory(memory, size);
    }
}

void SecureBuffer::Erase() noexcept {
    if (!bytes_.empty()) {
        eraser_(bytes_.data(), bytes_.size());
        bytes_.clear();
    }
}

} // namespace dbp::package
