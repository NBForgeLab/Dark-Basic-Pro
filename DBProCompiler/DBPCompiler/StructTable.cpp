// StructTable.cpp: implementation of the CStructTable class.
//
//////////////////////////////////////////////////////////////////////

// Common Includes
#include "ParserHeader.h"
#include "Declaration.h"
#include "Statement.h"
#include "StatementList.h"
#include "DBMWriter.h"
#include "Error.h"
#include "Errors.h"
#include "StructTable.h"
#include "StringUtils.h"

// Special access to global pointer to struct table (so can do full scan)
extern CStructTable* g_pStructTable;

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>

std::unordered_map<std::string, CStructTable*> CStructTable::g_Table;
std::vector<CStructTable*> CStructTable::g_Order;

namespace {
static std::string struct_to_lower(std::string_view s)
{
	std::string res(s);
	std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return res;
}
}

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

	m_pDecChain=nullptr;
	m_pDecBlock=nullptr;

	m_orderIndex=static_cast<size_t>(-1);
}

CStructTable::~CStructTable()
{
	SafeDelete(m_pDecChain);
	SafeDelete(m_pDecBlock);
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
	if ( g_Order.empty() )
	{
		delete this;
		return;
	}
	// delete every node tracked in the declaration-order index
	for ( CStructTable* pNode : g_Order )
		delete pNode;
	g_Order.clear();
}

void CStructTable::Add(CStructTable* pNew)
{
	// register the head of the list (this) on first use
	if ( g_Order.empty() )
	{
		g_Order.push_back(this);
		this->m_orderIndex=0;
	}
	g_Order.push_back(pNew);
	pNew->m_orderIndex=g_Order.size()-1;
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

bool CStructTable::SetStruct(DWORD dwValue, std::string_view structName, unsigned char cStructChar, DWORD dwSize)
{
	// Set Struct Data
	SetTypeMode(0);
	SetTypeValue(dwValue);
	SetTypeName(structName);
	SetTypeChar(cStructChar);
	SetTypeSize(dwSize);

#ifdef __AARON_STRUCPERF__
	std::string lowerName = struct_to_lower(structName);
	assert_msg(g_Table.find(lowerName) == g_Table.end() || g_Table[lowerName] == nullptr, "Struct already exists");
	g_Table[lowerName] = this;
#endif

	return true;
}

bool CStructTable::AddStruct(DWORD dwValue, std::string_view structName, unsigned char cStructChar, DWORD dwSize)
{
	// Create structure
	CStructTable* pNewType = new CStructTable;

	// Set Struct Data
	pNewType->SetTypeMode(0);
	pNewType->SetTypeValue(dwValue);
	pNewType->SetTypeName(structName);
	pNewType->SetTypeChar(cStructChar);
	pNewType->SetTypeSize(dwSize);
	pNewType->SetDecChain(nullptr);
	pNewType->SetTypeBlock(nullptr);

#ifdef __AARON_STRUCPERF__
	std::string lowerName = struct_to_lower(structName);
	assert_msg(g_Table.find(lowerName) == g_Table.end() || g_Table[lowerName] == nullptr, "Struct already exists");
	g_Table[lowerName] = pNewType;
#endif

	// Add Struct to list
	Add(pNewType);

	// Complete
	return true;
}

bool CStructTable::AddStructUserType(DWORD dwMode, std::string_view structName, unsigned char cStructChar, CDeclaration* pDecChain, CStatement* pTypeBlock, DWORD dwStructTypeMode, bool* pbReportError, DWORD dwParamInUserFunction )
{
	// Only add if unique
	if(DoesTypeEvenExist(structName)!=nullptr)
		return false;

	// lee - 150206 - u60 - add only if known type at this point (typeA before typeB if typeB uses typeA)
	CDeclaration* pCurrent = pDecChain;
	if ( pCurrent )
	{
		// check all dec types
		while ( pCurrent )
		{
			if ( !pCurrent->GetTypeView().empty() )
			{
				if ( g_pStructTable->DoesTypeEvenExist ( pCurrent->GetTypeView() ) == nullptr )
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
	pNewType->SetTypeMode(dwStructTypeMode);
	pNewType->SetTypeValue(dwMode);
	pNewType->SetTypeName(structName);
	pNewType->SetTypeChar(cStructChar);
	pNewType->SetTypeSize(0);
	pNewType->SetDecChain(pDecChain);
	pNewType->SetTypeBlock(pTypeBlock);

#ifdef __AARON_STRUCPERF__
	std::string lowerName = struct_to_lower(structName);
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

CStructTable* CStructTable::DoesTypeEvenExist(std::string_view name)
{
	if (name.empty())
		return nullptr;

	std::string lowerName = struct_to_lower(name);
	auto it = g_Table.find(lowerName);
	if (it == g_Table.end() || !it->second)
		return nullptr;

	return it->second;
}

DWORD CStructTable::GetSizeOfType(std::string_view name)
{
	if(GetTypeName())
		if(dbp::iequals(name, GetTypeNameView()))
			return GetTypeSize();

	if(GetNext())
		return GetNext()->GetSizeOfType(name);

	return 0;
}

CDeclaration* CStructTable::FindDecInType(std::string_view typenameStr, std::string_view fieldname)
{
	if (typenameStr.empty() || fieldname.empty())
		return nullptr;

	std::string lowerTypeName = struct_to_lower(typenameStr);
	auto it = g_Table.find(lowerTypeName);
	if (it == g_Table.end() || !it->second)
		return nullptr;

	CStructTable *struc = it->second;
	for(CDeclaration *dec = struc->m_pDecChain; dec; dec = dec->GetNext()) {
		if (dbp::iequals(dec->GetNameView(), fieldname))
			return dec;
	}

	return nullptr;
}

CDeclaration* CStructTable::FindFieldInType(std::string_view typenameStr, std::string_view fieldname, LPSTR* pReturnType, DWORD* pdwArrFlag, DWORD* pdwOffset)
{
	CDeclaration* pDec = FindDecInType(typenameStr, fieldname);
	if(pDec)
	{
		// Create string and copy typename of field
		*pReturnType = new char[pDec->GetTypeView().size()+1];
		snprintf(*pReturnType, pDec->GetTypeView().size()+1, "%s", pDec->GetTypeStr());
		*pdwArrFlag=pDec->GetArrFlag();
		*pdwOffset=pDec->GetOffset();
		return pDec;
	}

	// Not found soft fail
	return nullptr;
}

bool CStructTable::FindOffsetFromField(std::string_view typenameStr, std::string_view fieldname, DWORD* pReturnOffset, DWORD* pdwSizeData)
{
	CDeclaration* pDec = FindDecInType(typenameStr, fieldname);
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

int CStructTable::FindIndex(std::string_view typenameStr)
{
	int iIndex = 0;
	CStructTable* pCurrent = this;
	while(pCurrent)
	{
		// if find type, exit now to retain iIndex
		if ( dbp::iequals( pCurrent->GetTypeNameView(), typenameStr ) )
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
	if (!g_pDBMWriter->OutputDBM("")) return false;

	// Title
	if (!g_pDBMWriter->OutputDBM("DEBUG:")) return false;

	return true;
}

bool CStructTable::WriteDBM(void)
{
	// Write out text
	std::string dbmLine;

	// Structure Type
	if (GetTypeMode() == 0)
	{
		dbmLine = "STRUCT@";
	}
	else if (GetTypeMode() == 1)
	{
		dbmLine = "USERSTRUCT@";
	}
	else if (GetTypeMode() == 2)
	{
		dbmLine = "FS@";
	}

	if (m_pTypeName)
		dbmLine += m_pTypeName->View();
	dbmLine += " Overall Size:";
	dbmLine += std::to_string(m_dwSize);

	// U73 - 230309 - added for Diggory Debugger
	if (GetTypeMode() == 2)
	{
		dbmLine += " Parameter Count:";
		dbmLine += std::to_string(m_dwParamInUserFunction);
	}

	// User Types has a blank to make reading easier
	if (m_pDecChain)
	{
		if (!g_pDBMWriter->OutputDBM("")) return false;
	}

	// Output type details
	if (!g_pDBMWriter->OutputDBM(dbmLine)) return false;

	// Write out fields of type
	if (m_pDecChain)
	{
		if (!m_pDecChain->WriteDBM()) return false;
	}

	// Write next one
	if (GetNext())
	{
		if (!GetNext()->WriteDBM()) return false;
	}

	// Complete
	return true;
}
