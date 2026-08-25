#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include "macros.h"
#include "Str.h"

class CDeclaration  
{
	public:
		CDeclaration();
		~CDeclaration();
		void Add(CDeclaration* pNew);
		[[nodiscard]] CDeclaration* GetNext(void) noexcept { return m_pNext.get(); }
		[[nodiscard]] const CDeclaration* GetNext(void) const noexcept { return m_pNext.get(); }
		[[nodiscard]] CDeclaration* GetPrev(void) noexcept { return m_pPrev; }
		[[nodiscard]] const CDeclaration* GetPrev(void) const noexcept { return m_pPrev; }
		CDeclaration* Find(std::string_view name, uint32_t dwArrFlag);

		void SetLineNumber(uint32_t dwLine) noexcept { m_dwLineNumber=dwLine; }
		[[nodiscard]] uint32_t GetLineNumber(void) const noexcept { return m_dwLineNumber; }

		void SetArr(uint32_t dwArr) noexcept { m_dwArr = dwArr; }
		void SetArrValue(std::string_view arrVal) { m_pArrValue = std::make_unique<CStr>(arrVal); }
		void SetName(std::string_view name) { m_pName = std::make_unique<CStr>(name); }
		void SetType(std::string_view type) { m_pType = std::make_unique<CStr>(type); }
		void SetInit(std::string_view init) { m_pInit = std::make_unique<CStr>(init); }
		void SetOffset(uint32_t dwOffset) noexcept { m_dwOffset = dwOffset; }
		void SetDataSize(uint32_t dwSize) noexcept { m_dwDataSize = dwSize; }
		void SetDecData(uint32_t dwDecArr, std::string_view arrVal, std::string_view name, std::string_view type, std::string_view init, uint32_t lineNumberRef);
		void SetDecData(uint32_t dwDecArr, const char* pDecArrValue, const char* pDecName, const char* pDecType, const char* pDecInit, uint32_t LineNumberRef) {
			SetDecData(dwDecArr,
					   pDecArrValue ? std::string_view(pDecArrValue) : std::string_view{},
					   pDecName ? std::string_view(pDecName) : std::string_view{},
					   pDecType ? std::string_view(pDecType) : std::string_view{},
					   pDecInit ? std::string_view(pDecInit) : std::string_view{},
					   LineNumberRef);
		}

		bool GetNumberOfDecsInChain(uint32_t* pdwCount);
		bool GetNumberOfDecsInChain(unsigned long* pdwCount) {
			if (!pdwCount) return false;
			uint32_t count = static_cast<uint32_t>(*pdwCount);
			bool res = GetNumberOfDecsInChain(&count);
			*pdwCount = count;
			return res;
		}
		std::string GetTypeStringOfDecsInChain(void);

		[[nodiscard]] CStr* GetName(void) noexcept { return m_pName.get(); }
		[[nodiscard]] const CStr* GetName(void) const noexcept { return m_pName.get(); }
		[[nodiscard]] std::string_view GetNameView(void) const noexcept { return m_pName ? m_pName->View() : std::string_view{}; }
		[[nodiscard]] const char* GetNameStr(void) const noexcept { return m_pName ? m_pName->c_str() : ""; }

		[[nodiscard]] CStr* GetType(void) noexcept { return m_pType.get(); }
		[[nodiscard]] const CStr* GetType(void) const noexcept { return m_pType.get(); }
		[[nodiscard]] std::string_view GetTypeView(void) const noexcept { return m_pType ? m_pType->View() : std::string_view{}; }
		[[nodiscard]] const char* GetTypeStr(void) const noexcept { return m_pType ? m_pType->c_str() : ""; }

		[[nodiscard]] CStr* GetArrValue(void) noexcept { return m_pArrValue.get(); }
		[[nodiscard]] const CStr* GetArrValue(void) const noexcept { return m_pArrValue.get(); }
		[[nodiscard]] std::string_view GetArrValueView(void) const noexcept { return m_pArrValue ? m_pArrValue->View() : std::string_view{}; }

		[[nodiscard]] CStr* GetInit(void) noexcept { return m_pInit.get(); }
		[[nodiscard]] const CStr* GetInit(void) const noexcept { return m_pInit.get(); }
		[[nodiscard]] std::string_view GetInitView(void) const noexcept { return m_pInit ? m_pInit->View() : std::string_view{}; }

		[[nodiscard]] uint32_t GetArrFlag(void) const noexcept { return m_dwArr; }
		[[nodiscard]] uint32_t GetOffset(void) const noexcept { return m_dwOffset; }
		[[nodiscard]] uint32_t GetDataSize(void) const noexcept { return m_dwDataSize; }

		bool WriteDBM(void);

	private:

		// Debug Data
		uint32_t				m_dwLineNumber;

		// Declaration Data
		uint32_t				m_dwArr;
		std::unique_ptr<CStr>	m_pArrValue;
		std::unique_ptr<CStr>	m_pName;
		std::unique_ptr<CStr>	m_pType;
		std::unique_ptr<CStr>	m_pInit;
		uint32_t				m_dwOffset;
		uint32_t				m_dwDataSize;

		// Hierarchy Data
		std::unique_ptr<CDeclaration>	m_pNext;
		CDeclaration*					m_pPrev;  // Non-owning back-pointer
};