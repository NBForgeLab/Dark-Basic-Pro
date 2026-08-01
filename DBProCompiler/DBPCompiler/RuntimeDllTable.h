#pragma once

#include <cstdint>

namespace dbp::runtime
{
inline constexpr std::uint32_t DllCapacity = 256u;

[[nodiscard]] constexpr bool IsDllIndex(const std::uint32_t index) noexcept
{
	return index < DllCapacity;
}
}
