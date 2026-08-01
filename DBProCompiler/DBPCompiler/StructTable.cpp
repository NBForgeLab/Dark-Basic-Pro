// StructTable.cpp: implementation of the CStructTable class.
//
//////////////////////////////////////////////////////////////////////

// Common Includes
#define _CRT_SECURE_NO_DEPRECATE
#pragma warning(disable : 4996)
#include "StructTable.h"

// Special access to global pointer to struct table (so can do full scan)
extern CStructTable* g_pStructTable;

#ifdef __AARON_STRUCPERF__
#include <algorithm>
#include <string>
#include <unordered_map>

std::unordered_map<std::string, CStructTable*> CStructTable::g_Table;

namespace {
static std::string struct_to_lower(const std::string& s)
{
	std::string res = s;
	std::transform(res.begin(), res.end(), res.begin(), ::tolower);
	return res;
}
}
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CStructTable::CStructTable()
{
	m_dwTypeMode=0;
	m_dwTypeValue=0;
	m_pTypeName=nullptr;
	m_cTypeChar=0;
	m_dwSize=0;
	m_dwTargetAddressSize =
		static_cast<DWORD>(dbp::abi::ActiveTargetAbi::address_size);

	m_pDecChain=NULL;
	m_pDecBlock=NULL;

	m_pNext=NULL;
}

CStructTable::~CStructTable()
{
	SAFE_DELETE(m_pDecChain);
	SAFE_DELETE(m_pDecBlock);
#ifdef __AARON_STRUCPERF__
	if (m_pTypeName)
	{
		std::string lowerStr = struct_to_lower(m_pTypeName->GetStr());
		auto it = g_Table.find(lowerStr);
		if (it != g_Table.end() && it->second == this)
		{
			g_Table.erase(it);
		}
	}
#endif
}

void CStructTable::Free(void)
{
#ifdef __AARON_STRUCPERF__
	g_Table.clear();
#endif
	CStructTable* pCurrent = this;
	while(pCurrent)
	{
		CStructTable* pNext = pCurrent->GetNext();
		delete pCurrent;
		pCurrent = pNext;
	}
}

void CStructTable::Add(CStructTable* pNew)
{
	CStructTable* pCurrent = this;
	while(pCurrent->m_pNext)
	{
		pCurrent=pCurrent->GetNext();
	}
	pCurrent->m_pNext=pNew;
}

void CStructTable::SetStructDefaults(void)
{
	SetStructDefaultsFor<dbp::abi::ActiveTargetAbi>();
}

void CStructTable::SetStructDefaults(DWORD dwTargetAddressSize)
{
	m_dwTargetAddressSize = dwTargetAddressSize;

	// Default datatypes and sizes
	SetStruct(1, "integer",					'L', 4);
	AddStruct(2, "float",					'F', 4);
	AddStruct(3, "string",					'S', dwTargetAddressSize);
	AddStruct(4, "boolean",					'B', 1);
	AddStruct(5, "byte",					'Y', 1);
	AddStruct(6, "word",					'W', 2);
	AddStruct(7, "dword",					'D', 4);
	AddStruct(8, "double float",			'O', 8);
	AddStruct(9, "double integer",			'R', 8);
	AddStruct(10,"label",					'P', 4);
	AddStruct(20,"dabel",					'Q', 4);
	AddStruct(101, "integer array",			'm', dwTargetAddressSize);
	AddStruct(102, "float array",			'g', dwTargetAddressSize);
	AddStruct(103, "string array",			't', dwTargetAddressSize);
	AddStruct(104, "boolean array",			'c', dwTargetAddressSize);
	AddStruct(105, "byte array",			'z', dwTargetAddressSize);
	AddStruct(106, "word array",			'x', dwTargetAddressSize);
	AddStruct(107, "dword array",			'e', dwTargetAddressSize);
	AddStruct(108, "double float array",	'u', dwTargetAddressSize);
	AddStruct(109, "double integer array",	'v', dwTargetAddressSize);
	AddStruct(501, "anytype non casted",	'X', 4);
	AddStruct(1001, "userdefined var ptr",	'E', dwTargetAddressSize);
	AddStruct(1101, "userdefined array ptr",'e', dwTargetAddressSize);
}

bool CStructTable::SetStruct(DWORD dwValue, LPSTR pStructName, unsigned char cStructChar, DWORD dwSize)
{
	// Set Struct Data
	CStr* pStrTypeName = new CStr(pStructName);
	SetTypeMode(0);
	SetTypeValue(dwValue);
	SetTypeName(pStrTypeName);
	SetTypeChar(cStructChar);
	SetTypeSize(dwSize);

#ifdef __AARON_STRUCPERF__
	std::string lowerName = struct_to_lower(pStructName);
	assert_msg(g_Table.find(lowerName) == g_Table.end() || g_Table[lowerName] == nullptr, "Struct already exists");
	g_Table[lowerName] = this;
#endif

	return true;
}

bool CStructTable::AddStruct(DWORD dwValue, LPSTR pStructName, unsigned char cStructChar, DWORD dwSize)
{
	// Create structure
	CStructTable* pNewType = new CStructTable;

	// Set Struct Data
	CStr* pStrTypeName = new CStr(pStructName);
	pNewType->SetTypeMode(0);
	pNewType->SetTypeValue(dwValue);
	pNewType->SetTypeName(pStrTypeName);
	pNewType->SetTypeChar(cStructChar);
	pNewType->SetTypeSize(dwSize);
	pNewType->SetDecChain(NULL);
	pNewType->SetTypeBlock(NULL);

#ifdef __AARON_STRUCPERF__
	std::string lowerName = struct_to_lower(pStructName);
	assert_msg(g_Table.find(lowerName) == g_Table.end() || g_Table[lowerName] == nullptr, "Struct already exists");
	g_Table[lowerName] = pNewType;
#endif

	// Add Struct to list
	Add(pNewType);

	// Complete
	return true;
}

bool CStructTable::AddStructUserType(DWORD dwMode, LPSTR pStructName, unsigned char cStructChar, CDeclaration* pDecChain, CStatement* pTypeBlock, DWORD dwStructTypeMode, bool* pbReportError, DWORD dwParamInUserFunction )
{
	// Only add if unique
	if(DoesTypeEvenExist(pStructName)!=NULL)
		return false;

	// lee - 150206 - u60 - add only if known type at this point (typeA before typeB if typeB uses typeA)
	CDeclaration* pCurrent = pDecChain;
	if ( pCurrent )
	{
		// check all dec types
		while ( pCurrent )
		{
			if ( strcmp ( pCurrent->GetType()->GetStr(), "" )!=NULL )
			{
				if ( g_pStructTable->DoesTypeEvenExist ( pCurrent->GetType()->GetStr() )==false )
				{
					// fail here as type is unknown and so cannot create a struct based on an unknown type
					if ( pbReportError ) *pbReportError = true;
					return false;
				}
			}
			pCurrent = pCurrent->GetNext();
		}
	}

	// Create structure
	CStructTable* pNewType = new CStructTable;

	// When get a zero mode
	if(dwMode==0)
	{
		// Generate unique type value for it (starting at 1001...)
		dwMode=1001;
	}

	// Set Struct Data
	CStr* pStrTypeName = new CStr(pStructName);
	pNewType->SetTypeMode(dwStructTypeMode);
	pNewType->SetTypeValue(dwMode);
	pNewType->SetTypeName(pStrTypeName);
	pNewType->SetTypeChar(cStructChar);
	pNewType->SetTypeSize(0);
	pNewType->SetDecChain(pDecChain);
	pNewType->SetTypeBlock(pTypeBlock);

#ifdef __AARON_STRUCPERF__
	std::string lowerName = struct_to_lower(pStructName);
	assert_msg(g_Table.find(lowerName) == g_Table.end() || g_Table[lowerName] == nullptr, "Struct already exists");
	g_Table[lowerName] = pNewType;
#endif

	// U73 - 230309 - added param count for Diggory (new debugger)
	pNewType->SetParamInUserFunction ( dwParamInUserFunction );

	// Add Struct to list
	Add(pNewType);

	// Complete
	return true;	
}

bool CStructTable::AddStructUserType(DWORD dwMode, LPSTR pStructName, unsigned char cStructChar, CDeclaration* pDecChain, CStatement* pTypeBlock, DWORD dwStructTypeMode, bool* pbReportError )
{
	return AddStructUserType( dwMode, pStructName, cStructChar, pDecChain, pTypeBlock, dwStructTypeMode, pbReportError, 0 );
}

bool CStructTable::AddStructUserType(DWORD dwMode, LPSTR pStructName, unsigned char cStructChar, CDeclaration* pDecChain, CStatement* pTypeBlock, DWORD dwStructTypeMode )
{
	return AddStructUserType( dwMode, pStructName, cStructChar, pDecChain, pTypeBlock, dwStructTypeMode, NULL );
}

bool CStructTable::CalculateAllSizes(void)
{
	DWORD dwAttempts=0;
	DWORD dwStructuresMax=0;
	bool bKeepGoing=true;
	while(bKeepGoing)
	{
		bKeepGoing=false;
		DWORD dwCountStructures=0;
		CStructTable* pCurrent = this;
		while(pCurrent)
		{
			dwCountStructures++;
			if(dwCountStructures>dwStructuresMax)
				dwStructuresMax=dwCountStructures;

			if(pCurrent->CalculateSize()==false)
				bKeepGoing=true;

			pCurrent = pCurrent->GetNext();
		}

		// Drop out with error if cannot resolve type sizes
		if(dwAttempts>dwStructuresMax)
		{
			g_pErrorReport->AddErrorString("Failed to 'CalculateAllSizes::dwAttempts>dwStructuresMax'");
			return false;
		}

		dwAttempts++;
	}

	// Complete
	return true;
}

bool CStructTable::CalculateSize(void)
{
	// Calculate sizes of dec chain if have one
	if(m_pDecChain)
	{
		bool bSecondPass = true;
		while ( bSecondPass )
		{
			// do first pass
			bSecondPass = false;

			// calculate size
			DWORD dwCumilitiveSize=0;
			CDeclaration* pDec = m_pDecChain;
			while(pDec)
			{
				if(pDec->GetName())
				{
					if(g_pStructTable->DoesTypeEvenExist(pDec->GetType()->GetStr()))
					{
						//leefix - 300305 - bytes must be stored in DWORDS (as they are passed onto stack as such)
						DWORD dwSize=g_pStructTable->GetSizeOfType(pDec->GetType()->GetStr());
						if ( dwSize <4 ) dwSize=4;

						if(pDec->GetArrFlag()==1) dwSize=4;
						if(dwSize>0)
						{
							// Calculate full size of field using array value
							DWORD dwArraySize = (DWORD)pDec->GetArrValue()->GetValue();
							if(dwArraySize>0)
							{
								// Array determines final field size
								dwSize*=dwArraySize;
							}

							// Set Offset within declaration for struct-field-offset reference
							pDec->SetOffset(dwCumilitiveSize);
							pDec->SetDataSize(dwSize);
							dwCumilitiveSize+=dwSize;
						}
						else
						{
							g_pErrorReport->AddErrorString("Failed to 'CalculateSize::dwSize>0'");
							return false;
						}
					}
					else
					{
						DWORD LineNumber = pDec->GetLineNumber();
						g_pErrorReport->SetError(LineNumber, ERR_SYNTAX+35, pDec->GetType()->GetStr());
						return false;
					}
				}
				pDec = pDec->GetNext();
			}

			// leefix - 220604 - u54 - If not on DWORD boundary
			int iRemainder = dwCumilitiveSize % 4;
			if ( iRemainder > 0 )
			{
				// NOT DWORD boundary - add fillers
				iRemainder = 4-iRemainder;
				for ( int i=0; i<iRemainder; i++ )
				{
					CDeclaration* pNewDec = new CDeclaration;
					pNewDec->SetDecData(0, "", "___filler", "byte", "", 0);
					m_pDecChain->Add ( pNewDec );
				}

				// do again
				bSecondPass = true;
			}
			else
			{
				// Assign total to size member of 
				SetTypeSize(dwCumilitiveSize);
			}
		}
	}

	// Complete
	return true;
}

CStructTable* CStructTable::DoesTypeEvenExist(LPSTR pName)
{
#ifdef __AARON_STRUCPERF__
	std::string lowerName = struct_to_lower(pName);
	auto it = g_Table.find(lowerName);
	if (it == g_Table.end() || !it->second)
		return NULL;

	return it->second;
#else
	if(GetTypeName())
		if(stricmp(pName, GetTypeName()->GetStr())==NULL)
			return this;

	if(GetNext())
		return GetNext()->DoesTypeEvenExist(pName);

	return NULL;
#endif
}

DWORD CStructTable::GetSizeOfType(LPSTR pName)
{
	if(GetTypeName())
		if(stricmp(pName, GetTypeName()->GetStr())==NULL)
			return GetTypeSize();

	if(GetNext())
		return GetNext()->GetSizeOfType(pName);

	return 0;
}

CDeclaration* CStructTable::FindDecInType(LPSTR pTypename, LPSTR pFieldname)
{
#ifdef __AARON_STRUCPERF__
	CStructTable *struc;
	CDeclaration *dec;

	std::string lowerTypeName = struct_to_lower(pTypename);
	auto it = g_Table.find(lowerTypeName);
	if (it == g_Table.end() || !it->second)
		return NULL;

	struc = it->second;
	for(dec=struc->m_pDecChain; dec; dec=dec->GetNext()) {
		if (stricmp(dec->GetName()->GetStr(), pFieldname)==0)
			break;
	}

	return dec;
#else
	if(stricmp(GetTypeName()->GetStr(), pTypename)==NULL)
	{
		if(m_pDecChain)
		{
			CDeclaration* pDec = m_pDecChain;
			while(pDec)
			{
				if(stricmp(pDec->GetName()->GetStr(), pFieldname)==NULL)
				{
					return pDec;
				}
				pDec = pDec->GetNext();
			}
		}
		return NULL;
	}
	
	if(GetNext())
		return GetNext()->FindDecInType(pTypename, pFieldname);
	else
		return NULL;
#endif
}

CDeclaration* CStructTable::FindFieldInType(LPSTR pTypename, LPSTR pFieldname, LPSTR* pReturnType, DWORD* pdwArrFlag, DWORD* pdwOffset)
{
	CDeclaration* pDec = FindDecInType(pTypename, pFieldname);
	if(pDec)
	{
		// Create string and copy typename of field
		*pReturnType = new char[pDec->GetType()->Length()+1];
		strcpy(*pReturnType, pDec->GetType()->GetStr());
		*pdwArrFlag=pDec->GetArrFlag();
		*pdwOffset=pDec->GetOffset();
		return pDec;
	}

	// Not found soft fail
	return NULL;
}

bool CStructTable::FindOffsetFromField(LPSTR pTypename, LPSTR pFieldname, DWORD* pReturnOffset, DWORD* pdwSizeData)
{
	CDeclaration* pDec = FindDecInType(pTypename, pFieldname);
	if(pDec)
	{
		// Extract offset data from dec in type struct
		*pReturnOffset = pDec->GetOffset();
		*pdwSizeData = pDec->GetDataSize();

		return true;
	}
	
	// Not found soft fail
	return false;
}

int CStructTable::FindIndex(LPSTR pTypename)
{
	int iIndex = 0;
	CStructTable* pCurrent = this;
	while(pCurrent)
	{
		// if find type, exit now to retain iIndex
		if ( stricmp ( pCurrent->GetTypeName()->GetStr(), pTypename )==NULL )
			break;

		// next structure
		pCurrent = pCurrent->GetNext();
		iIndex++;
	}

	// return found index
	return iIndex;
}

bool CStructTable::WriteDBMHeader(void)
{
	// Blank Line
	CStr strDBMBlank(1);
	if(g_pDBMWriter->OutputDBM(&strDBMBlank)==false) return false;

	// Title
	CStr strDBMLine(256);
	strDBMLine.SetText("DEBUG:");
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	return true;
}

bool CStructTable::WriteDBM(void)
{
	// Write out text
	CStr strDBMLine(256);

	// Structure Type
	if(GetTypeMode()==0)
	{
		strDBMLine.SetText("STRUCT@");
	}
	if(GetTypeMode()==1)
	{
		strDBMLine.SetText("USERSTRUCT@");
	}
	if(GetTypeMode()==2)
	{
		strDBMLine.SetText("FS@");
	}

	strDBMLine.AddText(m_pTypeName.get());
	strDBMLine.AddText(" Overall Size:");
	strDBMLine.AddNumericText(m_dwSize);

	// U73 - 230309 - added for Diggory Debugger
	if(GetTypeMode()==2)
	{
		strDBMLine.AddText(" Parameter Count:");
		strDBMLine.AddNumericText(m_dwParamInUserFunction);
	}

	// User Types has a blank to make reading easier
	if(m_pDecChain)
	{
		CStr strDBMBlank(1);
		if(g_pDBMWriter->OutputDBM(&strDBMBlank)==false) return false;
	}

	// Output type details
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// Write out fields of type
	if(m_pDecChain)
	{
		if(m_pDecChain->WriteDBM()==false) return false;
	}

	// Write next one
	if(GetNext())
	{
		if((GetNext()->WriteDBM())==false) return false;
	}

	// Complete
	return true;
}
