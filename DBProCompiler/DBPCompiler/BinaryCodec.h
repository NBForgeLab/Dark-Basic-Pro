#pragma once

#include <cstddef>
#include <cstring>
#include <optional>
#include <type_traits>

namespace dbp::binary {

template <typename T>
[[nodiscard]] std::optional<T> ReadTrivial(
    const void* data,
    const std::size_t size,
    const std::size_t offset) noexcept
{
    static_assert(
        std::is_trivially_copyable_v<T>,
        "Binary decoding requires a trivially-copyable destination type.");

    if (data == nullptr || offset > size || sizeof(T) > size - offset)
        return std::nullopt;

    T value{};
    const auto* bytes = static_cast<const std::byte*>(data);
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

template <typename T>
[[nodiscard]] bool WriteTrivial(
    void* data,
    const std::size_t size,
    const std::size_t offset,
    const T& value) noexcept
{
    static_assert(
        std::is_trivially_copyable_v<T>,
        "Binary encoding requires a trivially-copyable source type.");

    if (data == nullptr || offset > size || sizeof(T) > size - offset)
        return false;

    auto* bytes = static_cast<std::byte*>(data);
    std::memcpy(bytes + offset, &value, sizeof(value));
    return true;
}

} // namespace dbp::binary
