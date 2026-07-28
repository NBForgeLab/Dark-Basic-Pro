#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace dbp::package {

class SecureBuffer {
public:
    using EraseFunction = void (*)(void*, std::size_t) noexcept;

    SecureBuffer() noexcept;
    ~SecureBuffer();

    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;
    SecureBuffer(SecureBuffer&& other) noexcept;
    SecureBuffer& operator=(SecureBuffer&& other) noexcept;

    static SecureBuffer FromBytes(std::vector<std::uint8_t> bytes);

#ifdef DBP_TESTS_COMPILATION
    static SecureBuffer FromBytesForTesting(
        std::vector<std::uint8_t> bytes,
        EraseFunction eraser) {
        return SecureBuffer(std::move(bytes), eraser);
    }
#endif

    const std::uint8_t* data() const noexcept {
        return bytes_.data();
    }

    std::uint8_t* data() noexcept {
        return bytes_.data();
    }

    std::size_t size() const noexcept {
        return bytes_.size();
    }

    bool empty() const noexcept {
        return bytes_.empty();
    }

    std::vector<std::uint8_t> CopyBytes() const {
        return bytes_;
    }

private:
    SecureBuffer(
        std::vector<std::uint8_t> bytes,
        EraseFunction eraser) noexcept;

    static void DefaultErase(void* memory, std::size_t size) noexcept;
    void Erase() noexcept;

    std::vector<std::uint8_t> bytes_;
    EraseFunction eraser_;
};

} // namespace dbp::package
