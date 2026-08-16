#include "PEBuilder.h"
#include "ASMWriter.h"
#include "DBPCompiler.h"
#include "DB3Time.h"
#include "ParserHeader.h"
#include "StatementList.h"
#include "DataTable.h"
#include "VarTable.h"
#include "StructTable.h"
#include "Declaration.h"

class CEXEBlock;
class CDLLTable;
class CCommandTable;
class CStringTable;
class CDataTable;
class CVarTable;
class CStructTable;

extern CEXEBlock* g_pEXE;
extern CDataTable* g_pDLLTable;
extern CDataTable* g_pCommandTable;
extern CDataTable* g_pStringTable;
extern CDataTable* g_pDataTable;
extern CVarTable* g_pVarTable;
extern CStructTable* g_pStructTable;

#include <algorithm>
#include <memory>
#include <vector>

namespace
{
bool ReportDllTableError(const char* message)
{
	if (g_pErrorReport != nullptr)
		g_pErrorReport->AddErrorString(message);
	return false;
}
}

void CPEBuilder::Reset() noexcept
{
    m_bPrepared = false;
    m_dwHeaderSize = 0;
}

DWORD CPEBuilder::CalculateAlignedSize(DWORD dwUnalignedSize, DWORD dwAlignment) const noexcept
{
    if (dwAlignment == 0) return dwUnalignedSize;
    DWORD dwRemainder = dwUnalignedSize % dwAlignment;
    if (dwRemainder == 0) return dwUnalignedSize;
    return dwUnalignedSize + (dwAlignment - dwRemainder);
}

bool CPEBuilder::ValidatePEHeaderRequirements(DWORD dwImageBase, DWORD dwSectionAlignment, DWORD dwFileAlignment) const noexcept
{
    if (dwImageBase == 0) return false;
    if (dwSectionAlignment == 0 || dwFileAlignment == 0) return false;
    if (dwFileAlignment > dwSectionAlignment) return false;
    return true;
}

bool CPEBuilder::ValidatePE64HeaderRequirements(uint64_t dwImageBase, DWORD dwSectionAlignment, DWORD dwFileAlignment) const noexcept
{
    if (dwImageBase == 0ULL) return false;
    if (dwSectionAlignment == 0 || dwFileAlignment == 0) return false;
    if (dwFileAlignment > dwSectionAlignment) return false;
    return true;
}

bool CPEBuilder::UpdateDLLData() const
{
	db3::CProfile<> prof("CPEBuilder::UpdateDLLData");
	if (g_pEXE == nullptr || g_pDLLTable == nullptr)
		return ReportDllTableError("Cannot build executable DLL data without compiler tables");

	const DWORD oldSize = g_pEXE->m_dwNumberOfDLLs;
	if (oldSize > RuntimeDllCapacity)
		return ReportDllTableError("Existing executable DLL table exceeds the runtime dispatch capacity");
	if (oldSize != 0 &&
		(g_pEXE->m_pDLLIndexArray == nullptr ||
		 g_pEXE->m_pDLLFilenameArray == nullptr ||
		 g_pEXE->m_pDLLLoadedAlreadyArray == nullptr))
		return ReportDllTableError("Existing executable DLL table is incomplete");
	for (DWORD index = 0; index < oldSize; ++index)
	{
		if (!IsRuntimeDllIndex(g_pEXE->m_pDLLIndexArray[index]))
			return ReportDllTableError("Existing executable DLL data contains an out-of-range runtime index");
	}

	std::vector<CDataTable*> pendingEntries;
	std::vector<CDataTable*> existingEntries;
	for (CDataTable* entry = g_pDLLTable->GetNext(); entry != nullptr; entry = entry->GetNext())
	{
		if (entry->GetAddedToEXEData())
			continue;
		if (entry->GetString() == nullptr || entry->GetString()->GetStr() == nullptr)
			return ReportDllTableError("Compiler DLL table contains an entry without a filename");
		if (!IsRuntimeDllIndex(entry->GetIndex()))
			return ReportDllTableError("Compiler DLL table exceeds the 256-slot runtime dispatch capacity");

		const char* filename = entry->GetString()->GetStr();
		bool alreadyPresent = false;
		for (DWORD index = 0; index < oldSize; ++index)
		{
			const auto* existing = reinterpret_cast<const char*>(
				g_pEXE->m_pDLLFilenameArray[index]);
			if (existing != nullptr && _stricmp(existing, filename) == 0)
			{
				alreadyPresent = true;
				break;
			}
		}
		if (alreadyPresent)
		{
			existingEntries.push_back(entry);
			continue;
		}
		pendingEntries.push_back(entry);
	}

	if (pendingEntries.empty())
	{
		for (CDataTable* entry : existingEntries)
			entry->SetAddedToEXEData(true);
		return true;
	}
	if (pendingEntries.size() > RuntimeDllCapacity - oldSize)
		return ReportDllTableError("Executable DLL table exceeds the 256-slot runtime dispatch capacity");
	std::stable_partition(
		pendingEntries.begin(),
		pendingEntries.end(),
		[](CDataTable* entry) {
			return _stricmp(entry->GetString()->GetStr(), "dbprocore.dll") == 0;
		});

	const DWORD newSize = oldSize + static_cast<DWORD>(pendingEntries.size());
	auto indexes = std::unique_ptr<DWORD[]>(g_pEXE->CreateArray(newSize));
	auto filenames = std::unique_ptr<uintptr_t[]>(g_pEXE->CreatePtrArray(newSize));
	auto loaded = std::unique_ptr<DWORD[]>(g_pEXE->CreateArray(newSize));
	if (!indexes || !filenames || !loaded)
		return ReportDllTableError("Unable to allocate executable DLL tables");

	if (oldSize != 0)
	{
		std::copy_n(g_pEXE->m_pDLLIndexArray, oldSize, indexes.get());
		std::copy_n(g_pEXE->m_pDLLFilenameArray, oldSize, filenames.get());
		std::copy_n(g_pEXE->m_pDLLLoadedAlreadyArray, oldSize, loaded.get());
	}

	std::vector<std::unique_ptr<char[]>> pendingNames;
	pendingNames.reserve(pendingEntries.size());
	for (CDataTable* entry : pendingEntries)
	{
		const char* source = entry->GetString()->GetStr();
		auto filename = std::make_unique<char[]>(strlen(source) + 1u);
		strcpy_s(filename.get(), strlen(source) + 1u, source);
		pendingNames.push_back(std::move(filename));
	}

	DWORD outputIndex = oldSize;
	for (std::size_t index = 0; index < pendingEntries.size(); ++index)
	{
		CDataTable* entry = pendingEntries[index];
		indexes[outputIndex] = entry->GetIndex();
		filenames[outputIndex] = reinterpret_cast<uintptr_t>(pendingNames[index].release());
		++outputIndex;
	}

	std::unique_ptr<DWORD[]> oldIndexes(g_pEXE->m_pDLLIndexArray);
	std::unique_ptr<uintptr_t[]> oldFilenames(g_pEXE->m_pDLLFilenameArray);
	std::unique_ptr<DWORD[]> oldLoaded(g_pEXE->m_pDLLLoadedAlreadyArray);
	g_pEXE->m_pDLLIndexArray = indexes.release();
	g_pEXE->m_pDLLFilenameArray = filenames.release();
	g_pEXE->m_pDLLLoadedAlreadyArray = loaded.release();
	g_pEXE->m_dwNumberOfDLLs = newSize;
	for (CDataTable* entry : pendingEntries)
		entry->SetAddedToEXEData(true);
	for (CDataTable* entry : existingEntries)
		entry->SetAddedToEXEData(true);

	return true;
}

bool CPEBuilder::UpdateCommandData() const
{
	db3::CProfile<> prof("CPEBuilder::UpdateCommandData");

	if(g_pEXE->m_pCommandDLLIdArray==nullptr)
	{
		DWORD dwNewSize = g_pStatementList->GetCommandIndexCounter();
		LPSTR pNewArray1 = (LPSTR)g_pEXE->CreateArray(dwNewSize);
		uintptr_t* pNewArray2 = g_pEXE->CreatePtrArray(dwNewSize);

		g_pEXE->m_dwNumberOfCommands = dwNewSize;
		g_pEXE->m_pCommandDLLIdArray = (DWORD*)pNewArray1;
		g_pEXE->m_pCommandDLLCallArray = pNewArray2;
	}
	else
	{
		DWORD dwOldSize = g_pEXE->m_dwNumberOfCommands;
		LPSTR pOldArray1 = (LPSTR)g_pEXE->m_pCommandDLLIdArray;
		uintptr_t* pOldArray2 = g_pEXE->m_pCommandDLLCallArray;
		DWORD dwNewSize = g_pStatementList->GetCommandIndexCounter();
		LPSTR pNewArray1 = (LPSTR)g_pEXE->CreateArray(dwNewSize);
		uintptr_t* pNewArray2 = g_pEXE->CreatePtrArray(dwNewSize);

		memcpy(pNewArray1, pOldArray1, dwOldSize*sizeof(DWORD));
		memcpy(pNewArray2, pOldArray2, dwOldSize*sizeof(uintptr_t));
		std::unique_ptr<DWORD[]> pOldArray1Owner((DWORD*)pOldArray1);
		std::unique_ptr<uintptr_t[]> pOldArray2Owner(pOldArray2);

		g_pEXE->m_dwNumberOfCommands = dwNewSize;
		g_pEXE->m_pCommandDLLIdArray = (DWORD*)pNewArray1;
		g_pEXE->m_pCommandDLLCallArray = pNewArray2;
	}

	CDataTable* pStringEntry = g_pCommandTable->GetNext();
	for(DWORD c=0; c<g_pEXE->m_dwNumberOfCommands; c++)
	{
		if(pStringEntry->GetAddedToEXEData()==false)
		{
			pStringEntry->SetAddedToEXEData(true);

			LPSTR pLeft = nullptr;
			LPSTR pRight = nullptr;
			std::unique_ptr<char[]> pLeftOwner, pRightOwner;
			if(pStringEntry)
			{
				CStr* pStr = pStringEntry->GetString();
				DWORD dwPos = pStr->FindFirstChar(',');
				pLeftOwner.reset(pStr->GetLeftOfPosition(dwPos));
				pRightOwner.reset(pStr->GetRightOfPosition(dwPos+1));
				pLeft = pLeftOwner.get();
				pRight = pRightOwner.get();
			}

			char* pDynamicString = new char[strlen(pRight)+1];
			strcpy_s(pDynamicString, strlen(pRight)+1, pRight);

			g_pEXE->m_pCommandDLLIdArray[c]=atoi(pLeft);
			g_pEXE->m_pCommandDLLCallArray[c]=(uintptr_t)pDynamicString;
		}

		if(pStringEntry) pStringEntry = pStringEntry->GetNext();
	}

	return true;
}

bool CPEBuilder::UpdateStringData() const
{
	db3::CProfile<> prof("CPEBuilder::UpdateStringData");

	if(g_pEXE->m_pStringsArray==nullptr)
	{
		DWORD dwNewSize = g_pStatementList->GetStringIndexCounter();
		uintptr_t* pNewArray = g_pEXE->CreatePtrArray(dwNewSize);

		g_pEXE->m_dwNumberOfStrings = dwNewSize;
		g_pEXE->m_pStringsArray = pNewArray;
	}
	else
	{
		DWORD dwOldSize = g_pEXE->m_dwNumberOfStrings;
		std::unique_ptr<uintptr_t[]> pOldArray(g_pEXE->m_pStringsArray);
		DWORD dwNewSize = g_pStatementList->GetStringIndexCounter();
		uintptr_t* pNewArray = g_pEXE->CreatePtrArray(dwNewSize);

		memcpy(pNewArray, pOldArray.get(), dwOldSize*sizeof(uintptr_t));

		g_pEXE->m_dwNumberOfStrings = dwNewSize;
		g_pEXE->m_pStringsArray = pNewArray;
	}

	CDataTable* pStringEntry = g_pStringTable->GetNext();
	for(DWORD s=0; s<g_pEXE->m_dwNumberOfStrings; s++)
	{
		if(pStringEntry->GetAddedToEXEData()==false)
		{
			pStringEntry->SetAddedToEXEData(true);

			LPCSTR pStringData=nullptr;
			CStr noSpeechMarks;
			if(pStringEntry)
			{
				noSpeechMarks.SetText(pStringEntry->GetString()->GetStr());
				noSpeechMarks.EatSpeechMarks();
				pStringData = noSpeechMarks.GetStr();
			}
			else
				pStringData = "???";

			char* pDynamicString = new char[strlen(pStringData)+1];
			strcpy_s(pDynamicString, strlen(pStringData)+1, pStringData);

			g_pEXE->m_pStringsArray[s]=(uintptr_t)pDynamicString;
		}

		if(pStringEntry) pStringEntry = pStringEntry->GetNext();
	}

	return true;
}

bool CPEBuilder::UpdateDataData() const
{
	db3::CProfile<> prof("CPEBuilder::UpdateDataData");

	if(g_pEXE->m_pDataArray==nullptr)
	{
		DWORD dwNewSize = g_pStatementList->GetDataIndexCounter();
		LPSTR pNewArray1 = (LPSTR)new char[dwNewSize*10];
		uintptr_t* pNewArray2 = g_pEXE->CreatePtrArray(dwNewSize);

		g_pEXE->m_dwNumberOfDataItems = dwNewSize;
		g_pEXE->m_pDataArray = pNewArray1;
		g_pEXE->m_pDataStringsArray = pNewArray2;
	}
	else
	{
		DWORD dwOldSize = g_pEXE->m_dwNumberOfDataItems;
		LPSTR pOldArray1 = (LPSTR)g_pEXE->m_pDataArray;
		uintptr_t* pOldArray2 = g_pEXE->m_pDataStringsArray;
		DWORD dwNewSize = g_pStatementList->GetDataIndexCounter();
		LPSTR pNewArray1 = (LPSTR)new char[dwNewSize*10];
		uintptr_t* pNewArray2 = g_pEXE->CreatePtrArray(dwNewSize);

		memcpy(pNewArray1, pOldArray1, dwOldSize*10);
		memcpy(pNewArray2, pOldArray2, dwOldSize*sizeof(uintptr_t));
		std::unique_ptr<char[]> pOldArray1Owner(pOldArray1);
		std::unique_ptr<uintptr_t[]> pOldArray2Owner(pOldArray2);

		g_pEXE->m_dwNumberOfDataItems = dwNewSize;
		g_pEXE->m_pDataArray = pNewArray1;
		g_pEXE->m_pDataStringsArray = pNewArray2;
	}

	CDataTable* pDataEntry = g_pDataTable->GetNext();
	for(DWORD d=0; d<g_pEXE->m_dwNumberOfDataItems*10; d+=10)
	{
		if(pDataEntry->GetAddedToEXEData()==false)
		{
			pDataEntry->SetAddedToEXEData(true);

			DWORD dwType = pDataEntry->GetType();
			g_pEXE->m_pDataArray[d+0] = (unsigned char)dwType;
			g_pEXE->m_pDataArray[d+1] = 0;

			if(dwType==1)
			{
				*(double*)&g_pEXE->m_pDataArray[d+2] = (double)pDataEntry->GetNumeric();
			}
			if(dwType==2)
			{
				LPCSTR pDataItem=nullptr;
				if(pDataEntry)
					pDataItem = pDataEntry->GetString()->GetStr();
				else
					pDataItem = "???";

				char* pDynamicString = new char[strlen(pDataItem)+1];
				strcpy_s(pDynamicString, strlen(pDataItem)+1, pDataItem);

				DWORD dwStrIndex=d/10;
				*(uintptr_t*)&g_pEXE->m_pDataArray[d+2] = static_cast<uintptr_t>(dwStrIndex);
				g_pEXE->m_pDataStringsArray[dwStrIndex]=(uintptr_t)pDynamicString;
			}
		}

		if(pDataEntry) pDataEntry = pDataEntry->GetNext();
	}

	return true;
}

bool CPEBuilder::UpdateDynamicData() const
{
	db3::CProfile<> prof("CPEBuilder::UpdateDynamicData");

	std::unique_ptr<DWORD[]> oldDynamicVars((DWORD*)g_pEXE->m_pDynamicVarsArray);
	std::unique_ptr<DWORD[]> oldDynamicVarsType((DWORD*)g_pEXE->m_pDynamicVarsArrayType);
	g_pEXE->m_pDynamicVarsArray = nullptr;
	g_pEXE->m_pDynamicVarsArrayType = nullptr;

	DWORD dwDynamicVarsCounter=0;
	for(short pass=0; pass<=1; pass++)
	{
		if(pass==1)
		{
			g_pEXE->m_pDynamicVarsArray = (DWORD*)g_pEXE->CreateArray(g_pEXE->m_dwDynamicVarsQuantity);
			g_pEXE->m_pDynamicVarsArrayType = (DWORD*)g_pEXE->CreateArray(g_pEXE->m_dwDynamicVarsQuantity);
		}

		dwDynamicVarsCounter=0;
		CVarTable* pVarEntry = g_pVarTable;
		while(pVarEntry)
		{
			if(strcmp(pVarEntry->GetVarScope()->GetStr(),"")==0)
			{
				if(pVarEntry->GetArrFlag()==1 || pVarEntry->GetVarTypeValue()==3)
				{
					if(pass==1)
					{
						g_pEXE->m_pDynamicVarsArray[dwDynamicVarsCounter]=pVarEntry->GetOffsetValue();
						g_pEXE->m_pDynamicVarsArrayType[dwDynamicVarsCounter]=pVarEntry->GetArrFlag();
					}
					dwDynamicVarsCounter++;
				}
			}
			pVarEntry=pVarEntry->GetNext();
		}

		if(pass==0)
		{
			g_pEXE->m_dwDynamicVarsQuantity = dwDynamicVarsCounter;
		}
	}
	
	return true;
}

static void UpdateStructurePatternDataRec(std::string& pattern, CDeclaration* pDecMain)
{
	while(pDecMain)
	{
		LPCSTR pTypeLetter = "-";
		LPSTR pFullString = pDecMain->GetType()->GetStr();
		if ( _stricmp ( "integer", pFullString )==0 )			pTypeLetter = "L";
		if ( _stricmp ( "float", pFullString )==0 )			pTypeLetter = "F";
		if ( _stricmp ( "string", pFullString )==0 )			pTypeLetter = "S";
		if ( _stricmp ( "boolean", pFullString )==0 )			pTypeLetter = "B";
		if ( _stricmp ( "byte", pFullString )==0 )			pTypeLetter = "Y";
		if ( _stricmp ( "word", pFullString )==0 )			pTypeLetter = "W";
		if ( _stricmp ( "dword", pFullString )==0 )			pTypeLetter = "D";
		if ( _stricmp ( "double float", pFullString )==0 )	pTypeLetter = "O";
		if ( _stricmp ( "double integer", pFullString )==0 )	pTypeLetter = "R";

		CStructTable* pStruct = g_pStructTable->DoesTypeEvenExist(pDecMain->GetType()->GetStr());
		if(pStruct)
		{
			CDeclaration* pDeeperDec = pStruct->GetDecChain();
			if(pDeeperDec)
			{
				UpdateStructurePatternDataRec ( pattern, pDeeperDec );
				pTypeLetter = "";
			}
		}

		pattern += pTypeLetter;
		pDecMain = pDecMain->GetNext();
	}
}

bool CPEBuilder::UpdateStructurePatternData() const
{
	db3::CProfile<> prof("CPEBuilder::UpdateStructurePatternData");

	g_pEXE->m_dwUsertypeStringPatternQuantity = 0;
	std::unique_ptr<char[]> oldPatternArray((char*)g_pEXE->m_pUsertypeStringPatternArray);
	g_pEXE->m_pUsertypeStringPatternArray = nullptr;

	for(short pass=0; pass<=1; pass++)
	{
		if(pass==1)
		{
			if ( g_pEXE->m_dwUsertypeStringPatternQuantity > 0 )
			{
				g_pEXE->m_pUsertypeStringPatternArray = new char[g_pEXE->m_dwUsertypeStringPatternQuantity];
				g_pEXE->m_pUsertypeStringPatternArray[0] = 0;
			}
		}

		DWORD dwCounter=0;
		CStructTable* pEntry = g_pStructTable;
		while(pEntry)
		{
			if(pEntry->GetDecChain())
			{
				LPSTR pTypeName = pEntry->GetTypeName()->GetStr();
				std::string pPattern = std::string(pTypeName) + ":" +
					std::to_string(g_pStructTable->FindIndex(pTypeName)) + ":";
				UpdateStructurePatternDataRec(pPattern, pEntry->GetDecChain());
				pPattern += ":";
				if(pass==1)
					strncat_s(
						g_pEXE->m_pUsertypeStringPatternArray,
						g_pEXE->m_dwUsertypeStringPatternQuantity,
						pPattern.c_str(),
						_TRUNCATE);
				dwCounter += static_cast<DWORD>(pPattern.size()) + 1;
			}
			pEntry=pEntry->GetNext();
		}

		if(pass==0) g_pEXE->m_dwUsertypeStringPatternQuantity = dwCounter;
	}
	
	return true;
}
