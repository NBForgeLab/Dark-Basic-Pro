// StatementList.h: interface for the CStatementList class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STATEMENTLIST_H__44BDB5FD_62E4_4B69_8950_E09A989E3475__INCLUDED_)
#define AFX_STATEMENTLIST_H__44BDB5FD_62E4_4B69_8950_E09A989E3475__INCLUDED_

#include <DB3Array.h>

#include "Statement.h"
#include "Declaration.h"
#include "InstructionTableEntry.h"

class CStatementList  
{
	public:
		CStatementList();
		virtual ~CStatementList();
	
	public:
		bool			UnfoldIncludes(LPSTR* ppData, DWORD* pSize); 
		bool			MakeStatements(LPSTR pData, DWORD Size);
		bool			AddMiniStatements(LPSTR pData, DWORD Size);

		[[nodiscard]] CStatement*		GetPreScanStatements(void) const noexcept { return m_pPreScanStatements; }
		[[nodiscard]] CStatement*		GetProgramStatements(void) const noexcept { return m_pProgramStatements; }
		[[nodiscard]] CStatement*		GetMiniStatements(void) const noexcept { return m_pMiniStatements; }
		void			ResetParserPointers(void);

		void			SetFileDataPointer(LPSTR pData) noexcept { m_pFileDataPointer=pData; }
		void			IncFileDataPointer(void) noexcept { if(m_pFileDataPointer<m_pFileDataEnd) m_pFileDataPointer++; }
		[[nodiscard]] LPSTR			GetFileDataPointer(void) const noexcept { return m_pFileDataPointer; }
		[[nodiscard]] LPSTR			GetFileDataEnd(void) const noexcept { return m_pFileDataEnd; }
		[[nodiscard]] LPSTR			GetFileDataStart(void) const noexcept { return m_pFileData; }

		void			UpdateLineDBMData(CStatement* pStatementAt);
		void			SetLineNumber(DWORD line) noexcept;
		[[nodiscard]] DWORD			GetLineNumber(void) const noexcept { return m_dwLineNumber; }
		void			IncLineNumber(void);

		void			SetLastCharInDataPosition(DWORD dwCharPos) noexcept { m_dwLastCharInDataPosition=dwCharPos; }
		[[nodiscard]] DWORD			GetLastCharInDataPosition(void) const noexcept { return m_dwLastCharInDataPosition; }
		void			SetLastStampedCharInDataPosition(void) noexcept { m_dwLastStampedCharInDataPosition=m_dwLastCharInDataPosition; }
		[[nodiscard]] DWORD			GetLastStampedCharInDataPosition(void) const noexcept { return m_dwLastStampedCharInDataPosition; }
		
		void			SetTokenLineNumber(DWORD line) noexcept { m_dwTokenLineNumber=line; }
		[[nodiscard]] DWORD			GetTokenLineNumber(void) const noexcept { return m_dwTokenLineNumber; }
		void			SetTempVarIndex(DWORD index) noexcept { m_dwTempVariableIndex=index; }
		[[nodiscard]] DWORD			GetTempVarIndex(void) const noexcept { return m_dwTempVariableIndex; }
		void			IncTempVarIndex(void) noexcept { m_dwTempVariableIndex++; }

		void			SetVarOffsetCounter(DWORD value) noexcept { m_dwVarOffsetCounter=value; }
		[[nodiscard]] DWORD			GetVarOffsetCounter(void) const noexcept { return m_dwVarOffsetCounter; }
		void			IncVarOffsetCounter(DWORD size) noexcept { m_dwVarOffsetCounter+=size; }

		void			SetDataIndexCounter(DWORD value) noexcept { m_dwDataIndexCounter=value; }
		[[nodiscard]] DWORD			GetDataIndexCounter(void) const noexcept { return m_dwDataIndexCounter; }
		void			IncDataIndexCounter(DWORD size) noexcept { m_dwDataIndexCounter+=size; }
		
		void			SetStringIndexCounter(DWORD value) noexcept { m_dwStringIndexCounter=value; }
		[[nodiscard]] DWORD			GetStringIndexCounter(void) const noexcept { return m_dwStringIndexCounter; }
		void			IncStringIndexCounter(DWORD size) noexcept { m_dwStringIndexCounter+=size; }

		void			SetDLLIndexCounter(DWORD value) noexcept { m_dwDLLIndexCounter=value; }
		[[nodiscard]] DWORD			GetDLLIndexCounter(void) const noexcept { return m_dwDLLIndexCounter; }
		void			IncDLLIndexCounter(DWORD size) noexcept { m_dwDLLIndexCounter+=size; }

		void			SetCommandIndexCounter(DWORD value) noexcept { m_dwCommandIndexCounter=value; }
		[[nodiscard]] DWORD			GetCommandIndexCounter(void) const noexcept { return m_dwCommandIndexCounter; }
		void			IncCommandIndexCounter(DWORD size) noexcept { m_dwCommandIndexCounter+=size; }

		void			SetLabelIndexCounter(DWORD value) noexcept { m_dwLabelIndexCounter=value; }
		[[nodiscard]] DWORD			GetLabelIndexCounter(void) const noexcept { return m_dwLabelIndexCounter; }
		void			IncLabelIndexCounter(DWORD size) noexcept { m_dwLabelIndexCounter+=size; }

		void			SetVarQtyCounter(DWORD value) noexcept { m_dwVarQtyCounter=value; }
		[[nodiscard]] DWORD			GetVarQtyCounter(void) const noexcept { return m_dwVarQtyCounter; }
		void			IncVarQtyCounter(DWORD size) noexcept { m_dwVarQtyCounter+=size; }
		void			SetLabelQtyCounter(DWORD value) noexcept { m_dwLabelQtyCounter=value; }
		[[nodiscard]] DWORD			GetLabelQtyCounter(void) const noexcept { return m_dwLabelQtyCounter; }
		void			IncLabelQtyCounter(DWORD size) noexcept { m_dwLabelQtyCounter+=size; }

		void			SetUserFunctionName(LPSTR pUFName) { m_pCurrentUserFunctionName.SetText(pUFName); }
		[[nodiscard]] LPSTR			GetUserFunctionName(void) noexcept { return m_pCurrentUserFunctionName.GetStr(); }

		void			SetUserFunctionDecChain(CDeclaration* pDec) noexcept { m_pCurrentUserFunctionDecChain=pDec; }
		[[nodiscard]] CDeclaration*	GetUserFunctionDecChain(void) const noexcept { return m_pCurrentUserFunctionDecChain; }

		void			SetInstructionRef(CInstructionTableEntry* pRef) noexcept { m_pInstructionRef=pRef; }
		[[nodiscard]] CInstructionTableEntry* GetInstructionRef(void) const noexcept { return m_pInstructionRef; }
		void			SetInstructionType(DWORD type) noexcept { m_dwInstructionType=type; }
		[[nodiscard]] DWORD			GetInstructionType(void) const noexcept { return m_dwInstructionType; }
		void			SetInstructionValue(DWORD value) noexcept { m_dwInstructionValue=value; }
		[[nodiscard]] DWORD			GetInstructionValue(void) const noexcept { return m_dwInstructionValue; }
		void			SetInstructionParamMax(DWORD max) noexcept { m_dwInstructionParamMax=max; }
		[[nodiscard]] DWORD			GetInstructionParamMax(void) const noexcept { return m_dwInstructionParamMax; }

		void			SetVariableAddParse(bool bState) noexcept { m_bParseVariableAdds = bState; }
		[[nodiscard]] bool			GetVariableAddParse(void) const noexcept { return m_bParseVariableAdds; }
		void			SetImplementationParse(bool bState) noexcept { m_bParseImplementation = bState; }
		[[nodiscard]] bool			GetImplementationParse(void) const noexcept { return m_bParseImplementation; }
		void			SetDisableParsingToCR(bool bState) noexcept { m_bDisableParsingToCR = bState; }
		[[nodiscard]] bool			GetDisableParsingToCR(void) const noexcept { return m_bDisableParsingToCR; }
		void			SetDisableParsingFull(bool bState) noexcept { m_bDisableParsingFull = bState; }
		[[nodiscard]] bool			GetDisableParsingFull(void) const noexcept { return m_bDisableParsingFull; }
		void			SetAllowLabelAsValue(bool bState) noexcept { m_bPermitLabelAsValue = bState; }
		[[nodiscard]] bool			GetAllowLabelAsValue(void) const noexcept { return m_bPermitLabelAsValue; }
		void			SetLocalVarUsageAsGlobal(bool bState) noexcept { m_bLocalVarUsage = bState; }
		[[nodiscard]] bool			GetLocalVarUsageAsGlobal(void) const noexcept { return m_bLocalVarUsage; }
		
		void			SetLastLine(DWORD line) noexcept { m_dwLastLineNumber=line; }
		[[nodiscard]] DWORD			GetLastLine(void) const noexcept { return m_dwLastLineNumber; }

		void			SetLatestLoopExitLabel(CParameter* pLabel) noexcept { m_pLatestLoopExitLabelRef=pLabel; }
		[[nodiscard]] CParameter*		GetLatestLoopExitLabel(void) const noexcept { return m_pLatestLoopExitLabelRef; }

		bool			FindStartOfFileDataProgramLine(DWORD dwFindLineNumber, LPSTR* pReturnText);
		size_t			GetLineText(DWORD dwLineNumber, char *pDst, size_t DstLen);

		void			SetWriteStarted(bool bState) noexcept { m_bWriteStarted=bState; }
		[[nodiscard]] bool			GetWriteStarted(void) const noexcept { return m_bWriteStarted; }
		[[nodiscard]] CStatement*		GetRefStatement(void) const noexcept { return m_pRefStatementDuringWrite; }
		bool			WriteDBM(void);

	public:
		int				m_iNestCount;

	private:

		// Original DBA FileData
		LPSTR			m_pFileData;
		LPSTR			m_pFileDataEnd;
		LPSTR			m_pFileDataPointer;
		DWORD			m_FileDataSize;

		// Tracking Data
		DWORD			m_dwLastCharInDataPosition;
		DWORD			m_dwLastStampedCharInDataPosition;
		DWORD			m_dwTokenLineNumber;
		DWORD			m_dwLineNumber;
		DWORD			m_dwTempVariableIndex;

		// Variable Creation Tracking
		DWORD			m_dwVarOffsetCounter;
		CStr			m_pCurrentUserFunctionName;
		CDeclaration*	m_pCurrentUserFunctionDecChain;

		// Data/String Creation Tracking
		DWORD			m_dwDataIndexCounter;
		DWORD			m_dwStringIndexCounter;

		// DLL/Command Creation Tracking
		DWORD			m_dwDLLIndexCounter;
		DWORD			m_dwCommandIndexCounter;

		// Label INTERNAL Creation Tracking
		DWORD			m_dwLabelIndexCounter;

		// Label TOTAL Creation Tracking
		DWORD			m_dwLabelQtyCounter;

		// Variable Creation Tracking
		DWORD			m_dwVarQtyCounter;

		// Instruction Workdata
		CInstructionTableEntry* m_pInstructionRef;
		DWORD			m_dwInstructionType;
		DWORD			m_dwInstructionValue;
		DWORD			m_dwInstructionParamMax;

		// Parsing Control
		bool			m_bParseVariableAdds;
		bool			m_bParseImplementation;
		bool			m_bDisableParsingToCR;
		bool			m_bDisableParsingFull;
		bool			m_bPermitLabelAsValue;
		bool			m_bLocalVarUsage;

		CStatement*		m_pRefStatementDuringWrite;
		bool			m_bWriteStarted;

		// Global Used When building Label Byte Offsets (for nonlinenumstatements)
		DWORD			m_dwLastLineNumber;

		// Track Loop Parsing (so EXIT knows the jump-to label)
		CParameter*		m_pLatestLoopExitLabelRef;

		// Start of Program Block
		CStatement*		m_pPreScanStatements;
		CStatement*		m_pProgramStatements;

		// MiniProgram Block
		CStatement*		m_pMiniStatements;

		// Track all lines
		db3::TArray<char *> m_LinePtrs;
};

#endif // !defined(AFX_STATEMENTLIST_H__44BDB5FD_62E4_4B69_8950_E09A989E3475__INCLUDED_)
