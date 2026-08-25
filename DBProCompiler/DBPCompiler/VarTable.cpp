// VarTable.cpp: implementation of the CVarTable class.
//
//////////////////////////////////////////////////////////////////////

// Custom Includes
#include "ParserHeader.h"
#include "StatementList.h"
#include "StringUtils.h"
#include "StructTable.h"
#include "VarTable.h"
#include "DBMWriter.h"
#include "Error.h"
#include "Errors.h"
#include "DBPLogger.h"
#include "time.h"

// Special access to global pointer to struct table (so can do full scan)
extern CStructTable* g_pStructTable;
extern CStatementList* g_pStatementList;

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>

std::unordered_map<std::string, CVarTable*> CVarTable::g_Table;
std::vector<CVarTable*> CVarTable::g_Order;

namespace {
static std::string var_to_lower(std::string_view s)
{
	std::string res(s);
	std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return res;
}

inline std::string MakeIntVarName(std::string_view scope, std::string_view name)
{
	if (scope.empty())
		return std::string(name);

	std::string buf;
	buf.reserve(scope.size() + 1 + name.size());
	buf.append(scope);
	buf.push_back(':');
	buf.append(name);
	return buf;
}
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CVarTable::CVarTable()
{
	m_dwLineNumber=0;

	m_pVarScope=nullptr;
	m_pVarName=nullptr;
	m_pVarType=nullptr;
	m_dwVarTypeValue=0;
	m_dwArrFlag=0;
	m_dwFinalDBMOffset=0;

	m_bOffsetAssigned=false;
	m_pAdditionalDataString=nullptr;

	m_orderIndex=static_cast<size_t>(-1);
}

CVarTable::CVarTable(const char* pStr)
{
	m_dwLineNumber=0;

	m_pVarScope = std::make_unique<CStr>("");
	m_pVarName = std::make_unique<CStr>(pStr ? pStr : "");
	m_pVarType = std::make_unique<CStr>("dword");
	m_dwVarTypeValue=7;
	m_dwArrFlag=0;
	m_dwFinalDBMOffset=0;

	m_bOffsetAssigned=false;
	m_pAdditionalDataString=nullptr;

	m_orderIndex=static_cast<size_t>(-1);

#ifdef __AARON_VARTABLEPERF__
	std::string lowerStr = var_to_lower(pStr ? pStr : "");
	assert_msg(g_Table.find(lowerStr) == g_Table.end() || g_Table[lowerStr] == nullptr, "Variable already exists");
	g_Table[lowerStr] = this;
#endif
}

CVarTable::CVarTable(std::string_view str)
{
	m_dwLineNumber=0;

	m_pVarScope = std::make_unique<CStr>("");
	m_pVarName = std::make_unique<CStr>(str);
	m_pVarType = std::make_unique<CStr>("dword");
	m_dwVarTypeValue=7;
	m_dwArrFlag=0;
	m_dwFinalDBMOffset=0;

	m_bOffsetAssigned=false;
	m_pAdditionalDataString=nullptr;

	m_orderIndex=static_cast<size_t>(-1);

#ifdef __AARON_VARTABLEPERF__
	std::string lowerStr = var_to_lower(str);
	assert_msg(g_Table.find(lowerStr) == g_Table.end() || g_Table[lowerStr] == nullptr, "Variable already exists");
	g_Table[lowerStr] = this;
#endif
}

CVarTable::~CVarTable()
{
#ifdef __AARON_VARTABLEPERF__
	if (m_pVarName)
	{
		std::string lowerStr = var_to_lower(m_pVarName->GetStr());
		auto it = g_Table.find(lowerStr);
		if (it != g_Table.end() && it->second == this)
		{
			g_Table.erase(it);
		}
	}
#endif
}

void CVarTable::Free(void)
{
#ifdef __AARON_VARTABLEPERF__
	g_Table.clear();
#endif
	if ( g_Order.empty() )
	{
		delete this;
		return;
	}
	// delete every node tracked in the declaration-order index
	for ( CVarTable* pNode : g_Order )
		delete pNode;
	g_Order.clear();
}

void CVarTable::Add(CVarTable* pNew)
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

void CVarTable::Insert(CVarTable* pNew)
{
	// register the head of the list (this) on first use
	if ( g_Order.empty() )
	{
		g_Order.push_back(this);
		this->m_orderIndex=0;
	}
	// defensive: an unregistered node must be the head of the list
	if ( this->m_orderIndex==static_cast<size_t>(-1) )
	{
		g_Order.insert(g_Order.begin(), this);
		this->m_orderIndex=0;
	}
	// insert pNew immediately before this node
	g_Order.insert( g_Order.begin() + static_cast<std::ptrdiff_t>(this->m_orderIndex), pNew );
	// refresh indices from the insertion point onwards
	for ( size_t i=this->m_orderIndex; i<g_Order.size(); i++ )
		g_Order[i]->m_orderIndex=i;
}

void CVarTable::SetVarDefaults(void)
{
	// One _RSP_ var in table
	DWORD dwAddDefaultVars=1;

	// Used To Hold Pointer to Runtime Error DWORD (filled by DLLs during RT-error)
	Add(new CVarTable("$_ERR_")); dwAddDefaultVars++;
	Add(new CVarTable("$_ESC_")); dwAddDefaultVars++;
	Add(new CVarTable("$_REK_")); dwAddDefaultVars++;
	Add(new CVarTable("$_SLN_")); dwAddDefaultVars++;
	
	// Used To Hold Float Immediates (bound to/from ST0)
	Add(new CVarTable("$_TEMPA_")); dwAddDefaultVars++;
	Add(new CVarTable("$_TEMPB_")); dwAddDefaultVars++;

	// Update Variable Counter
	g_pStatementList->IncVarQtyCounter(dwAddDefaultVars);
}

void CVarTable::AddInOrder(std::string_view name, CVarTable* pNew)
{
	// Find place to insert new variable
	CVarTable* pLocation = this->GetNext();
	while(pLocation)
	{
		if(dbp::icompare(name, pLocation->GetVarNameView()) < 0) break;
		pLocation=pLocation->GetNext();
	}
	if(pLocation)
	{
		// Insert before this location
		pLocation->Insert(pNew);
	}
	else
	{
		// Add to end of list
		Add(pNew);
	}
}

CVarTable* CVarTable::Advance(DWORD dwCountdown)
{
	if ( dwCountdown==0 || m_orderIndex==static_cast<size_t>(-1) )
		return this;
	if ( m_orderIndex + dwCountdown >= g_Order.size() )
		return nullptr;
	return g_Order[m_orderIndex + dwCountdown];
}

CVarTable* CVarTable::Subtract(DWORD dwCountdown)
{
	if ( dwCountdown==0 || m_orderIndex==static_cast<size_t>(-1) )
		return this;
	if ( m_orderIndex < dwCountdown )
		return nullptr;
	return g_Order[m_orderIndex - dwCountdown];
}

bool CVarTable::AddVariable(std::string_view name, std::string_view type, DWORD dwArrFlag, DWORD dwLineNumber, bool bFromActualCodeNotFromTypeDefing, DWORD* pdwAction, bool bIsGlobal)
{
	DBP_TRACE("Registering variable: name={}, type={}, isGlobal={}", name, type, bIsGlobal);

	// Ignore if no variables addable
	if(g_pStatementList->GetVariableAddParse()==false)
		return true;

	// Do not attempt to add system vars
	if(name.size() >= 2 && name[0]=='$' && name[1]=='$')
		return true;

	// LOCAL Temporary variables must not be confused with GLOBAL ones
	bool bVarIsTemporaryVariable=false;
	if(!name.empty() && name[0]=='$') bVarIsTemporaryVariable=true;

	// If defining var in userfunction, must be added to local dec chain
	std::string_view scope;
	LPSTR pUserFunc = g_pStatementList->GetUserFunctionName();
	if(pUserFunc && !dbp::iequals(pUserFunc,""))
		scope = pUserFunc;

	// leefix - 210703 - added for global var specified in ENDFUNCTION param
	// This flag is set when a variable being scanned for checks globally FIRST!
	bool bCheckGloballyFirst = g_pStatementList->GetLocalVarUsageAsGlobal();

	// Check if variable in local scope
	bool bVariableHasBeenAdded=false;
	CVarTable* pFoundVar = FindVariable(scope, name, dwArrFlag);
	if(pFoundVar)
	{
		// If variable created during a pre-scan, accept as added
		if(pFoundVar->GetPreScanAddFlag()==true)
		{
			// Variable re-use is fine - but only once more!
			bVariableHasBeenAdded=true;
			SetPreScanAddFlag(false);
		}
		else
		{
			if(bFromActualCodeNotFromTypeDefing==true)
			{
				// Variables used mid-code can be added many times (only one counts though)
				// Action: Variable already exists as local variable
				if(pdwAction) *pdwAction=3;
				return true;
			}
			else
			{
				// Cannot duplicate a variable declaration by assignment a different type
				if(dbp::iequals(pFoundVar->GetVarTypeView(), type))
				{
					// Same type, so treat as a re-init of variable
					if(pdwAction) *pdwAction=3;
					return true;
				}
				else
				{
					g_pErrorReport->SetError(dwLineNumber, ERR_SYNTAX+36, pFoundVar->GetVarTypeStr());
					return false;
				}
			}
		}
	}

	// leefix - 290703 - for backwards compatibility, prescan : arrays declared in a function and is global should be global (and not explicitly local)
	if ( !scope.empty() && dwArrFlag==1 && g_pStatementList->GetImplementationParse()==false && bIsGlobal==true )
		bCheckGloballyFirst=true;

	// Skip global var check if userfunction is declaring its local variables...
	if(!scope.empty()
	&& ( g_pStatementList->GetImplementationParse()==true || bCheckGloballyFirst==true ) )
	{
		if ( pFoundVar==nullptr )
		{
			// Ensure it does not match a global variable
			if(pdwAction) *pdwAction=0;
			CVarTable* pEntry = FindVariable(std::string_view{}, name, dwArrFlag);
			if(pEntry)
			{
				// Only for non-temp vars
				if(bVarIsTemporaryVariable==false)
				{
					// Only if user specified GLOBAL (or is global by default) for this variable
					if(pEntry->GetSpecifiedAsGlobalFlag()==true)
					{
						// Yes global variable, so use global
						// Action: Variable already exists as global variable
						if(pdwAction) *pdwAction=2;
						return true;
					}
				}
			}
		}
	}

	// If defining var in userfunction, must be added to local dec chain
	CDeclaration* pGlobalDecChain = g_pStatementList->GetUserFunctionDecChain();
	if(pGlobalDecChain)
	{
		// LEEFIX - 230603 - u54 - ONLY IF GLOBAL NOT EXPLICITLY REQUIRED
		if ( bIsGlobal==false )
		{
			// Create Declaration for variable
			std::string nameStr(name);
			std::string typeStr(type);
			CDeclaration* pNewDec = new CDeclaration;
			pNewDec->SetDecData(dwArrFlag, "", nameStr.c_str(), typeStr.c_str(), "", dwLineNumber);

			// Add to chain USERFUNCTION LOCAL (if unique)
			if(pGlobalDecChain->Find(name, dwArrFlag)==nullptr)
				pGlobalDecChain->Add(pNewDec);
			else
				delete pNewDec;
		}
	}

	// If added, need go no further
	if(bVariableHasBeenAdded==true)
	{
		if(pdwAction) *pdwAction=3;
		return true;
	}

	std::string scopeStr;
	if (!scope.empty())
	{
		if ((dwArrFlag==0 && bIsGlobal==true)
		|| (dwArrFlag==1 && bIsGlobal==true))
		{
			scopeStr = "";
		}
		else
		{
			scopeStr = std::string(scope);
		}
	}

	if(type.empty())
	{
		// Cannot duplicate a variable declaration
		g_pErrorReport->AddErrorString("Failed to 'AddVariable::pType==NULL'");
		return false;
	}

	// lee - 110406 - u6rc7 - if array name, skip the &
	std::string nameStr(name);
	if ( !nameStr.empty() )
	{
		if ( nameStr[0]=='&' )
		{
			// Before confirm, check if variable name is a reserved word
			if ( g_pStatementList->GetProgramStatements()->DetermineIfReservedWord ( nameStr.c_str()+1 ) )
			{
				g_pErrorReport->SetError(g_pStatementList->GetLineNumber()-1, ERR_SYNTAX+7, nameStr.c_str()+1);
				return false;
			}

			// before confirm, check if variable name is a function name
			if ( g_pStatementList->GetProgramStatements()->DetermineIfFunctionName ( nameStr.c_str()+1, true ) )
			{
				g_pErrorReport->SetError(g_pStatementList->GetLineNumber()-1, ERR_SYNTAX+7, nameStr.c_str()+1);
				return false;
			}
		}
		else
		{
			// Before confirm, check if variable name is a reserved word
			if ( g_pStatementList->GetProgramStatements()->DetermineIfReservedWord ( nameStr.c_str() ) )
			{
				g_pErrorReport->SetError(g_pStatementList->GetLineNumber(), ERR_SYNTAX+7, nameStr.c_str());
				return false;
			}

			// leefix-040803-Before confirm, check if variable name is a function name
			if ( g_pStatementList->GetProgramStatements()->DetermineIfFunctionName ( nameStr.c_str(), true ) )
			{
				g_pErrorReport->SetError(g_pStatementList->GetLineNumber(), ERR_SYNTAX+7, nameStr.c_str());
				return false;
			}
		}
	}

	// Create new variable
	CVarTable* pNewVar = new CVarTable;
	pNewVar->SetVarScope(scopeStr);
	pNewVar->SetVarName(name);
	pNewVar->SetVarType(type);
	pNewVar->SetVarTypeValue(GetBasicTypeValue(type));
	pNewVar->SetVarStruct(g_pStructTable->DoesTypeEvenExist(pNewVar->GetVarTypeStr()));
	pNewVar->SetArrFlag(dwArrFlag);
	pNewVar->SetLineNumber(dwLineNumber);
	pNewVar->SetSpecifiedAsGlobalFlag(bIsGlobal);

	// leefix - 170303 - Ensure arrays declared outside of scope are also forced as GLOBAL
	if ( dwArrFlag==1 )
	{
		if ( scopeStr.empty() )
			pNewVar->SetSpecifiedAsGlobalFlag(true);
	}

	// Set prescan add flag if defined during pre-scam (locals in func required)
	if(g_pStatementList->GetImplementationParse()==false)
		pNewVar->SetPreScanAddFlag(true);
	else
		pNewVar->SetPreScanAddFlag(false);

	// Add Variable to Variables Table
	AddInOrder(pNewVar->GetVarNameStr(), pNewVar);

#ifdef __AARON_VARTABLEPERF__
	std::string pIntVarName = MakeIntVarName(scopeStr, name);
	std::string lowerIntVarName = var_to_lower(pIntVarName);
	assert_msg(g_Table.find(lowerIntVarName) == g_Table.end() || g_Table[lowerIntVarName] == nullptr, "Variable already exists");
	g_Table[lowerIntVarName] = pNewVar;
#endif

	// Increment var qty index counter
	g_pStatementList->IncVarQtyCounter(1);

	// Action: Added for first time
	if(pdwAction) *pdwAction=1;

	// Complete
	return true;
}

CVarTable* CVarTable::FindVariable(std::string_view scope, std::string_view name, DWORD dwArrFlag)
{
	if (name.empty())
		return nullptr;

	std::string pIntName = MakeIntVarName(scope, name);
	std::string lowerIntName = var_to_lower(pIntName);
	auto it = g_Table.find(lowerIntName);
	if (it == g_Table.end() || !it->second)
		return nullptr;

	if (it->second->GetArrFlag() != dwArrFlag)
		return nullptr;

	return it->second;
}

bool CVarTable::FindVariableExist(std::string_view findVar, DWORD dwArrFlag)
{
	LPCSTR pScope=nullptr;
	if(g_pStatementList->GetUserFunctionDecChain())
		if(g_pStatementList->GetUserFunctionName())
			pScope = g_pStatementList->GetUserFunctionName();

	CVarTable* pFoundVar = FindVariable(pScope ? std::string_view(pScope) : std::string_view{}, findVar, dwArrFlag);
	if(pFoundVar==nullptr)
	{
		pFoundVar = FindVariable(std::string_view{}, findVar, dwArrFlag);
	}

	return pFoundVar != nullptr;
}

bool CVarTable::FindTypeOfVariable(std::string_view findVar, DWORD dwArrType, LPSTR* pReturnType)
{
	// Ensure pScope is observed
	LPCSTR pScope=nullptr;
	if(g_pStatementList->GetUserFunctionDecChain())
		if(g_pStatementList->GetUserFunctionName())
			pScope = g_pStatementList->GetUserFunctionName();

	// Soft-fail on empty name
	if(findVar.empty())
		return false;

	// Scan list and match variable name (guard: CStr::operator LPSTR may yield
	// nullptr for an empty user-function name even when the CStr itself is valid)
	CVarTable* pFoundVar = FindVariable(
		pScope ? std::string_view(pScope) : std::string_view{},
		findVar,
		dwArrType);
	if(pFoundVar==nullptr)
	{
		// Try as global
		pFoundVar = FindVariable("", findVar, dwArrType);
	}

	// Get Type Data
	if(pFoundVar)
	{
		// Get type string
		size_t length = pFoundVar->GetVarTypeView().size();
		*pReturnType = new char[length+2];
		snprintf(*pReturnType, length+2, "%s", pFoundVar->GetVarTypeStr());
		return true;
	}

	// Nope soft fail
	return false;
}

std::string CVarTable::MakeDefaultVarType(std::string_view decName)
{
	if (decName.empty())
		return std::string();

	if (decName.back() == '#')
		return "float";
	if (decName.back() == '$')
		return "string";

	return "integer";
}

DWORD CVarTable::MakeDefaultVarTypeValue(std::string_view decName)
{
	if (!decName.empty())
	{
		if (decName.back() == '#')
			return 2;
		if (decName.back() == '$')
			return 3;
	}
	return 1;
}

LPSTR CVarTable::MakeTypeNameOfTypeValue(DWORD dwTypeValue)
{
	CStructTable* pCurrent = g_pStructTable;
	while(pCurrent)
	{
		if(pCurrent->GetTypeValue()==dwTypeValue)
		{
			LPSTR pStr = new char[pCurrent->GetTypeName()->Length()+1];
			snprintf(pStr, pCurrent->GetTypeName()->Length()+1, "%s", pCurrent->GetTypeName()->GetStr());
			return pStr;
		}
		pCurrent = pCurrent->GetNext();
	}
	return nullptr;
}

DWORD CVarTable::GetBasicTypeValue(std::string_view typeString)
{
	CStructTable* pCurrent = g_pStructTable;
	while (pCurrent)
	{
		if (pCurrent->GetTypeName())
		{
			if (dbp::iequals(pCurrent->GetTypeName()->GetStr(), typeString))
				return pCurrent->GetTypeValue();
		}
		pCurrent = pCurrent->GetNext();
	}
	return 0;
}

CStructTable* CVarTable::GetStruct(std::string_view typeString)
{
	CStructTable* pCurrent = g_pStructTable;
	while (pCurrent)
	{
		if (pCurrent->GetTypeName())
		{
			if (dbp::iequals(pCurrent->GetTypeName()->GetStr(), typeString))
				return pCurrent;
		}
		pCurrent = pCurrent->GetNext();
	}
	return nullptr;
}

char CVarTable::GetCharOfType(DWORD dwTypeValue)
{
	CStructTable* pCurrent = g_pStructTable;
	while(pCurrent)
	{
		if(pCurrent->GetTypeValue()==dwTypeValue)
		{
			return pCurrent->GetTypeChar();
		}
		pCurrent = pCurrent->GetNext();
	}
	return 0;
}

DWORD CVarTable::GetTypeValueOfChar(unsigned char cTypeChar)
{
	CStructTable* pCurrent = g_pStructTable;
	while(pCurrent)
	{
		if(pCurrent->GetTypeChar()==cTypeChar)
		{
			return pCurrent->GetTypeValue();
		}
		pCurrent = pCurrent->GetNext();
	}
	return 0;
}

bool CVarTable::VerifyVariableStructures(void)
{
	// Scan list and match variable name
	if(GetVarType())
	{
		if(g_pStructTable->DoesTypeEvenExist(GetVarType()->GetStr())==nullptr)
		{
			DWORD LineNumber = GetLineNumber();
			g_pErrorReport->SetError(LineNumber, ERR_SYNTAX+37, GetVarType()->GetStr());
			return false;
		}
	}

	// And next one..
	if(GetNext())
		return GetNext()->VerifyVariableStructures();

	// Variable ok
	return true;
}

DWORD CVarTable::EstablishVarOffsets(DWORD* pdwOffsetValue)
{
	// Only if variable has name
	if(m_pVarName)
	{
		// Only variables that not been assigned offsets
		if(m_bOffsetAssigned==false)
		{
			// Only if variable GLOBAL
			if(m_pVarScope->Length()==0 || m_pVarScope->CheckChars(0, 6, "GLOBAL")==true)
			{
				// Assign offset to this variable
				m_dwFinalDBMOffset = *pdwOffsetValue;
				m_bOffsetAssigned = true;

				// Work out size of variable datatype
				DWORD dwAddSize=0;
				if(m_dwArrFlag)
					dwAddSize = g_pStructTable
						? g_pStructTable->GetTargetAddressSize()
						: static_cast<DWORD>(
							dbp::abi::ActiveTargetAbi::address_size);
				else
				{
					if(GetVarType())
					{
						dwAddSize = g_pStructTable->GetSizeOfType(GetVarType()->GetStr());
					}
				}

				// Add size to offset for next one
				*pdwOffsetValue = (*pdwOffsetValue) + dwAddSize;
			}
		}
	}

	// Write next one
	if(GetNext())
		return GetNext()->EstablishVarOffsets(pdwOffsetValue);

	// Return final accumilated size
	return (*pdwOffsetValue);
}

bool CVarTable::WriteDBMHeader(void)
{
	// Blank Line
	if (!g_pDBMWriter->OutputDBM("")) return false;

	// header Line
	if (!g_pDBMWriter->OutputDBM("VARIABLES:")) return false;

	return true;
}

bool CVarTable::WriteDBMFooter(DWORD dwSizeOfVariableBuffer)
{
	std::string dbmLine = "SIZE OF VARIABLE BUFFER = ";
	dbmLine += std::to_string(dwSizeOfVariableBuffer);
	if (!g_pDBMWriter->OutputDBM(dbmLine)) return false;

	// Complete
	return true;
}

bool CVarTable::WriteDBM(void)
{
	// Only if variable has name
	if (m_pVarName)
	{
		// Get Offset Value
		DWORD dwOffsetValue = m_dwFinalDBMOffset;

		// Only if variable GLOBAL
		if (m_pVarScope->Length() == 0 || m_pVarScope->CheckChars(0, 6, "GLOBAL") == true)
		{
			// Write out text
			std::string dbmLine = "@";
			if (m_pVarName) dbmLine += m_pVarName->View();
			dbmLine += "=";
			dbmLine += std::to_string(dwOffsetValue);

			// Array variables are pointers not actual data mem
			dbmLine += "  [STRUCT@";
			if (m_pVarType) dbmLine += m_pVarType->View();
			dbmLine += "]";

			// Output details
			if (!g_pDBMWriter->OutputDBM(dbmLine)) return false;
		}
	}

	// Write next one
	if (GetNext())
	{
		if (!GetNext()->WriteDBM()) return false;
	}

	// Complete
	return true;
}
