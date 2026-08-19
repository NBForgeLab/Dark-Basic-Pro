// StringUtils.h - modern, dependency-free string comparison helpers
//
// Replaces the deprecated C runtime _stricmp with pure C++17 std::string_view
// implementations that match the legacy semantics exactly (ASCII case folding,
// byte-wise lexicographic ordering, NUL-terminated input like C strings).
//////////////////////////////////////////////////////////////////////

#ifndef DBP_STRINGUTILS_H
#define DBP_STRINGUTILS_H

#include <cctype>
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

} // namespace dbp

#endif // DBP_STRINGUTILS_H
