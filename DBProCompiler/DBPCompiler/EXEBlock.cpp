//#include "stdafx.h"
// EXEBlock.cpp: implementation of the CEXEBlock class.
//#define INITGUID
#include "Error.h"
#include "StringUtils.h"
#include "EXEBlock.h"
#include "DataType.h"
#include "PluginRegistry.h"
#include "TextConvert.h"
#include "VFSHooks.h"
#include "MemoryPE.h"
#include "CoreRuntimeApi.h"
#include "SafeDLLLoading.h"
#include "DBPLogger.h"
#include "TargetABI.h"
#include "RuntimeDllTable.h"
#include <array>
#include <filesystem>
#include <limits>
#include <memory>
#include "direct.h"
#include "..\DBPCompilerEXE\resource.h"
#include ".\..\..\Dark Basic Public Shared\Dark basic Pro SDK\Shared\Core\globstruct.h"

// Prototype for associated GETDXVER.CPP (or empty true in MAIN.CPP if compiler)
HRESULT GetDXVersion( DWORD* pdwDirectXVersion, TCHAR* strDirectXVersion, int cchDirectXVersion );

namespace
{
// Program reference slots hold 64-bit target-ABI addresses.

bool WriteTargetAddress(uintptr_t* slot, const void* hostPointer, LPSTR* pReturnError, bool& bResult)
{
    const auto address = dbp::abi::FromHostAddress(reinterpret_cast<std::uintptr_t>(hostPointer));
    if (!address)
    {
        if (pReturnError != nullptr && *pReturnError == nullptr) *pReturnError = new char[1024];
        if (pReturnError != nullptr && *pReturnError != nullptr)
            snprintf(*pReturnError, 1024, "Host pointer %p does not fit the target address space", hostPointer);
        bResult = false;
        return false;
    }
    *slot = *address;
    return true;
}
} // namespace

// Internal Function Pointers For Core Management
GDI_RetVoidParamVoidPFN				g_CORE_Program;
GDI_RetVoidParamLPVOID				g_CORE_PassCmdLinePtr;
GDI_RetVoidParamLPVOID				g_CORE_PassErrorPtr;
GDI_RetVoidParamLPVOID				g_CORE_PassEscapePtr;
GDI_RetVoidParamLPVOID				g_CORE_PassBreakOutPtr;
GDI_RetVoidParamLPVOIDDWORD			g_CORE_PassStructurePatterns;
GDI_RetVoidParamLPSTR2				g_CORE_PassDataPtrs;
GDI_RetDWORDParamDWORD4HINSTLPSTRPFN	g_CORE_InitDisplay;
GDI_RetVoidParamVoidPFN				g_CORE_PassDLLs;
GDI_RetVoidParamVoidPFN				g_CORE_ConstructDLLs;
GDI_RetLPVOIDParamVoidPFN			g_CORE_GetGlobPtr;

GDI_RetDWORDParamVoidPFN			g_CORE_CloseDisplay;
GDI_CreateSpacePFN					g_CORE_CreateVarSpace;
GDI_RetVoidParamVoidPFN				g_CORE_DeleteVarSpace;
GDI_CreateSpacePFN					g_CORE_CreateDataSpace;
GDI_RetVoidParamVoidPFN				g_CORE_DeleteDataSpace;
GDI_RetVoidParamDWORDPTRPFN			g_CORE_DeleteVarItem;
GDI_RetDWORDPTRParamDWORDPTRPFN		g_CORE_UnDim;
GDI_RetVoidParamVoidPFN				g_CORE_SyncRefresh;
GDI_RetIntParamVoidPFN				g_CORE_GetSecurityCode;
GDI_RetVoidParamVoidPFN				g_CORE_WipeSecurityCode;

// Internal Function Pointers For Transforms Management
DLL_Constructor						g_Transforms_Constructor;
DLL_Destructor						g_Transforms_Destructor;
DLL_Update							g_Transforms_Update;

// Internal Function Pointers For Sprite Management
SPRITES_RetVoidParamHINSTANCE2PFN	g_Sprites_Constructor;
SPRITES_RetVoidParamVoidPFN			g_Sprites_Destructor;
SPRITES_RetVoidParamVoidPFN			g_Sprites_Update;

// Internal Function Pointers For Image Management
IMAGE_RetVoidParamVoidPFN			g_Image_Constructor;				// constructor
IMAGE_RetVoidParamVoidPFN			g_Image_Destructor;					// destructor

// Global DLL Storage
HINSTANCE							hDLLMod[256];
bool								bDLLTPC[256];

// Global Variables
bool								g_bSuccessfulDLLLinks			= false;
// Guard cells accessed by generated code with 64-bit (REX.W) operand
// size - they must be native 64-bit values (see CEXEBlock members for the
// matching compiler-side cells).
DWORD_PTR							g_dwEscapeValueMem				= 0;
DWORD_PTR							g_dwBreakOutPosition			= 0;
LPSTR								g_pVarSpaceAddressInUse			= nullptr;
DWORD								g_dwVarSpaceSizeInUse			= 0;
bool								g_bIsInternalDebugger			= false;
PROCESS_INFORMATION					g_InternalDebuggerProcessInfo;
HANDLE								g_hLastGFXPointer				= nullptr;

// Store DirectX version for globstruct transfer (so core knows what we have - 080306)
DWORD								g_dwDirectXVersion				= 0;
TCHAR								g_strDirectXVersion[10]			= { 0 };

// Global Shared Data Pointer (passed in from core)
GlobStruct*							g_pGlob							= nullptr;

// If linked from DarkEXE (temp loading... window)
extern HWND g_hTempWindow;
extern HWND g_igLoader_HWND;

#ifdef DEMOPROTECTEDMODE
bool WriteStringToRegistry(char* PerfmonNamesKey, char* valuekey, char* string)
{
	HKEY hKeyNames = 0;
	DWORD Status;
	DWORD dwDisposition;
	const char* ObjectType = "Num";
	Status = RegCreateKeyEx(HKEY_CURRENT_USER, PerfmonNamesKey, 0L, const_cast<LPSTR>(ObjectType), REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | KEY_WRITE, nullptr, &hKeyNames, &dwDisposition);
	if(dwDisposition==REG_OPENED_EXISTING_KEY)
	{
		RegCloseKey(hKeyNames);
		Status = RegOpenKeyEx(HKEY_CURRENT_USER, PerfmonNamesKey, 0L, KEY_WRITE, &hKeyNames);
	}
    if(Status==ERROR_SUCCESS)
	{
        Status = RegSetValueEx(hKeyNames, valuekey, 0, REG_SZ, (LPBYTE)string, (strlen(string)+1)*sizeof(char));
	}
	RegCloseKey(hKeyNames);
	hKeyNames=0;
	return true;
}
void ReadStringFromRegistry(char* PerfmonNamesKey, char* valuekey, char* string)
{
	HKEY hKeyNames = 0;
	DWORD Status;
	const char* ObjectType = "Num";
	DWORD Datavalue = 0;

	string[0] = '\0';
	Status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, PerfmonNamesKey, 0L, KEY_READ, &hKeyNames);
    if(Status==ERROR_SUCCESS)
	{
		DWORD Type=REG_SZ;
		DWORD Size=256;
		Status = RegQueryValueEx(hKeyNames, valuekey, nullptr, &Type, nullptr, &Size);
		if(Size<255)
			RegQueryValueEx(hKeyNames, valuekey, nullptr, &Type, (LPBYTE)string, &Size);

		RegCloseKey(hKeyNames);
	}
}
#endif

CEXEBlock::CEXEBlock()
{
	// Reset flags
	m_bEXEBlockPresent=false;

	// Settings
	m_dwInitialDisplayMode=1;
	m_dwInitialDisplayWidth=0;
	m_dwInitialDisplayHeight=0;
	m_dwInitialDisplayDepth=0;
	m_pInitialAppName=nullptr;

	m_OriginalFolderName.clear();
	m_UnpackFolderName.clear();
	m_AbsoluteAppFile.clear();

	// DLL Data
	m_dwNumberOfDLLs=0;
	m_pDLLIndexArray=nullptr;
	m_pDLLFilenameArray=nullptr;
	m_pDLLLoadedAlreadyArray=nullptr;

	// MCB Reference Data
	m_dwNumberOfReferences=0;
	m_pRefArray=nullptr;
	m_pRefTypeArray=nullptr;
	m_pRefIndexArray=nullptr;
	m_pRefWidthArray=nullptr;
	m_pRefRelEndArray=nullptr;

	// Clear runtime error dword
	m_dwRuntimeErrorDWORD=0;
	m_dwRuntimeErrorLineDWORD=0;

	// Runtime string array database
	m_dwNumberOfRuntimeErrorStrings=0;
	m_pRuntimeErrorStringsArray=nullptr;

	// Machine Code Block (MCB)
	m_dwSizeOfMCB=0;
	m_pMachineCodeBlock=nullptr;
	m_dwStartOfMiniMC=0;

	// Commands Data
	m_dwNumberOfCommands=0;
	m_pCommandDLLIdArray=nullptr;
	m_pCommandDLLCallArray=nullptr;

	// Strings Data
	m_dwNumberOfStrings=0;
	m_pStringsArray=nullptr;

	// Data Statements Data
	m_dwNumberOfDataItems=0;
	m_pDataArray=nullptr;
	m_pDataStringsArray=nullptr;

	// Variable Space Data
	m_dwVariableSpaceSize=0;
	m_pVariableSpace=nullptr;

	// Data Space Data
	m_dwDataSpaceSize=0;
	m_pDataSpace=nullptr;

	// Record Dynamic Variables for auto-freeing
	m_dwDynamicVarsQuantity=0;
	m_pDynamicVarsArray=nullptr;
	m_pDynamicVarsArrayType=nullptr;

	// Record UserTypeStringPatterns - reactivated for U71 (store structure types)
	m_dwUsertypeStringPatternQuantity=0;
	m_pUsertypeStringPatternArray=nullptr;
}

CEXEBlock::~CEXEBlock()
{
	Clear();
}

void CEXEBlock::Clear(void)
{
	// Release appname (allocated with new char[])
	SafeDeleteArray(m_pInitialAppName);

	// Release exefile ptrs (RAII handles strings)
	m_OriginalFolderName.clear();
	m_UnpackFolderName.clear();
	m_AbsoluteAppFile.clear();

	// Release DLLs Data (arrays allocated with new[])
	if ( m_pDLLFilenameArray ) DeleteArrayContents(m_pDLLFilenameArray,m_dwNumberOfDLLs);
	SafeDeleteArray(m_pDLLFilenameArray);
	SafeDeleteArray(m_pDLLIndexArray);
	SafeDeleteArray(m_pDLLLoadedAlreadyArray);

	// Release Ref Data
	SafeDeleteArray(m_pRefArray);
	SafeDeleteArray(m_pRefIndexArray);
	SafeDeleteArray(m_pRefTypeArray);
	SafeDeleteArray(m_pRefWidthArray);
	SafeDeleteArray(m_pRefRelEndArray);

	// Release MCB Data 9leeadd - 090305 - DEP release)
	VirtualFree ( m_pMachineCodeBlock, 0, MEM_DECOMMIT | MEM_RELEASE );
	m_pMachineCodeBlock = nullptr;

	// Release Runtime Error Strings Database
	if ( m_pRuntimeErrorStringsArray ) DeleteArrayContents(m_pRuntimeErrorStringsArray,m_dwNumberOfRuntimeErrorStrings);
	SafeDeleteArray(m_pRuntimeErrorStringsArray);

	// Release Commands Data
	if ( m_pCommandDLLCallArray ) DeleteArrayContents(m_pCommandDLLCallArray,m_dwNumberOfCommands);
	SafeDeleteArray(m_pCommandDLLCallArray);
	SafeDeleteArray(m_pCommandDLLIdArray);

	// Release Strings Data
	if ( m_pStringsArray ) DeleteArrayContents(m_pStringsArray,m_dwNumberOfStrings);
	SafeDeleteArray(m_pStringsArray);

	// Release Data Data
	SafeDeleteArray(m_pDataArray);
	if ( m_pDataStringsArray) DeleteArrayContents(m_pDataStringsArray,m_dwNumberOfDataItems);
	SafeDeleteArray(m_pDataStringsArray);

	// Release Dynamic Variable Offset Array
	SafeDeleteArray(m_pDynamicVarsArray);
	SafeDeleteArray(m_pDynamicVarsArrayType);

	// Release Structure Pattern Array
	SafeDeleteArray(m_pUsertypeStringPatternArray);

	// Reset all values so no consumer can walk a stale count over a null array
	m_bEXEBlockPresent=false;
	m_dwNumberOfDLLs=0;
	m_dwNumberOfReferences=0;
	m_dwSizeOfMCB=0;
	m_dwNumberOfCommands=0;
	m_dwNumberOfStrings=0;
	m_dwNumberOfRuntimeErrorStrings=0;
	m_dwNumberOfDataItems=0;
	m_dwDynamicVarsQuantity=0;
	m_dwUsertypeStringPatternQuantity=0;
	m_dwVariableSpaceSize=0;
	m_dwDataSpaceSize=0;
}

DWORD* CEXEBlock::CreateArray(DWORD dwCount,DWORD dwType)
{
	// leeadd - 090305 - DEP or regular flavour
	DWORD* pArray = nullptr;
	if ( dwType==PAGE_READWRITE )
	{
		// data block used for MCB (allocated RW, later transitioned to RX via VirtualProtect)
		pArray = (DWORD*) VirtualAlloc ( nullptr, dwCount*sizeof(DWORD), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
	}
	else
	{
		// regular data block (make_unique<T[]> value-initialises to zero,
		// matching the legacy hand-rolled clear loop; ownership is released
		// back to the raw-pointer member contract expected by callers)
		pArray = std::make_unique<DWORD[]>(dwCount).release();
	}

	// return ptr
	return pArray;
}

DWORD* CEXEBlock::CreateArray(DWORD dwCount)
{
	return CreateArray(dwCount,0);
}

uintptr_t* CEXEBlock::CreatePtrArray(DWORD dwCount)
{
	// make_unique<T[]> value-initialises every slot to zero; release the
	// ownership to honour the raw-pointer member contract of callers
	return std::make_unique<uintptr_t[]>(dwCount).release();
}

bool CEXEBlock::RecreateArray(uintptr_t** pArray, DWORD dwCount, DWORD NewCount)
{
	if(pArray)
	{
		// New buffer is value-initialised (zeroed) by make_unique<T[]>
		auto pNewOwner = std::make_unique<uintptr_t[]>(NewCount);

		// Copy Old to New
		if(*pArray && dwCount > 0)
			memcpy(pNewOwner.get(), *pArray, dwCount*sizeof(uintptr_t));

		// Delete Old (pointer array only) via RAII, then switch pointers
		std::unique_ptr<uintptr_t[]> pOldOwner(*pArray);
		*pArray=pNewOwner.release();

		return true;
	}

	// Soft fail
	return false;
}

bool CEXEBlock::RecreateArray(DWORD** pArray, DWORD dwCount, DWORD NewCount)
{
	if(pArray)
	{
		// New buffer is value-initialised (zeroed) by make_unique<T[]>
		auto pNewOwner = std::make_unique<DWORD[]>(NewCount);

		// Copy Old to New
		memcpy(pNewOwner.get(), *pArray, dwCount*sizeof(DWORD));

		// Delete Old (pointer array only) via RAII, then switch pointers
		std::unique_ptr<DWORD[]> pOldOwner(*pArray);
		*pArray=pNewOwner.release();

		return true;
	}

	// Soft fail
	return false;
}

void CEXEBlock::DeleteArrayContents(uintptr_t* pArray, DWORD dwCount)
{
	if(pArray)
	{
		for(DWORD i=0; i<dwCount; i++)
		{
			if(*(pArray+i))
			{
				delete[] (char*)*(pArray+i);
				*(pArray+i)=0;
			}
		}
	}
}

namespace {
static std::string exe_get_filename_only(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}
}

bool CEXEBlock::FileExists(LPSTR pFilename)
{
	if (!pFilename) return false;
	if (VFSRegistry::Exists(exe_get_filename_only(pFilename))) return true;
	return std::filesystem::exists(pFilename);
}

bool CEXEBlock::Save(char* lpFilename)
{
	// [DIAG] Optional reference-table dump for crash diagnosis (DBP_DUMP_REFS=1)
	{
		const DWORD dumpEnabled = GetEnvironmentVariableA("DBP_DUMP_REFS", nullptr, 0);
		if (dumpEnabled != 0)
		{
			const std::string dumpPath = std::string(lpFilename) + ".refs.txt";
			FILE* dump = nullptr;
			if (fopen_s(&dump, dumpPath.c_str(), "w") == 0 && dump != nullptr)
			{
				fprintf(dump, "refs=%lu strings=%lu commands=%lu mcb=%lu\n",
					static_cast<unsigned long>(m_dwNumberOfReferences),
					static_cast<unsigned long>(m_dwNumberOfStrings),
					static_cast<unsigned long>(m_dwNumberOfCommands),
					static_cast<unsigned long>(m_dwSizeOfMCB));
				for (DWORD ref = 0; ref < m_dwNumberOfReferences; ref++)
				{
					const DWORD pos = m_pRefArray[ref];
					const DWORD type = m_pRefTypeArray[ref];
					const unsigned long long index = m_pRefIndexArray[ref];
					const DWORD width = (m_pRefWidthArray != nullptr) ? m_pRefWidthArray[ref] : 8u;
					const DWORD relEnd = (m_pRefRelEndArray != nullptr) ? m_pRefRelEndArray[ref] : 0u;
					fprintf(dump, "ref[%lu] pos=0x%08X type=%lu index=0x%016llX width=%lu relEnd=0x%08X",
						static_cast<unsigned long>(ref), pos,
						static_cast<unsigned long>(type), index,
						static_cast<unsigned long>(width),
						static_cast<unsigned long>(relEnd));
					if (type == 2 && index < m_dwNumberOfStrings && m_pStringsArray != nullptr)
					{
						const char* text = reinterpret_cast<const char*>(m_pStringsArray[index]);
						if (text != nullptr)
							fprintf(dump, " str=\"%.60s\"", text);
					}
					fprintf(dump, "\n");
				}
				// Overlap detection: two refs whose [pos, pos+width) intersect.
				for (DWORD a = 0; a < m_dwNumberOfReferences; a++)
				{
					const DWORD pa = m_pRefArray[a];
					const DWORD wa = (m_pRefWidthArray != nullptr) ? m_pRefWidthArray[a] : 8u;
					for (DWORD b = a + 1; b < m_dwNumberOfReferences; b++)
					{
						const DWORD pb = m_pRefArray[b];
						const DWORD wb = (m_pRefWidthArray != nullptr) ? m_pRefWidthArray[b] : 8u;
						if (pa < pb + wb && pb < pa + wa)
							fprintf(dump, "OVERLAP ref[%lu](pos=0x%X,w=%lu,t=%lu) ref[%lu](pos=0x%X,w=%lu,t=%lu)\n",
								static_cast<unsigned long>(a), pa, static_cast<unsigned long>(wa),
								static_cast<unsigned long>(m_pRefTypeArray[a]),
								static_cast<unsigned long>(b), pb, static_cast<unsigned long>(wb),
								static_cast<unsigned long>(m_pRefTypeArray[b]));
					}
				}
				fclose(dump);
			}
		}
	}

	const auto outputPath = TextConvert::UTF8ToUTF16(lpFilename);
	HANDLE hFile = CreateFileW(outputPath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(hFile!=INVALID_HANDLE_VALUE)
	{
		bool success = true;
		const auto record = [&success](const bool result) noexcept {
			success = result && success;
		};

		// Settings
		record(SaveValue(hFile, &m_dwInitialDisplayMode));
		record(SaveValue(hFile, &m_dwInitialDisplayWidth));
		record(SaveValue(hFile, &m_dwInitialDisplayHeight));
		record(SaveValue(hFile, &m_dwInitialDisplayDepth));

		// The on-disk format reserves exactly 256 bytes for the application
		// name. Serialize through a bounded, zero-padded block instead of reading
		// 256 bytes from a variably-sized heap string.
		std::array<char, 256> appName{};
		if (m_pInitialAppName != nullptr)
			strncpy_s(appName.data(), appName.size(), m_pInitialAppName, _TRUNCATE);
		DWORD appNameLength = static_cast<DWORD>(appName.size());
		auto* appNameData = reinterpret_cast<DWORD*>(appName.data());
		record(SaveValueArrayBytes(hFile, reinterpret_cast<void**>(&appNameData), &appNameLength));

		// DLL Data
		record(SaveValue(hFile, &m_dwNumberOfDLLs));
		record(SaveValueArray(hFile, &m_pDLLIndexArray, &m_dwNumberOfDLLs));
		record(SaveStringArray(hFile, &m_pDLLFilenameArray, &m_dwNumberOfDLLs));
		record(SaveValueArray(hFile, &m_pDLLLoadedAlreadyArray, &m_dwNumberOfDLLs));

		// MCB Reference Data
		record(SaveValue(hFile, &m_dwNumberOfReferences));
		record(SaveValueArray(hFile, &m_pRefArray, &m_dwNumberOfReferences));
		record(SaveValueArray(hFile, &m_pRefTypeArray, &m_dwNumberOfReferences));
		record(SaveValueArray(hFile, &m_pRefIndexArray, &m_dwNumberOfReferences));
		record(SaveValueArray(hFile, &m_pRefWidthArray, &m_dwNumberOfReferences));
		record(SaveValueArray(hFile, &m_pRefRelEndArray, &m_dwNumberOfReferences));

		// Runtime Error String Database
		record(SaveValue(hFile, &m_dwNumberOfRuntimeErrorStrings));
		record(SaveStringArray(hFile, &m_pRuntimeErrorStringsArray, &m_dwNumberOfRuntimeErrorStrings));

		// Machine Code Block (MCB)
		record(SaveValue(hFile, &m_dwSizeOfMCB));
                record(SaveValueArrayBytes(hFile, reinterpret_cast<void**>(&m_pMachineCodeBlock), &m_dwSizeOfMCB));

		// Commands Data
		record(SaveValue(hFile, &m_dwNumberOfCommands));
		record(SaveValueArray(hFile, &m_pCommandDLLIdArray, &m_dwNumberOfCommands));
		record(SaveStringArray(hFile, &m_pCommandDLLCallArray, &m_dwNumberOfCommands));

		// Strings Data
		record(SaveValue(hFile, &m_dwNumberOfStrings));
		record(SaveStringArray(hFile, &m_pStringsArray, &m_dwNumberOfStrings));

		// Data Data
		record(SaveValue(hFile, &m_dwNumberOfDataItems));
		record(SaveBlock(hFile, &m_pDataArray, m_dwNumberOfDataItems*10));
		record(SaveStringArray(hFile, &m_pDataStringsArray, &m_dwNumberOfDataItems));

		// Variable Space Data
		record(SaveValue(hFile, &m_dwVariableSpaceSize));

		// Data Space Data
		record(SaveValue(hFile, &m_dwDataSpaceSize));

		// Dynamic Variable Offset Data
		record(SaveValue(hFile, &m_dwDynamicVarsQuantity));
		record(SaveValueArray(hFile, &m_pDynamicVarsArray, &m_dwDynamicVarsQuantity));
		record(SaveValueArray(hFile, &m_pDynamicVarsArrayType, &m_dwDynamicVarsQuantity));

		// Usertype String Patterns - reactivated for U71 (store structure types)
		record(SaveValue(hFile, &m_dwUsertypeStringPatternQuantity));
		record(SaveBlock(hFile, &m_pUsertypeStringPatternArray, m_dwUsertypeStringPatternQuantity));

		// Close file
		success = CloseHandle(hFile) != FALSE && success;
		if (!success)
			DeleteFileW(outputPath.c_str());
		return success;
	}
	else
	{
		// EXEBlock shared - silent fail
		return false;
	}
}

bool CEXEBlock::SaveValue(HANDLE hFile, DWORD* Value)
{
	DWORD bytes=0;
	return Value != nullptr &&
		WriteFile(hFile, Value, sizeof(DWORD), &bytes, nullptr) != FALSE &&
		bytes == sizeof(DWORD);
}

bool CEXEBlock::SaveBlock(HANDLE hFile, LPSTR* pMem, DWORD dwSize)
{
	DWORD bytes=0;
	if (dwSize == 0) return true;
	return pMem != nullptr && *pMem != nullptr &&
		WriteFile(hFile, *pMem, dwSize, &bytes, nullptr) != FALSE &&
		bytes == dwSize;
}

bool CEXEBlock::SaveValueArray(HANDLE hFile, DWORD** pArray, DWORD* Count)
{
	DWORD bytes=0;
	if (pArray == nullptr || Count == nullptr) return false;
	if (*Count == 0) return true;
	if (*Count > (std::numeric_limits<DWORD>::max)() / sizeof(DWORD))
		return false;
        const DWORD expected = (*Count) * sizeof(DWORD);
        return *pArray != nullptr &&
                WriteFile(hFile, *pArray, expected, &bytes, nullptr) != FALSE &&
                bytes == expected;
}

bool CEXEBlock::SaveValueArray(HANDLE hFile, std::uint64_t** pArray, DWORD* Count)
{
	DWORD bytes=0;
	if (pArray == nullptr || Count == nullptr) return false;
	if (*Count == 0) return true;
	if (*Count > (std::numeric_limits<DWORD>::max)() / sizeof(std::uint64_t))
		return false;
	const DWORD expected = (*Count) * sizeof(std::uint64_t);
	return *pArray != nullptr &&
		WriteFile(hFile, *pArray, expected, &bytes, nullptr) != FALSE &&
		bytes == expected;
}

bool CEXEBlock::SaveValueArrayBytes(HANDLE hFile, void** pArray, DWORD* Count)
{
	DWORD bytes=0;
	if (pArray == nullptr || Count == nullptr) return false;
	if (*Count == 0) return true;
	return *pArray != nullptr &&
		WriteFile(hFile, *pArray, *Count, &bytes, nullptr) != FALSE &&
		bytes == *Count;
}

bool CEXEBlock::SaveStringArray(HANDLE hFile, uintptr_t** pArray, DWORD* Count)
{
	DWORD bytes=0;
	if (pArray == nullptr || Count == nullptr) return false;
	if (*Count == 0) return true;
	bool bResult = *pArray != nullptr;
	if(bResult)
	{
		for(DWORD index=0; index<*Count; index++)
		{
			char* pStr = (char*)*(*pArray+index);
			DWORD length = 0;
			if (pStr)
			{
				const auto sourceLength = strlen(pStr);
				if (sourceLength > (std::numeric_limits<DWORD>::max)())
					return false;
				length = static_cast<DWORD>(sourceLength);
			}
			if (WriteFile(hFile, &length, sizeof(length), &bytes, nullptr) == FALSE ||
				bytes != sizeof(length))
				bResult=false;
			if(pStr)
			{
				// Write number of bytes in string
				if(length>0)
				{
					// Write string if has a length
					if (WriteFile(hFile, pStr, length, &bytes, nullptr) == FALSE ||
						bytes != length)
						bResult=false;
				}
			}
		}
	}
	return bResult;
}

bool CEXEBlock::StartInfo(LPSTR pUnpackFolderName, DWORD dwEncryptionKey)
{
	// Set Unpack Folder (and copy to global data)
	m_UnpackFolderName = pUnpackFolderName;
	m_dwEncryptionKey = dwEncryptionKey;

	// Complete
	return true;
}

bool CEXEBlock::Load(char* lpFilename)
{
        Clear();

	// Load EXE Filedata
	HANDLE hFile = Hook_CreateFileW(TextConvert::UTF8ToUTF16(lpFilename).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(hFile!=INVALID_HANDLE_VALUE)
	{
                bool success = true;
                const auto record = [&success](const bool result) noexcept {
                        success = result && success;
                };

		// Settings
                record(LoadValue(hFile, &m_dwInitialDisplayMode));
                record(LoadValue(hFile, &m_dwInitialDisplayWidth));
                record(LoadValue(hFile, &m_dwInitialDisplayHeight));
                record(LoadValue(hFile, &m_dwInitialDisplayDepth));

		// AppName String
		DWORD dwLength=256;
                record(LoadValueArrayBytes(hFile, reinterpret_cast<void**>(&m_pInitialAppName), &dwLength));

		// DLL Data
                record(LoadValue(hFile, &m_dwNumberOfDLLs));
                record(LoadValueArray(hFile, &m_pDLLIndexArray, &m_dwNumberOfDLLs));
                record(LoadStringArray(hFile, &m_pDLLFilenameArray, &m_dwNumberOfDLLs));
                record(LoadValueArray(hFile, &m_pDLLLoadedAlreadyArray, &m_dwNumberOfDLLs));
                if (m_pDLLLoadedAlreadyArray != nullptr && m_dwNumberOfDLLs > 0)
                        ZeroMemory(m_pDLLLoadedAlreadyArray, sizeof(DWORD)*m_dwNumberOfDLLs);
		
		// MCB Reference Data
                record(LoadValue(hFile, &m_dwNumberOfReferences));
                record(LoadValueArray(hFile, &m_pRefArray, &m_dwNumberOfReferences));
                record(LoadValueArray(hFile, &m_pRefTypeArray, &m_dwNumberOfReferences));                record(LoadValueArray(hFile, &m_pRefIndexArray, &m_dwNumberOfReferences));
                record(LoadValueArray(hFile, & m_pRefWidthArray, &m_dwNumberOfReferences));
                record(LoadValueArray(hFile, & m_pRefRelEndArray, &m_dwNumberOfReferences));

		// Runtime Error String Database
                record(LoadValue(hFile, &m_dwNumberOfRuntimeErrorStrings));
                record(LoadStringArray(hFile, &m_pRuntimeErrorStringsArray, &m_dwNumberOfRuntimeErrorStrings));

		// Machine Code Block (MCB)
		// leeadd - 090305 - added flag for DEP protection allowance
                record(LoadValue(hFile, &m_dwSizeOfMCB));
                record(LoadValueArrayBytes(hFile, reinterpret_cast<void**>(&m_pMachineCodeBlock), &m_dwSizeOfMCB, PAGE_READWRITE));

		// Commands Data
                record(LoadValue(hFile, &m_dwNumberOfCommands));
                record(LoadValueArray(hFile, &m_pCommandDLLIdArray, &m_dwNumberOfCommands));
                record(LoadStringArray(hFile, &m_pCommandDLLCallArray, &m_dwNumberOfCommands));

		// Strings Data
                record(LoadValue(hFile, &m_dwNumberOfStrings));
                record(LoadStringArray(hFile, &m_pStringsArray, &m_dwNumberOfStrings));

		// Data Data
                record(LoadValue(hFile, &m_dwNumberOfDataItems));
                record(LoadBlock(hFile, &m_pDataArray, m_dwNumberOfDataItems*10));
                record(LoadStringArray(hFile, &m_pDataStringsArray, &m_dwNumberOfDataItems));

		// Variable Space Data
                record(LoadValue(hFile, &m_dwVariableSpaceSize));

		// Data Space Data
                record(LoadValue(hFile, &m_dwDataSpaceSize));

		// Dynamic Variable Offset Data
                record(LoadValue(hFile, &m_dwDynamicVarsQuantity));
                record(LoadValueArray(hFile, &m_pDynamicVarsArray, &m_dwDynamicVarsQuantity));
                record(LoadValueArray(hFile, &m_pDynamicVarsArrayType, &m_dwDynamicVarsQuantity));

		// Usertype String Patterns - reactivated for U71 (store structure types)
                record(LoadValue(hFile, &m_dwUsertypeStringPatternQuantity));
                record(LoadBlock(hFile, &m_pUsertypeStringPatternArray, m_dwUsertypeStringPatternQuantity));

		// Close file
		Hook_CloseHandle(hFile);
                if (!success)
                        Clear();
                return success;
	}
	else
	{
		// EXEBlock shared - silent fail
		return false;
	}
}

bool CEXEBlock::LoadValue(HANDLE hFile, DWORD* Value)
{
	DWORD bytes=0;
        return Value != nullptr &&
                Hook_ReadFile(hFile, Value, sizeof(DWORD), &bytes, nullptr) != FALSE &&
                bytes == sizeof(DWORD);
}

bool CEXEBlock::LoadBlock(HANDLE hFile, LPSTR* pMem, DWORD dwSize)
{
	DWORD bytes=0;
        if (dwSize == 0) return true;
        if (pMem == nullptr) return false;
	// Allocate via make_unique<char[]> then release into the raw-pointer
	// out-param owned/freed by the caller (Clear via SAFE_DELETE_ARRAY)
	*pMem = std::make_unique<char[]>(dwSize+1).release();
        return *pMem != nullptr &&
                Hook_ReadFile(hFile, *pMem, dwSize, &bytes, nullptr) != FALSE &&
                bytes == dwSize;
}

bool CEXEBlock::LoadValueArray(HANDLE hFile, DWORD** pArray, DWORD* Count)
{
	DWORD bytes=0;
        if (pArray == nullptr || Count == nullptr) return false;
        if (*Count == 0) return true;

        // Create Array
        *pArray = CreateArray(*Count,0);

        // Read data into Array
        const DWORD expected = (*Count) * sizeof(DWORD);
        return *pArray != nullptr &&
                Hook_ReadFile(hFile, *pArray, expected, &bytes, nullptr) != FALSE &&
                bytes == expected;
}

bool CEXEBlock::LoadValueArray(HANDLE hFile, std::uint64_t** pArray, DWORD* Count)
{
	DWORD bytes=0;
        if (pArray == nullptr || Count == nullptr) return false;
        if (*Count == 0) return true;

        // Create Array (8-byte elements)
        *pArray = std::make_unique<std::uint64_t[]>(*Count).release();

        // Read data into Array
        const DWORD expected = (*Count) * sizeof(std::uint64_t);
        return *pArray != nullptr &&
                Hook_ReadFile(hFile, *pArray, expected, &bytes, nullptr) != FALSE &&
                bytes == expected;
}

bool CEXEBlock::LoadValueArrayBytes(HANDLE hFile, void** pArray, DWORD* Count, DWORD dwType)
{
	DWORD bytes=0;
        if (pArray == nullptr || Count == nullptr) return false;
        if (*Count == 0) return true;

        // Create Array
        *pArray = CreateArray(*Count,dwType);

        // Read data into Array
        return *pArray != nullptr &&
                Hook_ReadFile(hFile, *pArray, (*Count), &bytes, nullptr) != FALSE &&
                bytes == *Count;
}

bool CEXEBlock::LoadValueArrayBytes(HANDLE hFile, void** pArray, DWORD* Count)
{
	return LoadValueArrayBytes(hFile, pArray, Count, 0);
}


bool CEXEBlock::LoadStringArray(HANDLE hFile, uintptr_t** pArray, DWORD* Count)
{
	DWORD bytes=0;
        if (pArray == nullptr || Count == nullptr) return false;
        if (*Count == 0) return true;
        bool bResult=true;
        if(*Count>0)
	{
		// Create Array 
		*pArray = CreatePtrArray(*Count);
                if (*pArray == nullptr) return false;

		// Read strings into Array of strings
		for(DWORD index=0; index<*Count; index++)
		{
			// Read length of string
			DWORD length = 0;
                        if (Hook_ReadFile(hFile, &length, sizeof(DWORD), &bytes, nullptr) == FALSE ||
                                bytes != sizeof(DWORD))
                                return false;
			char* pStr = std::make_unique<char[]>(length+1).release();
			if(length>0)
			{
                                if (Hook_ReadFile(hFile, pStr, length, &bytes, nullptr) == FALSE ||
                                        bytes != length)
                                        bResult=false;
			}
			pStr[length]=0;
			*(*pArray+index) = (uintptr_t)pStr;
		}
	}
	return bResult;
}

bool CEXEBlock::Init(HINSTANCE hInstance, bool bResult, LPSTR* pReturnError, LPSTR pCmdLine)
{
	bResult=InitDebug(hInstance, nullptr, nullptr, nullptr, bResult, pReturnError, pCmdLine, false);
	return bResult;
}

bool CEXEBlock::InitMini(LPVOID pDHookS, LPVOID pDHookJ, LPVOID pDHookR, bool bResult, LPSTR* pReturnError)
{
	bResult=InitDebug(nullptr, pDHookS, pDHookJ, pDHookR, bResult, pReturnError, nullptr, true);
	return bResult;
}

bool CEXEBlock::CheckIfGotLatestDirectX ([[maybe_unused]] bool bSilent)
{
	// Modern Windows 10/11 natively supports DirectX runtime execution.
	return true;
}

bool CEXEBlock::InitDebug(HINSTANCE hInstance, LPVOID pDHookS, LPVOID pDHookJ, LPVOID pDHookR, bool bResult, LPSTR* pReturnError, LPSTR pCmdLine, bool bMiniInit)
{
	static const char *const pCoreName = "dbprocore.dll";

	// [EXE] Ensure dbprocore.dll is always present
	if(m_dwNumberOfDLLs==0)
	{
		// Make Core Entry
		m_dwNumberOfDLLs=1;
		m_pDLLIndexArray = CreateArray(1);
		m_pDLLIndexArray[0]=1;
		m_pDLLLoadedAlreadyArray = CreateArray(1);
		m_pDLLLoadedAlreadyArray[0] = 0;

		m_pDLLFilenameArray = CreatePtrArray(1);
		m_pDLLFilenameArray[0] = (uintptr_t)std::make_unique<char[]>(strlen(pCoreName)+1).release();
		snprintf((char*)m_pDLLFilenameArray[0], strlen(pCoreName)+1, "%s", pCoreName);
	}

	// The package stores DLL identifiers as DWORD values, while the legacy
	// runtime dispatch tables have exactly 256 slots. Validate the complete
	// package contract before converting or indexing any value.
	if (m_dwNumberOfDLLs > dbp::runtime::DllCapacity ||
		m_pDLLIndexArray == nullptr ||
		m_pDLLFilenameArray == nullptr ||
		m_pDLLLoadedAlreadyArray == nullptr)
	{
		if (*pReturnError == nullptr) *pReturnError = new char[1024];
		sprintf_s(
			*pReturnError,
			1024,
			"Executable DLL table is invalid (count=%lu, indexes=%s, filenames=%s, loaded-state=%s)",
			static_cast<unsigned long>(m_dwNumberOfDLLs),
			m_pDLLIndexArray != nullptr ? "present" : "missing",
			m_pDLLFilenameArray != nullptr ? "present" : "missing",
			m_pDLLLoadedAlreadyArray != nullptr ? "present" : "missing");
		return false;
	}
	for (DWORD dll = 0; dll < m_dwNumberOfDLLs; ++dll)
	{
		const DWORD dllIndex = m_pDLLIndexArray[dll];
		if (!dbp::runtime::IsDllIndex(dllIndex) || m_pDLLFilenameArray[dll] == 0)
		{
			if (*pReturnError == nullptr) *pReturnError = new char[1024];
			sprintf_s(
				*pReturnError,
				1024,
				"Executable DLL entry %lu has invalid runtime index %lu",
				static_cast<unsigned long>(dll),
				static_cast<unsigned long>(dllIndex));
			return false;
		}
	}

	// [EXE] - Detect if using Basic3D.DLL and if so, check for DX9!
	bool bBasic3DIsUsedSoWeNeedDirectXCheck = false;
	if ( m_dwNumberOfDLLs>0 )
	{
		for(DWORD dll=0; dll<m_dwNumberOfDLLs; dll++)
		{
			const DWORD dllindex=m_pDLLIndexArray[dll];
			if(dbp::runtime::IsDllIndex(dllindex))
			{
				// Load the DLL into memory
				LPSTR pDLLName = (LPSTR)m_pDLLFilenameArray[dll];
				if(dbp::iequals(pDLLName, "DBProBasic3DDebug.dll"))
				{
					// DX used, so we make sure we have DirectX
					bBasic3DIsUsedSoWeNeedDirectXCheck = true;
					break;
				}
			}
		}
	}

	// leemove - 010306 - u60 - DirectX Check
	// leemovedagain - 221008 - u71 - moved to AFTER above check, as we only 
	// need to verify we have DirectX of ANY version if Basic3D employed
	bool bDirectXIsUpToDateFlag = false;
	if ( bBasic3DIsUsedSoWeNeedDirectXCheck==true )
	{
		// uses DirectX, so error if not up to date
		bDirectXIsUpToDateFlag = CheckIfGotLatestDirectX(false);
		if ( bDirectXIsUpToDateFlag==false )
			bResult=false;
	}
	else
	{
		// does not use DirectX, but we want to store the result so use silent detect
		bDirectXIsUpToDateFlag = CheckIfGotLatestDirectX(true);
	}

	// [EXE] Dependent DLL Linkage Info
	HINSTANCE hCoreDLL = nullptr;

	// [EXE] Switch to TEMP Folder (that holds all exe-linked files)
	std::error_code ec;
	m_OriginalFolderName = std::filesystem::current_path(ec).string();
	std::filesystem::current_path(m_UnpackFolderName, ec);

	// [EXE] Dynamically load all DLLs
	if(bResult==true)
	{
		if(bMiniInit==false) ZeroMemory(hDLLMod, sizeof(HINSTANCE)*256);
		if(bMiniInit==false) ZeroMemory(bDLLTPC, sizeof(bool)*256);
		for(DWORD dll=0; dll<m_dwNumberOfDLLs; dll++)
		{
			const DWORD dllindex=m_pDLLIndexArray[dll];
			if(dbp::runtime::IsDllIndex(dllindex))
			{
				// Skip if already loaded
				if(m_pDLLLoadedAlreadyArray[dll]==0)
				{
					// Set flag to loaded
					m_pDLLLoadedAlreadyArray[dll]=1;

					// Load the DLL into memory
					LPSTR pDLLName = (LPSTR)m_pDLLFilenameArray[dll];
					if(dbp::iequals(pDLLName, "EXE"))
					{
						// Module is part of the EXE (functionptrs passed into init above)
						hDLLMod[dllindex]=0;
					}
					else
					{
						// Module is a DLL. Attempt loading via VFS first.
						hDLLMod[dllindex] = MemoryPE::LoadFromVFS(exe_get_filename_only(pDLLName));
						if (hDLLMod[dllindex] == nullptr)
						{
							hDLLMod[dllindex] = MemoryPE::LoadFromVFS(pDLLName);
						}

						// If not loaded from VFS, search possible candidate filesystem paths.
						if (hDLLMod[dllindex] == nullptr)
						{
							std::vector<std::filesystem::path> searchPaths;
							auto addBaseSearchPaths = [&searchPaths, pDLLName](const std::filesystem::path& base) {
								if (base.empty()) return;
								searchPaths.push_back(base / pDLLName);
								searchPaths.push_back(base / "plugins" / pDLLName);
								searchPaths.push_back(base / "plugins-user" / pDLLName);
								searchPaths.push_back(base / "plugins-licensed" / pDLLName);
								searchPaths.push_back(base / ".." / "Compiler" / "plugins" / pDLLName);
								searchPaths.push_back(base / ".." / "Compiler" / "plugins-user" / pDLLName);
								searchPaths.push_back(base / ".." / "Compiler" / "plugins-licensed" / pDLLName);
								searchPaths.push_back(base / ".." / "plugins" / pDLLName);
								searchPaths.push_back(base / ".." / "plugins-user" / pDLLName);
								searchPaths.push_back(base / ".." / "plugins-licensed" / pDLLName);
								searchPaths.push_back(base / ".." / ".." / "Compiler" / "plugins" / pDLLName);
								searchPaths.push_back(base / ".." / ".." / "Compiler" / "plugins-user" / pDLLName);
								searchPaths.push_back(base / ".." / ".." / "Compiler" / "plugins-licensed" / pDLLName);
								searchPaths.push_back(base / ".." / ".." / "plugins" / pDLLName);
								searchPaths.push_back(base / ".." / ".." / "plugins-user" / pDLLName);
								searchPaths.push_back(base / ".." / ".." / "plugins-licensed" / pDLLName);
							};

							if (!m_AbsoluteAppFile.empty())
							{
								addBaseSearchPaths(std::filesystem::path(m_AbsoluteAppFile).parent_path());
							}
							if (!m_OriginalFolderName.empty())
							{
								addBaseSearchPaths(std::filesystem::path(m_OriginalFolderName));
							}
							if (!m_UnpackFolderName.empty())
							{
								addBaseSearchPaths(std::filesystem::path(m_UnpackFolderName));
							}

							for (const auto& probePath : searchPaths)
							{
								std::error_code probeEc;
								if (std::filesystem::exists(probePath, probeEc))
								{
									hDLLMod[dllindex] = dbp::dll::LoadApplicationDLLW(probePath.wstring().c_str());
									if (hDLLMod[dllindex] != nullptr)
									{
										break;
									}
								}
							}
						}

						if (hDLLMod[dllindex] != nullptr)
						{
							PluginRegistry::GetInstance().RegisterPlugin(pDLLName, hDLLMod[dllindex]);
							char dbgBuf[512];
							sprintf_s(dbgBuf, "[EXEBlock] Loaded DLL index %lu '%s' at %p\n", static_cast<unsigned long>(dllindex), pDLLName, (void*)hDLLMod[dllindex]);
							OutputDebugStringA(dbgBuf);
						}
						else
						{
							bool bIgnorableDLLs=false;
							if ( dbp::iequals(pDLLName,"ConvX.dll") ) bIgnorableDLLs=true;
							if ( dbp::iequals(pDLLName,"Conv3DS.dll") ) bIgnorableDLLs=true;
							if ( dbp::iequals(pDLLName,"ConvMDL.dll") ) bIgnorableDLLs=true;
							if ( dbp::iequals(pDLLName,"ConvMD2.dll") ) bIgnorableDLLs=true;
							if ( dbp::iequals(pDLLName,"ConvMD3.dll") ) bIgnorableDLLs=true;
							if ( bIgnorableDLLs )
							{
								// skips rest of nested code
								std::filesystem::current_path(m_UnpackFolderName, ec);
								continue;
							}
							else
							{
								if(*pReturnError==nullptr) *pReturnError = new char[1024];
								sprintf_s(*pReturnError, 1024, "Failed to load DLL (%d: %s)", dllindex, pDLLName);
								bResult=false;
								std::filesystem::current_path(m_UnpackFolderName, ec);
								break;
							}
						}

						std::filesystem::current_path(m_UnpackFolderName, ec);

						// Detect if DLL has the PassCoreData Function..
						DLL_PassCore g_DLL_PassCoreData;
						g_DLL_PassCoreData = ( DLL_PassCore ) Hook_GetProcAddress( hDLLMod[dllindex], "?ReceiveCoreDataPtr@@YAXPAX@Z" );
						if (!g_DLL_PassCoreData)
							g_DLL_PassCoreData = ( DLL_PassCore ) Hook_GetProcAddress( hDLLMod[dllindex], "ReceiveCoreDataPtr" );
						}

					// Flag if official DLL
					bool bIsOfficialDLL=false;

					// Record Internal Functions from COREDLL
					if(dbp::iequals(pDLLName,pCoreName))
					{
						// CORE Inits
						hCoreDLL = hDLLMod[dllindex];

						// CORE Pass Ptrs To Core (Error Handling, Data Statements)
						const auto coreApiResult = ResolveCoreRuntimeApi(
							[hCoreDLL](const char* name) -> void* {
								return reinterpret_cast<void*>(Hook_GetProcAddress(hCoreDLL, name));
							},
							DeriveProgramRuntimeRequirements(m_dwUsertypeStringPatternQuantity));
						if(coreApiResult)
						{
							g_CORE_PassCmdLinePtr = coreApiResult.value().passCommandLine;
							g_CORE_PassErrorPtr = coreApiResult.value().passError;
							g_CORE_PassEscapePtr = coreApiResult.value().passEscape;
							g_CORE_PassBreakOutPtr = coreApiResult.value().passBreakout;
							g_CORE_PassStructurePatterns = coreApiResult.value().passStructurePatterns;
							g_CORE_PassDataPtrs = coreApiResult.value().passDataStatements;
							g_CORE_InitDisplay = coreApiResult.value().initializeDisplay;
							g_CORE_CloseDisplay = coreApiResult.value().closeDisplay;
							g_CORE_PassDLLs = coreApiResult.value().passDlls;
							g_CORE_ConstructDLLs = coreApiResult.value().constructDlls;
							g_CORE_GetGlobPtr = coreApiResult.value().getGlob;
							g_CORE_CreateVarSpace = coreApiResult.value().createVariableSpace;
							g_CORE_DeleteVarSpace = coreApiResult.value().deleteVariableSpace;
							g_CORE_CreateDataSpace = coreApiResult.value().createDataSpace;
							g_CORE_DeleteDataSpace = coreApiResult.value().deleteDataSpace;
							g_CORE_DeleteVarItem = coreApiResult.value().deleteVariableItem;
							g_CORE_UnDim = coreApiResult.value().unDim;
							g_CORE_SyncRefresh = coreApiResult.value().sync;
						}
						else
						{
							if(*pReturnError==nullptr) *pReturnError = new char[1024];
							sprintf_s(*pReturnError, 1024, "%s", coreApiResult.error().message.c_str());
							bResult=false;
							break;
						}
						
						// CORE SEcurity Functions
						g_CORE_GetSecurityCode	= ( GDI_RetIntParamVoidPFN )			Hook_GetProcAddress( hCoreDLL, "?GetSecurityCode@@YAHXZ" );
						g_CORE_WipeSecurityCode	= ( GDI_RetVoidParamVoidPFN )			Hook_GetProcAddress( hCoreDLL, "?WipeSecurityCode@@YAXXZ" );

						// Get GlobStruct Ptr for rest of DLL loading
						if(g_CORE_GetGlobPtr) g_pGlob = (GlobStruct*)g_CORE_GetGlobPtr();
						bIsOfficialDLL=true;
					}

					// Associated DLLs for Minimal DirectX Support
					if(dbp::iequals(pDLLName,"DBProSetupDebug.dll")) { g_pGlob->g_GFX = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("GFX", hDLLMod[dllindex]); }
					if(dbp::iequals(pDLLName,"DBProBasic2DDebug.dll")) { g_pGlob->g_Basic2D = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("Basic2D", hDLLMod[dllindex]); }
					if(dbp::iequals(pDLLName,"DBProTextDebug.dll")) { g_pGlob->g_Text = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("Text", hDLLMod[dllindex]); }

					// Transforms Support
					if(dbp::iequals(pDLLName,"DBProTransformsDebug.dll"))
					{
						g_pGlob->g_Transforms = hDLLMod[dllindex];
						PluginRegistry::GetInstance().RegisterPlugin("Transforms", hDLLMod[dllindex]);

						g_Transforms_Constructor	= ( DLL_Constructor )						Hook_GetProcAddress( g_pGlob->g_Transforms, "Constructor" );
						g_Transforms_Destructor		= ( DLL_Destructor )						Hook_GetProcAddress( g_pGlob->g_Transforms, "Destructor" );
						g_Transforms_Update			= ( DLL_Update )							Hook_GetProcAddress( g_pGlob->g_Transforms, "Update" );

						bIsOfficialDLL=true;
					}

					// Sprite Support
					if(dbp::iequals(pDLLName,"DBProSpritesDebug.dll"))
					{
						// SPRITES Inits
						g_pGlob->g_Sprites = hDLLMod[dllindex];
						PluginRegistry::GetInstance().RegisterPlugin("Sprites", hDLLMod[dllindex]);

						// SPRITES Function Calls
						g_Sprites_Constructor       = ( SPRITES_RetVoidParamHINSTANCE2PFN )		Hook_GetProcAddress( g_pGlob->g_Sprites, "?Constructor@@YAXPAUHINSTANCE__@@0@Z" );
						g_Sprites_Destructor        = ( SPRITES_RetVoidParamVoidPFN )			Hook_GetProcAddress( g_pGlob->g_Sprites, "?Destructor@@YAXXZ" );
						g_Sprites_Update            = ( SPRITES_RetVoidParamVoidPFN )			Hook_GetProcAddress( g_pGlob->g_Sprites, "?Update@@YAXXZ" );
						bIsOfficialDLL=true;
					}

					// Image Support
					if(dbp::iequals(pDLLName,"DBProImageDebug.dll"))
					{
						// IMAGE Inits
						g_pGlob->g_Image = hDLLMod[dllindex];
						PluginRegistry::GetInstance().RegisterPlugin("Image", hDLLMod[dllindex]);

						// IMAGE Function Calls
						g_Image_Constructor          = ( IMAGE_RetVoidParamVoidPFN )		Hook_GetProcAddress( g_pGlob->g_Image, "?Constructor@@YAXPAUHINSTANCE__@@@Z" );
						g_Image_Destructor           = ( IMAGE_RetVoidParamVoidPFN )		Hook_GetProcAddress( g_pGlob->g_Image, "?Destructor@@YAXXZ" );
						bIsOfficialDLL=true;
					}

					// Input Support
					if(dbp::iequals(pDLLName,"DBProInputDebug.dll"))
					{
						g_pGlob->g_Input = hDLLMod[dllindex];
						bIsOfficialDLL=true;
						PluginRegistry::GetInstance().RegisterPlugin("Input", hDLLMod[dllindex]);
					}

					// System Support
					if(dbp::iequals(pDLLName,"DBProSystemDebug.dll"))
					{
						g_pGlob->g_System = hDLLMod[dllindex];
						bIsOfficialDLL=true;
						PluginRegistry::GetInstance().RegisterPlugin("System", hDLLMod[dllindex]);
					}	
					
					// Sound and Music Support
					if(dbp::iequals(pDLLName,"DBProSoundDebug.dll")) { g_pGlob->g_Sound = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("Sound", hDLLMod[dllindex]); }
					if(dbp::iequals(pDLLName,"DBProMusicDebug.dll")) { g_pGlob->g_Music = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("Music", hDLLMod[dllindex]); }
					
					// File Support
					if(dbp::iequals(pDLLName,"DBProFileDebug.dll"))
					{
						g_pGlob->g_File = hDLLMod[dllindex];
						bIsOfficialDLL=true;
						PluginRegistry::GetInstance().RegisterPlugin("File", hDLLMod[dllindex]);
					}	
					
					// FTP Support
					if(dbp::iequals(pDLLName,"DBProFTPDebug.dll"))
					{
						g_pGlob->g_FTP = hDLLMod[dllindex];
						bIsOfficialDLL=true;
						PluginRegistry::GetInstance().RegisterPlugin("FTP", hDLLMod[dllindex]);
					}			

					// Memblocks Support
					if(dbp::iequals(pDLLName,"DBProMemblocksDebug.dll"))
					{
						g_pGlob->g_Memblocks = hDLLMod[dllindex];
						bIsOfficialDLL=true;
						PluginRegistry::GetInstance().RegisterPlugin("Memblocks", hDLLMod[dllindex]);
					}			

					// Animation Support
					if(dbp::iequals(pDLLName,"DBProAnimationDebug.dll"))
					{
						g_pGlob->g_Animation = hDLLMod[dllindex];
						bIsOfficialDLL=true;
						PluginRegistry::GetInstance().RegisterPlugin("Animation", hDLLMod[dllindex]);
					}	
					
					// Bitmap Support
					if(dbp::iequals(pDLLName,"DBProBitmapDebug.dll"))
					{
						g_pGlob->g_Bitmap = hDLLMod[dllindex];
						bIsOfficialDLL=true;
						PluginRegistry::GetInstance().RegisterPlugin("Bitmap", hDLLMod[dllindex]);
					}	

					// Multiplayer Support
					if(dbp::iequals(pDLLName,"DBProMultiplayerDebug.dll"))
					{
						g_pGlob->g_Multiplayer = hDLLMod[dllindex];
						bIsOfficialDLL=true;
						PluginRegistry::GetInstance().RegisterPlugin("Multiplayer", hDLLMod[dllindex]);
					}	

					// 3D System Support
					if(dbp::iequals(pDLLName,"DBProCameraDebug.dll")) { g_pGlob->g_Camera3D = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("Camera3D", hDLLMod[dllindex]); }
					if(dbp::iequals(pDLLName,"DBProLightDebug.dll")) { g_pGlob->g_Light3D = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("Light3D", hDLLMod[dllindex]); }
					if(dbp::iequals(pDLLName,"DBProMatrixDebug.dll")) { g_pGlob->g_Matrix3D = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("Matrix3D", hDLLMod[dllindex]); }
					if(dbp::iequals(pDLLName,"DBProBasic3DDebug.dll")) { g_pGlob->g_Basic3D = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("Basic3D", hDLLMod[dllindex]); }
					if(dbp::iequals(pDLLName,"DBProWorld3DDebug.dll")) { g_pGlob->g_World3D = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("World3D", hDLLMod[dllindex]); }
					if(dbp::iequals(pDLLName,"DBProQ2BSPDebug.dll")) { g_pGlob->g_Q2BSP = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("Q2BSP", hDLLMod[dllindex]); }
					if(dbp::iequals(pDLLName,"DBProOwnBSPDebug.dll")) { g_pGlob->g_OwnBSP = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("OwnBSP", hDLLMod[dllindex]); }
					if(dbp::iequals(pDLLName,"DBProBSPCompilerDebug.dll")) { g_pGlob->g_BSPCompiler = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("BSPCompiler", hDLLMod[dllindex]); }
					if(dbp::iequals(pDLLName,"DBProParticlesDebug.dll")) { g_pGlob->g_Particles = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("Particles", hDLLMod[dllindex]); }

					// Support DLLs
					if(dbp::iequals(pDLLName,"DBProPrimObjectDebug.dll")) { g_pGlob->g_PrimObject = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("PrimObject", hDLLMod[dllindex]); }
					if(dbp::iequals(pDLLName,"DBProVectorsDebug.dll")) { g_pGlob->g_Vectors = hDLLMod[dllindex]; bIsOfficialDLL=true; PluginRegistry::GetInstance().RegisterPlugin("Vectors", hDLLMod[dllindex]); }
					if(dbp::iequals(pDLLName,"DBProLODTerrainDebug.dll")) { g_pGlob->g_LODTerrain = hDLLMod[dllindex]; bIsOfficialDLL=true; }
					if(dbp::iequals(pDLLName,"DBProCSGDebug.dll")) { g_pGlob->g_CSG = hDLLMod[dllindex]; bIsOfficialDLL=true; }

					// leeadd - 1403060 - igl - DLLs supported by CORE and GLOBSTRUCT, auxiliary functions
					if(dbp::iequals(pDLLName,"IGL.dll")) { g_pGlob->g_igLoader = hDLLMod[dllindex]; }

					// If none of these, must be TPC
					if ( bIsOfficialDLL==false )
						bDLLTPC[dllindex]=true;
				}
			}
		}
	}

	// [EXE] Copy vital data for all DLLs to access to Glob Structure
	if(g_pGlob)
	{
		memset ( g_pGlob->pEXEUnpackDirectory, 0, _MAX_PATH );
		snprintf(g_pGlob->pEXEUnpackDirectory, _MAX_PATH, "%s", m_UnpackFolderName.c_str());
		g_pGlob->ppEXEAbsFilename = (uintptr_t)m_AbsoluteAppFile.c_str();
		g_pGlob->dwEncryptionUniqueKey = m_dwEncryptionKey;
	}

	// [EXE] Load Icons into Glob for use by Core
	if(g_pGlob)
	{
		g_pGlob->hAppIcon = LoadIcon(g_pGlob->hInstance, MAKEINTRESOURCE(IDI_ICON1));
	}

	// [EXE] Pass Pointers and Call DLLs..
	if(bResult==true)
	{
		// Always does this first, but can do later if CLI adds DX support
		bool bDoFullDisplayInitialisation=false;

		// First and subsequent passings
		if(bMiniInit==false)
		{
			// Prepare Display (GDI or EXT)
			OutputDebugStringA("[EXEBlock] Step 1: Passing runtime pointers to Core\n");
			g_CORE_PassCmdLinePtr((LPVOID)pCmdLine);
			g_CORE_PassErrorPtr((LPVOID)&m_dwRuntimeErrorDWORD);
			g_CORE_PassEscapePtr((LPVOID)&g_dwEscapeValueMem);
			g_CORE_PassBreakOutPtr((LPVOID)&g_dwBreakOutPosition);
			// U71 - also pass in structure pattern qty+ptr
			if(m_dwUsertypeStringPatternQuantity>0)
				g_CORE_PassStructurePatterns((LPVOID)m_pUsertypeStringPatternArray, m_dwUsertypeStringPatternQuantity);
			OutputDebugStringA("[EXEBlock] Step 2: Calling g_CORE_PassDLLs\n");
			g_CORE_PassDLLs();

			// 1ST : Get CORE CREATION Security Code
			OutputDebugStringA("[EXEBlock] Step 3: Getting Security Code\n");
			int iSecurityCode = -1;
			if ( g_CORE_GetSecurityCode )
				iSecurityCode = g_CORE_GetSecurityCode();
			OutputDebugStringA("[EXEBlock] Step 4: Security Code obtained\n");

			// Initialise each TPC DLL from the plugin-user folder 
			for(DWORD dll=0; dll<m_dwNumberOfDLLs; dll++)
			{
				const DWORD dllindex=m_pDLLIndexArray[dll];
				if(dbp::runtime::IsDllIndex(dllindex) && bDLLTPC[dllindex]==true)
				{
					// Get Any DLL Function Pointers
					DLL_Constructor			g_DLL_Constructor;		// constructor
					g_DLL_Constructor		= ( DLL_Constructor )	Hook_GetProcAddress( hDLLMod[dllindex], "?Constructor@@YAXXZ" );
					if (!g_DLL_Constructor)
						g_DLL_Constructor	= ( DLL_Constructor )	Hook_GetProcAddress( hDLLMod[dllindex], "Constructor" );

					// Call TPC constructor functions (if any)
					if(g_DLL_Constructor)	g_DLL_Constructor();						

					// 2ND : TPC Sends special security code
					DLL_OptionalSecurityCode g_DLL_OptionalSecurityCode;
					g_DLL_OptionalSecurityCode = ( DLL_OptionalSecurityCode ) Hook_GetProcAddress( hDLLMod[dllindex], "?OptionalSecurityCode@@YAXH@Z" );
					if (!g_DLL_OptionalSecurityCode)
						g_DLL_OptionalSecurityCode = (DLL_OptionalSecurityCode)Hook_GetProcAddress(hDLLMod[dllindex], "OptionalSecurityCode");
					if(g_DLL_OptionalSecurityCode)	g_DLL_OptionalSecurityCode ( iSecurityCode );

					// get num of additional dependencies
					int iNumDLLDependencies = 0;
					typedef int ( *RETINTNOPARAM ) ( void );
					RETINTNOPARAM GetNumDependencies = ( RETINTNOPARAM ) Hook_GetProcAddress( hDLLMod[dllindex], "?GetNumDependencies@@YAHXZ" );
					if(!GetNumDependencies)
						GetNumDependencies = (RETINTNOPARAM)Hook_GetProcAddress(hDLLMod[dllindex], "GetNumDependencies");
					if ( GetNumDependencies ) iNumDLLDependencies=GetNumDependencies();

					// Obtain dependence and receive function pointers from DLL
					typedef void ( *RETVOIDLPSTRHINSTANCE ) ( LPSTR, HINSTANCE );
					RETVOIDLPSTRHINSTANCE ReceiveDependenceHinstance = ( RETVOIDLPSTRHINSTANCE ) Hook_GetProcAddress( hDLLMod[dllindex], "?ReceiveDependenciesHinstance@@YAXPADPAUHINSTANCE__@@@Z" );
					if (!ReceiveDependenceHinstance)
						ReceiveDependenceHinstance = (RETVOIDLPSTRHINSTANCE)Hook_GetProcAddress(hDLLMod[dllindex], "ReceiveDependenciesHinstance");

					// If Dependency HINSTANCE passing exists, pass them now
					if ( iNumDLLDependencies > 0 )
					{
						for ( int iD=0; iD<iNumDLLDependencies; iD++ )
						{
							// get dependence name from ID
							char pDLLNameToFind [ 256 ];
							typedef const char * ( *RETLPSTRNOPARAM ) ( int n );
							RETLPSTRNOPARAM GetDependencyID = ( RETLPSTRNOPARAM ) Hook_GetProcAddress( hDLLMod[dllindex], "?GetDependencyID@@YAPBDH@Z" );
							if (!GetDependencyID)
								GetDependencyID = (RETLPSTRNOPARAM)Hook_GetProcAddress(hDLLMod[dllindex], "GetDependencyID");
							const char* pDepName = GetDependencyID(iD);
							if (pDepName) snprintf(pDLLNameToFind, sizeof(pDLLNameToFind), "%s", pDepName); else pDLLNameToFind[0] = '\0';

							// find hModuleFound of that dependence
							HINSTANCE hModuleFound = nullptr;
							for ( DWORD findll=0; findll<m_dwNumberOfDLLs; findll++ )
							{
								LPSTR pFoundDLLName = (LPSTR)m_pDLLFilenameArray[findll];
								const DWORD founddllindex=m_pDLLIndexArray[findll];
								if(dbp::runtime::IsDllIndex(founddllindex))
								{
									if ( dbp::iequals( pFoundDLLName, pDLLNameToFind ) )
									{
										hModuleFound = hDLLMod[founddllindex];
										break;
									}
								}
							}

							// send matching module hinstance back to DLL
							if ( hModuleFound && ReceiveDependenceHinstance )
								ReceiveDependenceHinstance ( pDLLNameToFind, hModuleFound );
						}
					}
				}
			}

			// leeadd - 280305 - copy module handles to globstruct (so setupDLL can call refreshD3Ds)
			// U58			(sizeof(HINSTANCE)*256)+(sizeof(bool)*256)
			// HINSTANCE	hDLLMod[256];
			// bool			bDLLTPC[256];
			if ( g_pGlob->pDynMemPtr==nullptr )
			{
				g_pGlob->dwDynMemSize = (sizeof(HINSTANCE)*256)+(sizeof(bool)*256);
				g_pGlob->pDynMemPtr = new char [ g_pGlob->dwDynMemSize ];
				memcpy ( g_pGlob->pDynMemPtr+0, hDLLMod, (sizeof(HINSTANCE)*256) );
				memcpy ( g_pGlob->pDynMemPtr+(sizeof(HINSTANCE)*256), bDLLTPC, (sizeof(bool)*256) );
			}

			// 3RD : Wipe CORE CREATION Security Code
			if ( g_CORE_WipeSecurityCode ) g_CORE_WipeSecurityCode();

			// First time pass always init display
			bDoFullDisplayInitialisation=true;
		}
		else
		{
			// Pass DLLs that may have been loaded by miniCLI
			g_CORE_PassDLLs();

			// EXTDX Introduced, so init display now..
			if(g_hLastGFXPointer!=g_pGlob->g_GFX)
			{
				bDoFullDisplayInitialisation=true;
			}
		}

		// leeadd - 080306 - u60 - add DX version to globstruct
		g_pGlob->lpDirectXVersionString = (LPSTR)g_strDirectXVersion;

		// If full display initialisation required
		if(bDoFullDisplayInitialisation==true)
		{
			// leeadd - 070306 - u60 - istore igLoader HWND in glob struct so SETDISPLAYMODE in initdisplay can send the EMBED command
			if ( g_igLoader_HWND )
			{
				// going to embed to IGLoader
				g_pGlob->hwndIGLoader = g_igLoader_HWND;

				// superclass temp window for app-use (no second window!)
				g_pGlob->hWnd = g_hTempWindow;
			}
			else
			{
				// Remove temp window - now ready to create real window
				if(g_hTempWindow)
				{
					DestroyWindow(g_hTempWindow);
					g_hTempWindow=nullptr;
				}
			}

			// Initialise Display (and DX check)
			OutputDebugStringA("[EXEBlock] Step 5: Calling g_CORE_InitDisplay\n");
			if ( g_CORE_InitDisplay ( m_dwInitialDisplayMode, m_dwInitialDisplayWidth, m_dwInitialDisplayHeight, m_dwInitialDisplayDepth, hInstance, m_pInitialAppName)==1)
			{
				// Failed to DXSetup - Exit now
				OutputDebugStringA("[EXEBlock] Step 5: g_CORE_InitDisplay failed\n");
				bResult=false;
			}
			else
			{
				OutputDebugStringA("[EXEBlock] Step 5: g_CORE_InitDisplay succeeded\n");
			}
		}

		// leefix - 090703 - TPC needs to call passcoredata 'after' initdisplay
		if(bResult==true)
		{
			if(bMiniInit==false)
			{
				OutputDebugStringA("[EXEBlock] Step 6: Calling ReceiveCoreDataPtr for TPC plugins\n");
				for(DWORD dll=0; dll<m_dwNumberOfDLLs; dll++)
				{
					const DWORD dllindex=m_pDLLIndexArray[dll];
					if(dbp::runtime::IsDllIndex(dllindex) && bDLLTPC[dllindex]==true)
					{
						DLL_PassCore g_DLL_PassCoreData;
						g_DLL_PassCoreData = ( DLL_PassCore ) Hook_GetProcAddress( hDLLMod[dllindex], "?ReceiveCoreDataPtr@@YAXPAX@Z" );
						if (!g_DLL_PassCoreData)
							g_DLL_PassCoreData = ( DLL_PassCore )Hook_GetProcAddress(hDLLMod[dllindex], "ReceiveCoreDataPtr");
						if(g_DLL_PassCoreData)	g_DLL_PassCoreData( g_pGlob );
					}
				}
			}

			// Construct any new DLLs from miniCLI
			if(bMiniInit==true)
			{
				g_CORE_ConstructDLLs();
			}

			// Record DX ptr in case CLI introduces it later
			g_hLastGFXPointer=g_pGlob->g_GFX;
		}
	}

	// [EXE] Create Memory
	if(bMiniInit==false)
	{
		// [] Create Variable Space
		if(bResult==true)
		{
			if(m_dwVariableSpaceSize==0) m_dwVariableSpaceSize=1;
			m_pVariableSpace = static_cast<LPSTR>(g_CORE_CreateVarSpace(m_dwVariableSpaceSize));
			ZeroMemory(m_pVariableSpace, m_dwVariableSpaceSize);
			m_dwOldVariableSpaceSize=m_dwVariableSpaceSize;
		}

		// [] Create Data Statement Space
		if(bResult==true)
		{
			if(m_dwDataSpaceSize>0)
			{
				m_pDataSpace = static_cast<LPSTR>(g_CORE_CreateDataSpace(m_dwDataSpaceSize));
				memcpy(m_pDataSpace, m_pDataArray, m_dwDataSpaceSize);
				m_dwOldDataSpaceSize=m_dwDataSpaceSize;
				for(DWORD d=0; d<m_dwNumberOfDataItems; d++)
				{
					// Fill strings within data space using dynamic CExe creations
					if(m_pDataArray[(d*10)+0]==2)
					{
						char* pStr = (char*)m_pDataStringsArray[d];
						*(uintptr_t*)&m_pDataSpace[(d*10)+2] = (uintptr_t)pStr;
					}
				}
				g_CORE_PassDataPtrs(m_pDataSpace, m_pDataSpace+m_dwDataSpaceSize);
			}
		}
	}
	else
	{
		// [] Adjust Variable Space
		if(bResult==true)
		{
			if(m_dwVariableSpaceSize>0)
			{
				// Local Copy of vars (RAII scratch buffer)
				auto pTemp = std::make_unique<char[]>(m_dwOldVariableSpaceSize);
				memcpy(pTemp.get(), m_pVariableSpace, m_dwOldVariableSpaceSize);

				// Increase Size of VarSpace
				g_CORE_DeleteVarSpace();
				LPSTR pNew = static_cast<LPSTR>(g_CORE_CreateVarSpace(m_dwVariableSpaceSize));
				memcpy(pNew, pTemp.get(), m_dwOldVariableSpaceSize);
				m_dwOldVariableSpaceSize=m_dwVariableSpaceSize;
				m_pVariableSpace=pNew;
			}
		}

		// [] Create Data Statement Space
		if(bResult==true)
		{
			if(m_dwDataSpaceSize>0)
			{
				// Local Copy of data (RAII scratch buffer)
				auto pTemp = std::make_unique<char[]>(m_dwOldDataSpaceSize);
				memcpy(pTemp.get(), m_pDataSpace, m_dwOldDataSpaceSize);

				// Increase Size of DataSpace
				g_CORE_DeleteDataSpace();
				LPSTR pNew = static_cast<LPSTR>(g_CORE_CreateDataSpace(m_dwDataSpaceSize));
				memcpy(pNew, pTemp.get(), m_dwOldDataSpaceSize);
				m_dwOldDataSpaceSize=m_dwDataSpaceSize;
				m_pDataSpace=pNew;

				// Ensure Strings in DataSpace are handled (I guess...)
				for(DWORD d=0; d<m_dwNumberOfDataItems; d++)
				{
					// Fill strings within data space using dynamic CExe creations
					if(m_pDataArray[(d*10)+0]==2)
					{
						char* pStr = (char*)m_pDataStringsArray[d];
						*(uintptr_t*)&m_pDataSpace[(d*10)+2] = (uintptr_t)pStr;
					}
				}

				// Pass Updated DataSpace
				g_CORE_PassDataPtrs(m_pDataSpace, m_pDataSpace+m_dwDataSpaceSize);
			}
		}
	}

	// [EXE] Replace all XXXX Pointers with Dynamic Creations (RAII owner + raw alias)
	std::unique_ptr<uintptr_t[]> pProgramRefPtrOwner;
	uintptr_t* pProgramRefPtr = nullptr;
	if(bResult==true)
	{
		pProgramRefPtrOwner = std::make_unique<uintptr_t[]>(m_dwNumberOfReferences);
		pProgramRefPtr = pProgramRefPtrOwner.get();
		for(DWORD ref=0; ref<m_dwNumberOfReferences; ref++)
		{
			int iRefType=(int)m_pRefTypeArray[ref];
			if(iRefType>0)
			{
				// lee, somehow world3d decname is being looked for in imageDLL...

				// Generate Pointers from Data
				std::uint64_t index = m_pRefIndexArray[ref];
				if(iRefType==1)
				{
					// Command Address
					if (index >= m_dwNumberOfCommands ||
						m_pCommandDLLIdArray == nullptr ||
						m_pCommandDLLCallArray == nullptr ||
						m_pCommandDLLCallArray[index] == 0)
					{
						if (*pReturnError == nullptr) *pReturnError = new char[1024];
						sprintf_s(*pReturnError, 1024, "Command reference %lu is outside the executable command table", static_cast<unsigned long>(index));
						bResult = false;
						break;
					}
					const DWORD dll=m_pCommandDLLIdArray[index];
					if (!dbp::runtime::IsDllIndex(dll))
					{
						if (*pReturnError == nullptr) *pReturnError = new char[1024];
						sprintf_s(*pReturnError, 1024, "Command reference %lu uses invalid DLL index %lu", static_cast<unsigned long>(index), static_cast<unsigned long>(dll));
						bResult = false;
						break;
					}
					char* pStr = (char*)m_pCommandDLLCallArray[index];
					if(hDLLMod[dll]==0)
					{
						// Locate function ptr from EXE function ptr (passed in)
						if(dbp::iequals(pStr, "DHookS")) *(pProgramRefPtr+ref)=(uintptr_t)pDHookS;
						else if(dbp::iequals(pStr, "DHookJ")) *(pProgramRefPtr+ref)=(uintptr_t)pDHookJ;
						else if(dbp::iequals(pStr, "DHookR")) *(pProgramRefPtr+ref)=(uintptr_t)pDHookR;
						else
						{
							g_bSuccessfulDLLLinks=false;
							if(*pReturnError==nullptr) *pReturnError = new char[1024];
							int dlli = dll - 1; if(dlli<0) dlli=0;
							sprintf_s(*pReturnError, 1024, "DLL %lu:%s was not loaded for function '%s'", static_cast<unsigned long>(dll), (LPSTR)m_pDLLFilenameArray[dlli], pStr);
							bResult=false;
							break;
						}
					}
					else
					{
						// Locate function ptr from DLL
						uintptr_t dwAdd = (uintptr_t)Hook_GetProcAddress(hDLLMod[dll], pStr);
						if(dwAdd!=0)
						{
							*(pProgramRefPtr+ref)=dwAdd;
						}
						else
						{
							// Exit loop
							g_bSuccessfulDLLLinks=false;
							if(*pReturnError==nullptr) *pReturnError = new char[1024];
							int dlli = dll - 1; if(dlli<0) dlli=0;
							sprintf_s(*pReturnError, 1024, "Could not find function '%s' in %lu:%s", pStr, static_cast<unsigned long>(dll), (LPSTR)m_pDLLFilenameArray[dlli]);
							bResult=false;
							break;
						}
					}
				}
				if(iRefType==2)
				{
					// String Address
					if (index >= m_dwNumberOfStrings || m_pStringsArray == nullptr)
					{
						if (*pReturnError == nullptr) *pReturnError = new char[1024];
						sprintf_s(*pReturnError, 1024, "String reference %lu is outside the executable string table", static_cast<unsigned long>(index));
						bResult = false;
						break;
					}
					char* pStr = (char*)m_pStringsArray[index];
					if (pStr != nullptr && WriteTargetAddress(pProgramRefPtr + ref, pStr, pReturnError, bResult))
					{
						// string address recorded in reference slot
					}
					else if (bResult)
					{
						if(*pReturnError==nullptr) *pReturnError = new char[1024];
						snprintf(*pReturnError, 1024, "%s", "Could not find dynamic string during referencing");
						bResult=false;
					}
				}
				if(iRefType==3)
				{
					// Second Variable (_ERR_)(bytes 4,5,6,7) is always substituted
					if(index==4)
					{
						// Pointer to Runtime Error DWORD (filled by DLLs)
						WriteTargetAddress(pProgramRefPtr + ref, &m_dwRuntimeErrorDWORD, pReturnError, bResult);
					}
					else
					{
						// Third Variable (_ESC_)(bytes 4,5,6,7) is always substituted
						if(index==8)
						{
							// Pointer to Runtime Escape Value DWORD (filled by DLLs)
							WriteTargetAddress(pProgramRefPtr + ref, &g_dwEscapeValueMem, pReturnError, bResult);
						}
						else
						{
							// Forth Variable (_REK_)(bytes 8,9,10,11) is always substituted
							if(index==12)
							{
								// Pointer to Runtime Escape Value DWORD (filled by DLLs)
								WriteTargetAddress(pProgramRefPtr + ref, &g_dwBreakOutPosition, pReturnError, bResult);
							}
							else
							{
								if(index==16)
								{
									// NEW Pointer to Runtime Error Line DWORD - now protection value
									WriteTargetAddress(pProgramRefPtr + ref, &m_dwRuntimeErrorLineDWORD, pReturnError, bResult);
								}
								else
								{
									// Variable + Plus Offset stored representing global var
									WriteTargetAddress(pProgramRefPtr + ref, m_pVariableSpace + index, pReturnError, bResult);
								}
							}
						}
					}
				}
				if(iRefType==4)
				{
					// Direct Immediate Value
					*(pProgramRefPtr+ref)=index;
				}
				if(iRefType==5)
				{
					// Label Jump Position is index(byte offset)
					*(pProgramRefPtr+ref)=index;
				}
				if(iRefType==6)
				{
					// Data Position is index(byte offset)
					WriteTargetAddress(pProgramRefPtr + ref, m_pDataSpace + (index*10), pReturnError, bResult);
				}
			}
		}

		// Update DLL linking status based on whether all references succeeded
		g_bSuccessfulDLLLinks = bResult;
	}

	// [EXE] Replace Tokens with Data in Byte Positions.
	// x64-native relocation model: imm64 slots receive the absolute 64-bit
	// target; disp32/rel32 slots receive a PC-relative displacement computed
	// against the end of the enclosing instruction (relEnd), exactly like a
	// R_X86_64_PC32 relocation. The legacy uniform-8-byte patch wrote
	// absolute addresses into 4-byte slots, truncating them and clobbering
	// the following instruction - the root cause of boot-time AVs.
	if(bResult==true)
	{
		for(DWORD ref=0; ref<m_dwNumberOfReferences; ref++)
		{
			uintptr_t dwRefValue=*(pProgramRefPtr+ref);
			DWORD dwBytePosition=m_pRefArray[ref];
			int iRefType=(int)m_pRefTypeArray[ref];
			DWORD dwSlotBytes = (m_pRefWidthArray != nullptr)
				? m_pRefWidthArray[ref] : 8u;
			DWORD dwRelEnd = (m_pRefRelEndArray != nullptr)
				? m_pRefRelEndArray[ref] : 0u;
			if(dwRelEnd==0) dwRelEnd = dwBytePosition + dwSlotBytes;
			if(dwSlotBytes==8 && (iRefType==1 || iRefType==2 || iRefType==3 || iRefType==6))
			{
				// 64-bit absolute target pointer (imm64 slot)
				*(uintptr_t*)((char*)m_pMachineCodeBlock+dwBytePosition)=dwRefValue;
			}
			else if(iRefType==3 && dwSlotBytes<8)
			{
				// Variable address in a RIP-relative disp32 slot. The ref value
				// is the absolute runtime address of the variable (variable
				// space, a separate allocation), so the displacement must be
				// measured from the absolute address of the instruction end:
				// target - (MCB base + relEnd). Subtracting the raw offset
				// (relEnd) from an absolute pointer truncated the address into
				// the slot - the boot-time AV.
				const uintptr_t instructionEnd =
					(uintptr_t)m_pMachineCodeBlock + dwRelEnd;
				int iSigned = static_cast<int>(dwRefValue - instructionEnd);
				*(int*)((char*)m_pMachineCodeBlock+dwBytePosition)=iSigned;
			}
			else if(iRefType==5)
			{
				// Code label in a rel32 slot: PC-relative displacement.
				int iSigned = static_cast<int>(dwRefValue - dwRelEnd);
				*(int*)((char*)m_pMachineCodeBlock+dwBytePosition)=iSigned;
			}
			else if(iRefType==4)
			{
				// Direct immediate value sized to the operand slot.
				char* pTarget=(char*)m_pMachineCodeBlock+dwBytePosition;
				if(dwSlotBytes==8)
					*(uint64_t*)pTarget=static_cast<uint64_t>(dwRefValue);
				else if(dwSlotBytes==4)
					*(DWORD*)pTarget=static_cast<DWORD>(dwRefValue);
				else if(dwSlotBytes==2)
					*(WORD*)pTarget=static_cast<WORD>(dwRefValue);
				else if(dwSlotBytes==1)
					*(BYTE*)pTarget=static_cast<BYTE>(dwRefValue);
			}
			else
			{
				// 64-bit target pointer (imm64 slot); fall back to a DWORD for
				// any 4-byte slot that reaches this branch.
				if(dwSlotBytes>=8)
					*(uintptr_t*)((char*)m_pMachineCodeBlock+dwBytePosition)=dwRefValue;
				else
					*(DWORD*)((char*)m_pMachineCodeBlock+dwBytePosition)=static_cast<DWORD>(dwRefValue);
			}
		}
	}

	// Disable EscapeKey (WHEN running debug mode)
	if(pDHookS)
	{
		if(g_pGlob)
		{
			g_pGlob->bEscapeKeyEnabled=false;
		}
	}

	// leeadd - 221008 - u71 - extra info in globstruct for external debuggers
	if ( g_pGlob ) g_pGlob->g_pMachineCodeBlock = (DWORD_PTR)m_pMachineCodeBlock;

	// [EXE] W^X: Transition MCB from read-write to execute-read after all patching
	if ( m_pMachineCodeBlock && m_dwSizeOfMCB > 0 )
	{
		DWORD oldProtect = 0;
		if ( !VirtualProtect( m_pMachineCodeBlock, m_dwSizeOfMCB, PAGE_EXECUTE_READ, &oldProtect ) )
		{
			// Protection transition failed - MCB cannot be executed safely
			bResult = false;
		}
	}

	// [EXE] Switch out of TEMP Folder
	std::filesystem::current_path(m_OriginalFolderName, ec);

	return bResult;
}

bool CEXEBlock::Run(bool bResult)
{
	// [EXE] Run Machine Code Program
	if (bResult != true)
		return bResult;

	// [EXE] Run Program
	if(!m_pMachineCodeBlock)
		return bResult;

	char szDbg[256];
	sprintf_s(szDbg, "[EXEBlock] Calling g_CORE_Program: MCB=%p size=%lu\n", m_pMachineCodeBlock, m_dwSizeOfMCB);
	OutputDebugStringA(szDbg);
	g_CORE_Program = ( GDI_RetVoidParamVoidPFN ) m_pMachineCodeBlock;
	g_CORE_Program();
	OutputDebugStringA("[EXEBlock] g_CORE_Program returned successfully\n");

	return bResult;
}

bool CEXEBlock::RunFrom(bool bResult, DWORD dwOffset)
{
	// [EXE] Run Machine Code Program
	if (bResult != true)
		return false;

	// [EXE] Run Program
	if(!m_pMachineCodeBlock)
		return bResult;

	OutputDebugStringA("[EXEBlock] Calling g_CORE_Program (RunFrom)\n");
	g_CORE_Program = ( GDI_RetVoidParamVoidPFN ) ((LPSTR)m_pMachineCodeBlock + dwOffset);
	g_CORE_Program();
	OutputDebugStringA("[EXEBlock] g_CORE_Program returned successfully (RunFrom)\n");

	return bResult;
}

void CEXEBlock::FreeUptoDisplay(void)
{
	OutputDebugStringA("[EXEBlock] FreeUptoDisplay start\n");
	// [EXE] Delete All Allocations Within Data Space. String data items hold
	// copies of the m_pDataStringsArray pointers; those originals are released
	// by Clear(), so nothing here may free them a second time. The data space
	// block itself is released below.
	// (Data Item Format [Type][Reserved Byte][8byte for data]=10 bytes each)

	// [EXE] Delete Data Space
	if(g_CORE_DeleteDataSpace) g_CORE_DeleteDataSpace();

	OutputDebugStringA("[EXEBlock] FreeUptoDisplay: deleting var items\n");
	// [EXE] Delete All Allocations Within Var Space
	if(m_pVariableSpace && m_pDynamicVarsArray && m_pDynamicVarsArrayType)
	{
		for(DWORD dv=0; dv<m_dwDynamicVarsQuantity; dv++)
		{
			const DWORD dwOffset = m_pDynamicVarsArray[dv];
			if(dwOffset + sizeof(DWORD_PTR) > m_dwVariableSpaceSize)
				continue;
			DWORD_PTR* pSlot = reinterpret_cast<DWORD_PTR*>(m_pVariableSpace + dwOffset);
			if(!pSlot || !*pSlot)
				continue;
			if(m_pDynamicVarsArrayType[dv] == static_cast<DWORD>(DataType::Array))
			{
				if(g_CORE_UnDim) g_CORE_UnDim(*pSlot);
				*pSlot = 0;
			}
			else
			{
				if(g_CORE_DeleteVarItem) g_CORE_DeleteVarItem(pSlot);
			}
		}
	}

	OutputDebugStringA("[EXEBlock] FreeUptoDisplay: DeleteVarSpace\n");
	// [EXE] Delete Variable Space
	if(g_CORE_DeleteVarSpace) g_CORE_DeleteVarSpace();

	OutputDebugStringA("[EXEBlock] FreeUptoDisplay: CloseDisplay\n");
	// [EXE] Close Display
	if(g_CORE_CloseDisplay) g_CORE_CloseDisplay();
	OutputDebugStringA("[EXEBlock] FreeUptoDisplay end\n");
}

void CEXEBlock::Free(void)
{
	OutputDebugStringA("[EXEBlock] Free start\n");
	// [CORE] Close any memory created for glob (allocated with new char[])
	if ( g_pGlob && g_pGlob->pDynMemPtr ) SafeDeleteArray ( g_pGlob->pDynMemPtr );

	// [EXE] Free Up Data Allocations (from Load)
	Clear();
	OutputDebugStringA("[EXEBlock] Free: Clear completed\n");

	// [EXE] Freeing Up
	if(!g_bSuccessfulDLLLinks)
		return;

	// leeadd - 010304 - predestructor for better memory management
	for(int dll=255; dll>=0; dll--)
	{
		if(bDLLTPC[dll]==true)
		{
			// Call TPC pre-destructor (if any)
			DLL_Destructor			g_DLL_Destructor;			
			g_DLL_Destructor		= ( DLL_Destructor )	Hook_GetProcAddress( hDLLMod[dll], "?PreDestructor@@YAXXZ" );
			if (!g_DLL_Destructor)
				g_DLL_Destructor	= ( DLL_Destructor )	Hook_GetProcAddress( hDLLMod[dll], "PreDestructor" );
			if(g_DLL_Destructor)	g_DLL_Destructor();
		}
	}
	for(int dll=255; dll>=0; dll--)
	{
		if(bDLLTPC[dll]==true)
		{
			// Call TPC destructor (if any)
			DLL_Destructor			g_DLL_Destructor;
			g_DLL_Destructor		= ( DLL_Destructor )	Hook_GetProcAddress( hDLLMod[dll], "?Destructor@@YAXXZ" );
			if (!g_DLL_Destructor)
				g_DLL_Destructor	= ( DLL_Destructor )	Hook_GetProcAddress( hDLLMod[dll], "Destructor" );
			if(g_DLL_Destructor)	g_DLL_Destructor();
			bDLLTPC[dll]=false;
		}
		if(hDLLMod[dll])
		{
			if(MemoryPE::IsMemoryModule(hDLLMod[dll]))
				MemoryPE::UnloadModule(hDLLMod[dll]);
			else
				FreeLibrary(hDLLMod[dll]);
			hDLLMod[dll]=nullptr;
		}
	}
}
