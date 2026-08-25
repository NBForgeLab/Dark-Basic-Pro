// StringUtils.h - modern, dependency-free string comparison helpers
//
// Replaces the deprecated C runtime _stricmp with pure C++17 std::string_view
// implementations that match the legacy semantics exactly (ASCII case folding,
// byte-wise lexicographic ordering, NUL-terminated input like C strings).
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace dbp
{

	// Case-insensitive three-way comparison, mirroring _stricmp (ASCII only).
	// Returns a value < 0, == 0, or > 0 when a < b, a == b, or a > b.
	inline int icompare(std::string_view a, std::string_view b) noexcept
	{
		const size_t nCommon = (a.size() < b.size()) ? a.size() : b.size();
		for (size_t i = 0; i < nCommon; ++i)
		{
			const unsigned char ca = static_cast<unsigned char>(a[i]);
			const unsigned char cb = static_cast<unsigned char>(b[i]);
			const unsigned char la = static_cast<unsigned char>(std::tolower(ca));
			const unsigned char lb = static_cast<unsigned char>(std::tolower(cb));
			if (la != lb)
				return (la < lb) ? -1 : 1;
		}
		if (a.size() == b.size())
			return 0;
		return (a.size() < b.size()) ? -1 : 1;
	}

	// Case-insensitive equality, mirroring (_stricmp(a, b) == 0).
	inline bool iequals(std::string_view a, std::string_view b) noexcept
	{
		return icompare(a, b) == 0;
	}

	// Case-insensitive prefix check.
	[[nodiscard]] inline bool starts_with_ci(std::string_view text, std::string_view prefix) noexcept
	{
		return text.size() >= prefix.size() && iequals(text.substr(0, prefix.size()), prefix);
	}

	// Case-insensitive suffix check.
	[[nodiscard]] inline bool ends_with_ci(std::string_view text, std::string_view suffix) noexcept
	{
		return text.size() >= suffix.size() && iequals(text.substr(text.size() - suffix.size()), suffix);
	}

	namespace detail
	{
		[[nodiscard]] inline bool is_space(unsigned char c) noexcept
		{
			return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
		}
	}

	// Trims leading whitespace (space/tab/CR/LF) and returns a new string.
	[[nodiscard]] inline std::string trim_left(std::string_view text)
	{
		size_t i = 0;
		while (i < text.size() && detail::is_space(static_cast<unsigned char>(text[i])))
			++i;
		return std::string(text.substr(i));
	}

	// Trims trailing whitespace (space/tab/CR/LF) and returns a new string.
	[[nodiscard]] inline std::string trim_right(std::string_view text)
	{
		size_t i = text.size();
		while (i > 0 && detail::is_space(static_cast<unsigned char>(text[i - 1])))
			--i;
		return std::string(text.substr(0, i));
	}

	// Trims both ends of whitespace (space/tab/CR/LF).
	[[nodiscard]] inline std::string trim(std::string_view text)
	{
		return trim_right(trim_left(text));
	}

	// ASCII-only uppercase copy (mirrors the legacy _strupr behaviour).
	[[nodiscard]] inline std::string to_upper_copy(std::string_view text)
	{
		std::string result(text);
		for (char& c : result)
			if (c >= 'a' && c <= 'z')
				c = static_cast<char>(c - 'a' + 'A');
		return result;
	}

	// ASCII-only lowercase copy (mirrors the legacy _strlwr behaviour).
	[[nodiscard]] inline std::string to_lower_copy(std::string_view text)
	{
		std::string result(text);
		for (char& c : result)
			if (c >= 'A' && c <= 'Z')
				c = static_cast<char>(c - 'A' + 'a');
		return result;
	}

} // namespace dbp
