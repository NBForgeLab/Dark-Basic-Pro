// DataTable.cpp: implementation of the CDataTable class.
//
//////////////////////////////////////////////////////////////////////

#include "DataTable.h"
#include "StringUtils.h"

// Includes and external ptr for AssociateDLL scan
#include "DBPCompiler.h"
#include <filesystem>
#include "TextConvert.h"
#include "SafeDLLLoading.h"
extern CDBPCompiler* g_pDBPCompiler;
extern bool g_bExternaliseDLLS;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDataTable::CDataTable()
	: m_dwIndex(0), m_dwType(0), m_pNumeric(0), m_bAddedToEXEData(false)
{
}

CDataTable::CDataTable(LPCSTR pInitString)
	: m_dwIndex(0), m_dwType(0), m_pNumeric(0),
	  m_pString(std::make_unique<CStr>(pInitString)),
	  m_pString2(std::make_unique<CStr>((LPSTR)"")),
	  m_bAddedToEXEData(false)
{
}

CDataTable::~CDataTable()
{
	// Iteratively release chain to prevent stack overflow on deep lists
	auto current = std::move(m_pNext);
	while (current) {
		current = std::move(current->m_pNext);
	}
}

void CDataTable::Free(void)
{
	// Iteratively release the entire chain after this node
	auto current = std::move(m_pNext);
	while (current) {
		auto next = std::move(current->m_pNext);
		current.reset();
		current = std::move(next);
	}
	// Delete self (preserves original Free() semantics for callers)
	delete this;
}

void CDataTable::Add(CDataTable* pNew)
{
	CDataTable* pCurrent = this;
	while(pCurrent->m_pNext)
	{
		pCurrent = pCurrent->m_pNext.get();
	}
	pCurrent->m_pNext.reset(pNew);
}

bool CDataTable::AddNumeric(double dNum, DWORD dwIndex)
{
	// Create new data item (owned until handed to the chain)
	auto pNewData = std::make_unique<CDataTable>();
	pNewData->SetNumeric(dNum);

	// Set index
	pNewData->SetIndex(dwIndex);

	// Add to Data Table (chain takes ownership)
	Add(pNewData.release());

	// Complete
	return true;
}

bool CDataTable::AddString(LPSTR pString, DWORD dwIndex)
{
	// Create new data item (owned until handed to the chain)
	auto pNewData = std::make_unique<CDataTable>();
	pNewData->SetString(std::make_unique<CStr>(pString).release());
	pNewData->SetString2(nullptr);

	// Set index
	pNewData->SetIndex(dwIndex);

	// Add to Data Table (chain takes ownership)
	Add(pNewData.release());

	// Complete
	return true;
}

bool CDataTable::AddTwoStrings(LPSTR pString, LPSTR pString2, DWORD* dwIndex)
{
	// If string is NOT unique, fail
	DWORD dwResult = FindString(pString);
	if(dwResult>0)
	{
		*dwIndex=dwResult;
		return false;
	}

	// Create new data item (owned until handed to the chain)
	auto pNewData = std::make_unique<CDataTable>();
	pNewData->SetString(std::make_unique<CStr>(pString).release());
	pNewData->SetString2(std::make_unique<CStr>(pString2).release());

	// Set index
	pNewData->SetIndex(*dwIndex);

	// Add to Data Table (chain takes ownership)
	Add(pNewData.release());

	// Complete
	return true;
}

bool CDataTable::AddUniqueString(LPCSTR pString, DWORD* dwIndex)
{
	// If string is NOT unique, fail
	DWORD dwResult = FindString(pString);
	if(dwResult>0)
	{
		*dwIndex=dwResult;
		return false;
	}

	// Create new data item (owned until handed to the chain)
	auto pNewData = std::make_unique<CDataTable>();
	pNewData->SetString(std::make_unique<CStr>(pString).release());
	pNewData->SetString2(nullptr);

	// Set index
	pNewData->SetIndex(*dwIndex);

	// Add to Data Table (chain takes ownership)
	Add(pNewData.release());

	// Complete
	return true;
}

DWORD CDataTable::FindString(LPCSTR pFindString)
{
	// Find String
	CDataTable* pCurrent = this;
	while(pCurrent)
	{
		// Match list item with search string
		if(pCurrent->GetString())
			if(dbp::iequals(pCurrent->GetString()->GetStr(), pFindString))
				return pCurrent->GetIndex();

		pCurrent=pCurrent->GetNext();
	}

	// Failed to find
	return 0;
}

bool CDataTable::FindIndexStr(LPCSTR pIndexAsString)
{
	// Convert String to Index
	DWORD dwFindIndex = atoi(pIndexAsString);

	// Find String
	CDataTable* pCurrent = this;
	while(pCurrent)
	{
		// Match list item with search string
		if(pCurrent->GetString())
			if(pCurrent->GetIndex()==dwFindIndex)
				return true;

		pCurrent=pCurrent->GetNext();
	}

	// Soft Failed to find
	return false;
}

bool CDataTable::NotExcluded ( LPCSTR pFilename )
{
	// false if excluded from compile
	for ( DWORD i=1; i<g_pDBPCompiler->g_dwExcludeFilesCount; i++)
		if ( !g_pDBPCompiler->g_ExcludeFiles [ i ].empty() )
			if ( dbp::iequals( g_pDBPCompiler->g_ExcludeFiles [ i ].c_str(), pFilename ) )
				return false;

	// lee - 270308 - u67 - do not include DLL at all if flagged
	if ( g_bExternaliseDLLS==true )
		return false;

	// complete, not excluded
	return true;
}

int CDataTable::CompleteAnyLinkAssociates(void)
{
	// Scan user plugins - check if associations require any DBPro DLLs
	bool bAtLeastOneUserDLLNeeds3D = false;
	bool bAtLeastOneUserDLLNeedsSOUND = false;

	// reset index
	DWORD dwIndex=0;
	DWORD dwIndexBeforeAdds=0;

	// First pass basic DLLs, second pass is dependence additions
	for ( int iAddDependentsLoop=0; iAddDependentsLoop<2; iAddDependentsLoop++ )
	{
		for ( int iPass=0; iPass<2; iPass++ )
		{
			// Switch to PLUGINS-XXXX Folder
			std::error_code ec;
			const auto prevPath = std::filesystem::current_path(ec);

			// Depends on pass value
			if ( iPass==0 ) std::filesystem::current_path(g_pDBPCompiler->GetInternalFile(PATH_PLUGINSUSERFOLDER), ec);
			if ( iPass==1 ) std::filesystem::current_path(g_pDBPCompiler->GetInternalFile(PATH_PLUGINSLICENSEDFOLDER), ec);

			// Go through DLLs from direct-command-list
			CDataTable* pCurrent = this->GetNext();
			while(pCurrent)
			{
				// Check if DLL is user-dll ( leefix - 011208 - u71 - gameFX needed to link to Basic3D! )
				LPSTR pDLLName = pCurrent->GetString()->GetStr();
				if ( _strnicmp ( pDLLName, "dbpro", 5 )!=0 || _strnicmp ( pDLLName, "dbprogamefx", 11 )==0 )
				{
					// must be user DLL (associated with main DLL)
					int iAssociationCode = 0;
					HMODULE hModule = dbp::dll::LoadApplicationDLLW(TextConvert::UTF8ToUTF16(pDLLName).c_str());
					if(hModule)
					{
						// get associate dll value if any
						if ( iAddDependentsLoop==0 )
						{
							typedef int ( *RETINTNOPARAM ) ( void );
							RETINTNOPARAM GetAssociatedDLLs = ( RETINTNOPARAM ) GetProcAddress ( hModule, "?GetAssociatedDLLs@@YAHXZ" );
							if (!GetAssociatedDLLs)
								GetAssociatedDLLs = (RETINTNOPARAM)GetProcAddress(hModule, "GetAssociatedDLLs");
							if ( GetAssociatedDLLs ) iAssociationCode=GetAssociatedDLLs();
						}
						else
						{
							// get num of additional dependencies
							int iNumDLLDependencies = 0;
							typedef int ( *RETINTNOPARAM ) ( void );
							RETINTNOPARAM GetNumDependencies = ( RETINTNOPARAM ) GetProcAddress ( hModule, "?GetNumDependencies@@YAHXZ" );
							if (!GetNumDependencies)
								GetNumDependencies = (RETINTNOPARAM)GetProcAddress(hModule, "GetNumDependencies");
							if ( GetNumDependencies ) iNumDLLDependencies=GetNumDependencies();
							if ( iNumDLLDependencies > 0 )
							{
								typedef const char * ( *RETLPSTRNOPARAM ) ( int n );
								RETLPSTRNOPARAM GetDependencyID = ( RETLPSTRNOPARAM ) GetProcAddress ( hModule, "?GetDependencyID@@YAPBDH@Z" );
								if (!GetDependencyID)
									GetDependencyID = (RETLPSTRNOPARAM)GetProcAddress(hModule, "GetDependencyID");
								// store dependencies in list
								for ( int iD=0; iD<iNumDLLDependencies; iD++ )
								{
									DWORD dwTry=dwIndex+1;
									if(AddUniqueString(GetDependencyID(iD), &dwTry)) dwIndex=dwTry;
								}
							}
						}
					}

					// free it if loaded
					if(hModule)
					{
						FreeLibrary(hModule);
						hModule=nullptr;
					}

					// Association Codes (1=3d/2=sound/4-//)
					if ( iAssociationCode & 1 ) bAtLeastOneUserDLLNeeds3D=true;
					if ( iAssociationCode & 2 ) bAtLeastOneUserDLLNeedsSOUND=true;
				}

				// Next DLL
				if ( iAddDependentsLoop==0 && iPass==0 ) dwIndex++;
				pCurrent=pCurrent->GetNext();
			}

			// Restore dir before continue
			if (!prevPath.empty())
				std::filesystem::current_path(prevPath, ec);
		}

		// DLL index before adding any associations
		if ( iAddDependentsLoop==0 ) dwIndexBeforeAdds=dwIndex;
	}

	// link Basic3D
	if ( bAtLeastOneUserDLLNeeds3D )
	{
		DWORD dwTry=dwIndex+1;
		if(AddUniqueString("DBProBasic3DDebug.dll", &dwTry)) dwIndex=dwTry;
	}

	// link Sound
	if ( bAtLeastOneUserDLLNeedsSOUND )
	{
		DWORD dwTry=dwIndex+1;
		if(AddUniqueString("DBProSoundDebug.dll", &dwTry)) dwIndex=dwTry;
	}

	// Scan all DLLS, and add any that are link-associated
	CDataTable* pCurrent = this->GetNext();
	while(pCurrent)
	{
		// If DLLTable Entry has string..
		if(pCurrent->GetString())
		{
			// DLL Name contained in stringname
			DWORD dwTry = 0;
			LPCSTR pDLL = nullptr;
			LPSTR pDLLName = pCurrent->GetString()->GetStr();
// Register an associated DLL unless excluded (replaces the legacy TRY_DLL macro)
const auto tryDll = [&](LPCSTR dllName)
			{
				dwTry = dwIndex + 1;
				pDLL = dllName;
				if(NotExcluded(pDLL))
					if(AddUniqueString(pDLL, &dwTry))
						dwIndex = dwTry;
			};
			// Add other DLLs Associated With These..
			if(dbp::iequals(pDLLName, "DBProSetupDebug.dll"))
			{
				// Associate DLLs
				tryDll("DBProBasic2DDebug.dll");
				tryDll("DBProTextDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProTextDebug.dll"))
			{
				// Associate DLLs
				tryDll("DBProSetupDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProInputDebug.dll"))
			{
				// Checklist Support
				tryDll("DBProSystemDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProSpritesDebug.dll"))
			{
				// Image Support
				tryDll("DBProImageDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProBasic3DDebug.dll"))
			{
				// Image Support
				tryDll("DBProImageDebug.dll");
				// Transforms Support
				tryDll("DBProTransformsDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProBasic2DDebug.dll"))
			{
				// Minimal DirectX
				tryDll("DBProSetupDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProImageDebug.dll")
			|| dbp::iequals(pDLLName, "DBProAnimationDebug.dll")
			|| dbp::iequals(pDLLName, "DBProBitmapDebug.dll"))
			{
				// Sprite Support for pasting
				tryDll("DBProSpritesDebug.dll");

				// Minimal DirectX
				tryDll("DBProSetupDebug.dll");
				tryDll("DBProBasic2DDebug.dll");
				tryDll("DBProTextDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProMultiplayerDebug.dll"))
			{
				// Need access to memblock support
				tryDll("DBProMemblocksDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProMemblocksDebug.dll"))
			{
				// Memblocks Access to Bitmap, Image, Sound and Mesh
				tryDll("DBProBitmapDebug.dll");
				tryDll("DBProImageDebug.dll");
				tryDll("DBProSoundDebug.dll");
				tryDll("DBProBasic3DDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProCameraDebug.dll"))
			{
				tryDll("DBProSetupDebug.dll");
				tryDll("DBProImageDebug.dll");
				tryDll("DBProVectorsDebug.dll");
				tryDll("DBProTransformsDebug.dll");
				tryDll("DBProBasic3DDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProLightDebug.dll"))
			{
				tryDll("DBProSetupDebug.dll");
				tryDll("DBProCameraDebug.dll");
				tryDll("DBProVectorsDebug.dll");
				tryDll("DBProTransformsDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProMatrixDebug.dll"))
			{
				tryDll("DBProSetupDebug.dll");
				tryDll("DBProImageDebug.dll");
				tryDll("DBProCameraDebug.dll");
				tryDll("DBProVectorsDebug.dll");
				tryDll("DBProTransformsDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProBasic3DDebug.dll"))
			{
				// Primary Support
				tryDll("DBProSetupDebug.dll");
				tryDll("DBProImageDebug.dll");
				tryDll("DBProCameraDebug.dll");
				tryDll("DBProLightDebug.dll");
				tryDll("DBProTransformsDebug.dll");

				// Secondary Support
				tryDll("DBProVectorsDebug.dll");
				tryDll("ConvX.dll");
				tryDll("Conv3DS.dll");
				tryDll("ConvMDL.dll");
				tryDll("ConvMD2.dll");
				tryDll("ConvMD3.dll");
			}
			if(dbp::iequals(pDLLName, "DBProWorld3DDebug.dll") )
			{
				// Primary Support
				tryDll("DBProLODTerrainDebug.dll");
				tryDll("DBProQ2BSPDebug.dll");
				tryDll("DBProBasic3DDebug.dll");
				tryDll("DBProVectorsDebug.dll");
				tryDll("DBProTransformsDebug.dll");
				tryDll("DBProOwnBSPDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProLODTerrainDebug.dll") )
			{
				// Primary Support
				tryDll("DBProSetupDebug.dll");
				tryDll("DBProImageDebug.dll");
				tryDll("DBProCameraDebug.dll");
				tryDll("DBProTransformsDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProCSGDebug.dll") )
			{
				// Primary Support
				tryDll("DBProSetupDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProParticlesDebug.dll") )
			{
				// Primary Support
				tryDll("DBProParticlesDebug.dll");
				tryDll("DBProVectorsDebug.dll");
				tryDll("DBProTransformsDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProSystemDebug.dll") )
			{
				// for access to display mem
				tryDll("DBProSetupDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProVectorsDebug.dll") )
			{
				tryDll("DBProSetupDebug.dll");
			}
			if(dbp::iequals(pDLLName, "DBProTransformsDebug.dll"))
			{
				tryDll("DBProSetupDebug.dll");
			}
#undef TRY_DLL
		}

		// Next entry in DLL Table
		pCurrent=pCurrent->GetNext();
	}

	// Complete
	return (dwIndex-dwIndexBeforeAdds); 
}

// WriteDBM

bool CDataTable::WriteDBMHeader(DWORD dwKindOfTable)
{
	// Blank Line
	CStr strDBMBlank(1);
	if(g_pDBMWriter->OutputDBM(&strDBMBlank)==false) return false;

	// header Line
	CStr strDBMLine(256);
	if(dwKindOfTable==1) strDBMLine.SetText("STRING:");
	if(dwKindOfTable==2) strDBMLine.SetText("DATA:");
	if(dwKindOfTable==3) strDBMLine.SetText("DLLS:");
	if(dwKindOfTable==4) strDBMLine.SetText("COMMANDS:");
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	return true;
}

bool CDataTable::WriteDBM(void)
{
	// Write out text
	CStr strDBMLine(256);
	strDBMLine.SetText(">>");
	if(GetType()==1)
	{
		strDBMLine.AddNumericText(GetIndex());
		strDBMLine.AddText("=");
		strDBMLine.AddDoubleText(GetNumeric());
	}
	if(GetType()==2)
	{
		strDBMLine.AddNumericText(GetIndex());
		strDBMLine.AddText("=");
		strDBMLine.AddText(GetString());
	}

	// Output details
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// Write next one
	if(GetNext())
	{
		if((GetNext()->WriteDBM())==false) return false;
	}

	// Complete
	return true;
}

