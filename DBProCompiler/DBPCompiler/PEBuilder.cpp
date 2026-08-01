#include "PEBuilder.h"
#include "DBPCompiler.h"
#include "ParserHeader.h"
#include "StatementList.h"
#include "DataTable.h"
#include "VarTable.h"
#include "StructTable.h"
#include "Declaration.h"

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

bool CPEBuilder::UpdateDLLData() const
{
	db3::CProfile<> prof("CPEBuilder::UpdateDLLData");

	DWORD dwDLLIndex = 0;
	if(g_pEXE->m_pDLLIndexArray==NULL)
	{
		DWORD dwDLLCount = g_pStatementList->GetDLLIndexCounter();
		LPSTR pNewArray1 = (LPSTR)g_pEXE->CreateArray(dwDLLCount);
		uintptr_t* pNewArray2 = g_pEXE->CreatePtrArray(dwDLLCount);
		LPSTR pNewArray3 = (LPSTR)g_pEXE->CreateArray(dwDLLCount);

		g_pEXE->m_dwNumberOfDLLs = dwDLLCount;
		g_pEXE->m_pDLLIndexArray = (DWORD*)pNewArray1;
		g_pEXE->m_pDLLFilenameArray = pNewArray2;
		g_pEXE->m_pDLLLoadedAlreadyArray = (DWORD*)pNewArray3;
	}
	else
	{
		DWORD dwNewDLLs=0;
		CDataTable* pStringEntry = g_pDLLTable->GetNext();
		while(pStringEntry)
		{
			if(pStringEntry->GetAddedToEXEData()==false) dwNewDLLs++;
			pStringEntry=pStringEntry->GetNext();
		}

		DWORD dwOldSize = g_pEXE->m_dwNumberOfDLLs;
		LPSTR pOldArray1 = (LPSTR)g_pEXE->m_pDLLIndexArray;
		uintptr_t* pOldArray2 = g_pEXE->m_pDLLFilenameArray;
		LPSTR pOldArray3 = (LPSTR)g_pEXE->m_pDLLLoadedAlreadyArray;
		DWORD dwNewSize = dwOldSize + dwNewDLLs;
		if(dwNewSize>dwOldSize)
		{
			LPSTR pNewArray1 = (LPSTR)g_pEXE->CreateArray(dwNewSize);
			uintptr_t* pNewArray2 = g_pEXE->CreatePtrArray(dwNewSize);
			LPSTR pNewArray3 = (LPSTR)g_pEXE->CreateArray(dwNewSize);

			memcpy(pNewArray1, pOldArray1, dwOldSize*sizeof(DWORD));
			memcpy(pNewArray2, pOldArray2, dwOldSize*sizeof(uintptr_t));
			memcpy(pNewArray3, pOldArray3, dwOldSize*sizeof(DWORD));
			std::unique_ptr<DWORD[]> pOldArray1Owner((DWORD*)pOldArray1);
			std::unique_ptr<uintptr_t[]> pOldArray2Owner(pOldArray2);
			std::unique_ptr<DWORD[]> pOldArray3Owner((DWORD*)pOldArray3);

			g_pEXE->m_dwNumberOfDLLs = dwNewSize;
			g_pEXE->m_pDLLIndexArray = (DWORD*)pNewArray1;
			g_pEXE->m_pDLLFilenameArray = pNewArray2;
			g_pEXE->m_pDLLLoadedAlreadyArray = (DWORD*)pNewArray3;
		}

		dwDLLIndex=dwOldSize;
	}

	for(DWORD pass=0; pass<2; pass++)
	{
		CDataTable* pStringEntry = g_pDLLTable->GetNext();
		while(pStringEntry)
		{
			if(pStringEntry->GetAddedToEXEData()==false)
			{
				LPSTR pStringData = pStringEntry->GetString()->GetStr();

				bool bAddThisString=false;
				if(pass==0 && stricmp("dbprocore.dll", pStringData)==NULL) bAddThisString=true;
				if(pass==1 && stricmp("dbprocore.dll", pStringData)!=NULL) bAddThisString=true;
				if(bAddThisString)
				{
					pStringEntry->SetAddedToEXEData(true);

					bool bNotGot=true;
					for(DWORD n=0; n<g_pEXE->m_dwNumberOfDLLs; n++)
					{
						LPSTR pCompareWith=(LPSTR)(g_pEXE->m_pDLLFilenameArray[n]);
						if(pCompareWith)
							if(stricmp(pCompareWith, pStringData)==NULL) bNotGot=false;
					}

					if(bNotGot)
					{
						char* pDynamicString = new char[strlen(pStringData)+1];
						strcpy_s(pDynamicString, strlen(pStringData)+1, pStringData);

						g_pEXE->m_pDLLIndexArray[dwDLLIndex]=pStringEntry->GetIndex();
						g_pEXE->m_pDLLFilenameArray[dwDLLIndex]=(uintptr_t)pDynamicString;
						dwDLLIndex++;
					}
				}
			}
			pStringEntry=pStringEntry->GetNext();
		}
	}

	return true;
}

bool CPEBuilder::UpdateCommandData() const
{
	db3::CProfile<> prof("CPEBuilder::UpdateCommandData");

	if(g_pEXE->m_pCommandDLLIdArray==NULL)
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

			LPSTR pLeft = "???";
			LPSTR pRight = "???";
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

	if(g_pEXE->m_pStringsArray==NULL)
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

			LPSTR pStringData=NULL;
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

	if(g_pEXE->m_pDataArray==NULL)
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
				LPSTR pDataItem=NULL;
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
	g_pEXE->m_pDynamicVarsArray = NULL;
	g_pEXE->m_pDynamicVarsArrayType = NULL;

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
			if(strcmp(pVarEntry->GetVarScope()->GetStr(),"")==NULL)
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

static void UpdateStructurePatternDataRec(LPSTR pPattern, CDeclaration* pDecMain)
{
	while(pDecMain)
	{
		LPSTR pTypeLetter = "-";
		LPSTR pFullString = pDecMain->GetType()->GetStr();
		if ( stricmp ( "integer", pFullString )==NULL )			pTypeLetter = "L";
		if ( stricmp ( "float", pFullString )==NULL )			pTypeLetter = "F";
		if ( stricmp ( "string", pFullString )==NULL )			pTypeLetter = "S";
		if ( stricmp ( "boolean", pFullString )==NULL )			pTypeLetter = "B";
		if ( stricmp ( "byte", pFullString )==NULL )			pTypeLetter = "Y";
		if ( stricmp ( "word", pFullString )==NULL )			pTypeLetter = "W";
		if ( stricmp ( "dword", pFullString )==NULL )			pTypeLetter = "D";
		if ( stricmp ( "double float", pFullString )==NULL )	pTypeLetter = "O";
		if ( stricmp ( "double integer", pFullString )==NULL )	pTypeLetter = "R";

		CStructTable* pStruct = g_pStructTable->DoesTypeEvenExist(pDecMain->GetType()->GetStr());
		if(pStruct)
		{
			CDeclaration* pDeeperDec = pStruct->GetDecChain();
			if(pDeeperDec)
			{
				UpdateStructurePatternDataRec ( pPattern, pDeeperDec );
				pTypeLetter = "";
			}
		}

		strcat ( pPattern, pTypeLetter );
		pDecMain = pDecMain->GetNext();
	}
}

bool CPEBuilder::UpdateStructurePatternData() const
{
	db3::CProfile<> prof("CPEBuilder::UpdateStructurePatternData");

	g_pEXE->m_dwUsertypeStringPatternQuantity = 0;
	std::unique_ptr<char[]> oldPatternArray((char*)g_pEXE->m_pUsertypeStringPatternArray);
	g_pEXE->m_pUsertypeStringPatternArray = NULL;

	for(short pass=0; pass<=1; pass++)
	{
		if(pass==1)
		{
			if ( g_pEXE->m_dwUsertypeStringPatternQuantity > 0 )
			{
				g_pEXE->m_pUsertypeStringPatternArray = new char[g_pEXE->m_dwUsertypeStringPatternQuantity];
				strcpy ( (LPSTR)g_pEXE->m_pUsertypeStringPatternArray, "" );
			}
		}

		DWORD dwCounter=0;
		CStructTable* pEntry = g_pStructTable;
		while(pEntry)
		{
			if(pEntry->GetDecChain())
			{
				char pPattern[512];
				LPSTR pTypeName = pEntry->GetTypeName()->GetStr();
				strcpy ( pPattern, pTypeName );
				strcat ( pPattern, ":" );
				char num[32];
				wsprintf ( num, "%d", g_pStructTable->FindIndex(pTypeName) );
				strcat ( pPattern, num );
				strcat ( pPattern, ":" );
				UpdateStructurePatternDataRec ( pPattern, pEntry->GetDecChain() );
				strcat ( pPattern, ":" );
				if(pass==1) strcat ( (LPSTR)g_pEXE->m_pUsertypeStringPatternArray, pPattern );
				dwCounter+=strlen(pPattern)+1;
			}
			pEntry=pEntry->GetNext();
		}

		if(pass==0) g_pEXE->m_dwUsertypeStringPatternQuantity = dwCounter;
	}
	
	return true;
}
