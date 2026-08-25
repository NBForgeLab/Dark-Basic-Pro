#pragma once

// Common Includes
#include <cstdint>
#include <memory>

// Custom Includes
#include "Statement.h"
#include "InstructionTableEntry.h"

class CParseInstruction  
{
	public:
		CParseInstruction();
		virtual ~CParseInstruction();

		void				SetType(uint32_t dwType) { m_dwInstructionType = dwType; }
		void				SetValue(uint32_t dwValue) { m_dwInstructionValue = dwValue; }
		void				SetParamMax(uint32_t dwMax) { m_dwInstructionParamMax = dwMax; }
		void				SetReturnParameter(CStr* pReturn) { m_pReturnParam.reset(pReturn); }
		void				SetLabelParam(CStr* pParam) { m_pLabelParameter.reset(pParam); }

		void				SetLineNumber(uint32_t line) { m_dwLineNumber = line; }
		uint32_t				GetLineNumber(void) { return m_dwLineNumber; }

		void				SetParameter(CParameter* pParam) { m_pParameter.reset(pParam); }
		CParameter*			GetParameter(void) { return m_pParameter.get(); }
		CStr*				GetReturnParameter(void) { return m_pReturnParam.get(); }
		CStr*				GetLabelParam(void) { return m_pLabelParameter.get(); }

		void				SetInstructionRef(CInstructionTableEntry* pRef) { m_pRefInstructionEntry=pRef; }
		CInstructionTableEntry* GetInstructionRef(void) { return m_pRefInstructionEntry; }

		void				PassStartEndCharForPossibleDebugHook(uint32_t dwS, uint32_t dwE) { m_dwS=dwS; m_dwE=dwE; }

		bool				ActOnSingleVar(CResultData* pVar, uint32_t dwType, int iDisplacement);
		bool				ActOnType(CResultData* pVar, CStr* pTypeName, int iDisplacement);
		bool				ActOnLocalVars(CResultData* pVar);

		bool				WriteDBM(void);
		bool				WriteDBMBit(void);
		bool				WriteDBMHardCode(uint32_t dwBuildID, CResultData* pP1, CResultData* pP2, CResultData* pP3);

	private:

		// Debug Data
		uint32_t						m_dwLineNumber;
		uint32_t						m_dwS;
		uint32_t						m_dwE;
	
		// Instruction Data
		uint32_t						m_dwInstructionType;
		uint32_t						m_dwInstructionValue;
		uint32_t						m_dwInstructionParamMax;
		std::unique_ptr<CParameter>	m_pParameter;
		std::unique_ptr<CStr>		m_pLabelParameter;
		std::unique_ptr<CStr>		m_pReturnParam;

		CInstructionTableEntry*		m_pRefInstructionEntry;
};