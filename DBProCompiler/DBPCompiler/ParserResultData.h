#pragma once
//
// Result Data — RAII wrapper with deep-copy semantics
//

#include <memory>

class CStr;
class CStructTable;

class CResultData
{
	public:
		// Construction
		CResultData() : m_dwType(0), m_dwDataOffset(0), m_pStruct(nullptr) {}

		// Deep-copy construction
		CResultData(const CResultData& other);
		CResultData& operator=(const CResultData& other);

		// Move construction (default)
		CResultData(CResultData&&) noexcept = default;
		CResultData& operator=(CResultData&&) noexcept = default;

		// Destruction — handled by unique_ptr
		~CResultData() = default;

		// Normal Literal Data
		std::unique_ptr<CStr>	m_pStringToken;

		// Optional Array Offset Data
		std::unique_ptr<CStr>	m_pAdditionalOffset;

		// Breakdown of Data
		DWORD			m_dwType;
		DWORD			m_dwDataOffset;
		CStructTable*	m_pStruct;
};
