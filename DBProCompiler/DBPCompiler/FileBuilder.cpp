//
// FileBuilder.cpp: implementation of the CFileBuilder class.
//

// Includes
#include "windows.h"
#include "StringUtils.h"
#include "resource.h"
#include "FileBuilder.h"
#include "Error.h"
#include "macros.h"
#include "wingdi.h"
#include "TextConvert.h"
#include "SafeDLLLoading.h"
#include "dbp/package/ExecutableKeyResource.h"
#include "dbp/package/RuntimeDescriptor.h"

#include <algorithm>
#include <filesystem>
#include <optional>

// External Class Pointer
extern CError* g_pErrorReport;

// External Data
extern char gUnpackDirectory[_MAX_PATH];
extern bool g_bLocalTempFolder;

// Implementations
CFileBuilder::CFileBuilder()
	: m_hfile(nullptr), m_SizeOfEXECode(0), m_bEncryptionState(false)
{
}

CFileBuilder::~CFileBuilder()
{
	if(!m_stagedExecutablePath.empty())
	{
		std::error_code ignored;
		std::filesystem::remove(m_stagedExecutablePath, ignored);
	}
}

namespace {

bool PublicationFailureRequested(const char* const stage)
{
	char value[64]{};
	const auto size = GetEnvironmentVariableA(
		"DBP_TEST_FAIL_PUBLICATION_STAGE",
		value,
		static_cast<DWORD>(std::size(value)));
	return size!=0 &&
		size<std::size(value) &&
		dbp::iequals(value, stage);
}

std::wstring HexKeyId(const dbp::package::KeyId& keyId)
{
	static constexpr wchar_t digits[] = L"0123456789abcdef";
	std::wstring result;
	result.reserve(keyId.size()*2);
	for(const auto byte : keyId)
	{
		result.push_back(digits[(byte>>4U)&0x0FU]);
		result.push_back(digits[byte&0x0FU]);
	}
	return result;
}

bool FlushFileForPublication(const std::filesystem::path& path)
{
	const auto handle = CreateFileW(
		path.c_str(),
		GENERIC_WRITE,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
		nullptr);
	if(handle==INVALID_HANDLE_VALUE)
		return false;
	const auto flushed = FlushFileBuffers(handle)!=FALSE;
	CloseHandle(handle);
	return flushed;
}

void RemoveStaleExecutableBackups(
	const std::filesystem::path& executablePath)
{
	const auto prefix =
		executablePath.filename().wstring() +
		L".dbp-backup-";
	std::error_code iterationError;
	for(const auto& entry :
		std::filesystem::directory_iterator(
			executablePath.parent_path(),
			iterationError))
	{
		if(iterationError)
			return;
		const auto name = entry.path().filename().wstring();
		std::error_code statusError;
		const auto status = entry.symlink_status(statusError);
		if(!statusError &&
			std::filesystem::is_regular_file(status) &&
			name.size()>=prefix.size() &&
			name.compare(0, prefix.size(), prefix)==0)
		{
			std::filesystem::remove(entry.path(), statusError);
		}
	}
}

} // namespace

void CFileBuilder::DeleteFileTable(void)
{
	m_FileTable.clear();
	m_FileTablePlacement.clear();
}

bool CFileBuilder::NewFileTable(void)
{
	// Delete old filetable
	DeleteFileTable();

	// Reserve initial capacity
	m_FileTable.reserve(10);
	m_FileTablePlacement.reserve(10);

	// Complete
	return true;
}

bool CFileBuilder::AddFile(LPSTR pFilename, LPSTR pPlacementFolder)
{
	// Add filename and placement path to table
	m_FileTable.emplace_back(pFilename);
	m_FileTablePlacement.emplace_back(pPlacementFolder);

	// Complete
	return true;
}

bool CFileBuilder::AddWildcardFiles(LPSTR pMediaRoot, LPSTR pMediaWidlcardFile)
{
	// Source media folder (std::string for safe concatenation)
	std::string srcFolderStr = std::string(pMediaRoot) + pMediaWidlcardFile;
	// Trim to directory (strip trailing filename after last separator)
	{
		size_t lastSep = srcFolderStr.find_last_of("\\/");
		if (lastSep != std::string::npos)
			srcFolderStr = srcFolderStr.substr(0, lastSep + 1);
	}

	// Destination media folder (strip leading directory from wildcard spec)
	std::string destFolderStr(pMediaWidlcardFile);
	{
		size_t lastSep = destFolderStr.find_last_of("\\/");
		if (lastSep != std::string::npos)
			destFolderStr = destFolderStr.substr(0, lastSep + 1);
		else
			destFolderStr.clear();
	}

	// Wildcard filename only (portion after dest folder prefix)
	std::string wildcardOnlyStr = std::string(pMediaWidlcardFile).substr(destFolderStr.size());

	std::error_code ec;
	if (!std::filesystem::exists(srcFolderStr, ec) || !std::filesystem::is_directory(srcFolderStr, ec))
		return true;

	for (const auto& entry : std::filesystem::directory_iterator(srcFolderStr, ec))
	{
		if (ec) break;
		if (entry.is_directory(ec))
		{
			std::string nestMediaDir = destFolderStr + entry.path().filename().string() + "\\";
			std::string nestWildcard = nestMediaDir + wildcardOnlyStr;
			AddWildcardFiles(pMediaRoot, const_cast<LPSTR>(nestWildcard.c_str()));
		}
		else if (entry.is_regular_file(ec))
		{
			std::string filename = entry.path().filename().string();
			bool bMatch = false;
			if (wildcardOnlyStr == "*.*" || wildcardOnlyStr == "*")
			{
				bMatch = true;
			}
			else if (wildcardOnlyStr.rfind("*.", 0) == 0)
			{
				std::string ext = wildcardOnlyStr.substr(1);
				if (dbp::iequals(entry.path().extension().string().c_str(), ext.c_str()))
					bMatch = true;
			}
			else if (dbp::iequals(filename.c_str(), wildcardOnlyStr.c_str()))
			{
				bMatch = true;
			}

			if (bMatch)
			{
				std::string mediaPath = std::string("media\\") + destFolderStr + filename;
				std::string absPathToMedia = (std::filesystem::path(srcFolderStr) / filename).string();
				AddFile(const_cast<LPSTR>(absPathToMedia.c_str()), const_cast<LPSTR>(mediaPath.c_str()));
			}
		}
	}

	return true;
}

bool CFileBuilder::MakeEXE(LPSTR destEXEfilename, bool bEncryptionState, LPSTR pCompressDLL)
{
	// The legacy switches are migration inputs only. DBPAK v2 always applies
	// authenticated encryption and selects compression per entry.
	m_bEncryptionState = bEncryptionState;
	(void)pCompressDLL;
	m_packageSessionReady = false;
	if(!m_stagedExecutablePath.empty())
	{
		std::error_code ignored;
		std::filesystem::remove(m_stagedExecutablePath, ignored);
		m_stagedExecutablePath.clear();
	}
	m_packageEntries.clear();
	if(m_FileTable.size()!=m_FileTablePlacement.size())
	{
		g_pErrorReport->AddErrorString(
			"DBP3101: Package input table is inconsistent.");
		return false;
	}
	for(std::size_t index=0; index<m_FileTable.size(); ++index)
	{
		m_packageEntries.push_back({
			std::filesystem::path(
				TextConvert::UTF8ToUTF16(m_FileTable[index])),
			m_FileTablePlacement[index],
			true});
	}

	dbp::package::CngCryptoProvider crypto;
	const auto randomKeyId = crypto.RandomBytes(m_packageKeyId.size());
	if(!randomKeyId)
	{
		std::string message =
			"DBP3102: Failed to generate the package key identifier: " +
			randomKeyId.error().message;
		g_pErrorReport->AddErrorString(message.data());
		return false;
	}
	std::copy(
		randomKeyId.value().begin(),
		randomKeyId.value().end(),
		m_packageKeyId.begin());

	if(m_packageKeyFile)
	{
		dbp::package::FileKeyProvider provider(
			m_packageKeyId,
			*m_packageKeyFile);
		auto resolved = provider.Resolve(m_packageKeyId);
		if(!resolved)
		{
			std::string message =
				"DBP3103: Failed to read the package key file: " +
				resolved.error().message;
			g_pErrorReport->AddErrorString(message.data());
			return false;
		}
		m_packageMasterKey = std::move(resolved.value());
	}
	else
	{
		const auto generated =
			crypto.RandomBytes(dbp::package::kPackageMasterKeySize);
		if(!generated)
		{
			std::string message =
				"DBP3102: Failed to generate the package master key: " +
				generated.error().message;
			g_pErrorReport->AddErrorString(message.data());
			return false;
		}
		m_packageMasterKey =
			dbp::package::SecureBuffer::FromBytes(generated.value());
	}

	std::error_code outputError;
	m_finalExecutablePath = std::filesystem::absolute(
		std::filesystem::path(
			TextConvert::UTF8ToUTF16(destEXEfilename)),
		outputError).lexically_normal();
	if(outputError || m_finalExecutablePath.filename().empty())
	{
		g_pErrorReport->AddErrorString(
			"DBP3104: Resolving the executable staging path failed.");
		return false;
	}
	m_stagedExecutablePath =
		m_finalExecutablePath.parent_path() /
		(L"." + m_finalExecutablePath.filename().wstring() +
		 L".dbp-stage-" + HexKeyId(m_packageKeyId));
	auto stagedUtf8 =
		TextConvert::UTF16ToUTF8(m_stagedExecutablePath.wstring());
	if(!ConstructEXE(stagedUtf8.data()))
		return false;
	m_packageSessionReady = true;
	return true;
}

void CFileBuilder::SetPackageKeyFile(
	std::optional<std::filesystem::path> packageKeyFile)
{
	m_packageKeyFile = std::move(packageKeyFile);
}

bool CFileBuilder::HasStagedExecutable() const noexcept
{
	std::error_code error;
	return !m_stagedExecutablePath.empty() &&
		std::filesystem::is_regular_file(
			m_stagedExecutablePath,
			error) &&
		!error;
}

bool CFileBuilder::FinalizePackage(
	LPSTR pEXEFilename,
	DWORD KindOfExecutable)
{
	if(!m_packageSessionReady ||
		m_packageMasterKey.size()!=dbp::package::kPackageMasterKeySize)
	{
		g_pErrorReport->AddErrorString(
			"DBP3104: Package finalization was requested without a valid session.");
		return false;
	}
	if(KindOfExecutable>1)
	{
		g_pErrorReport->AddErrorString(
			"DBP3104: The runtime package mode is invalid.");
		return false;
	}

	std::error_code pathError;
	auto executablePath = std::filesystem::absolute(
		std::filesystem::path(
			TextConvert::UTF8ToUTF16(pEXEFilename)),
		pathError).lexically_normal();
	if(pathError)
	{
		g_pErrorReport->AddErrorString(
			"DBP3104: Resolving the executable output path failed.");
		return false;
	}
	if(executablePath!=m_finalExecutablePath ||
		m_stagedExecutablePath.empty())
	{
		g_pErrorReport->AddErrorString(
			"DBP3104: The executable finalization path does not match its staging session.");
		return false;
	}
	auto outputDirectory = executablePath.parent_path();
	if(outputDirectory.empty())
	{
		outputDirectory =
			std::filesystem::current_path(pathError);
		if(pathError)
		{
			g_pErrorReport->AddErrorString(
				"DBP3104: Resolving the package output directory failed.");
			return false;
		}
	}

	dbp::package::CngCryptoProvider crypto;
	dbp::package::ZstdCompressionCodec compression;
	dbp::package::Win32AtomicFilePublisher publisher;
	dbp::package::MemoryKeyProvider keys(
		m_packageKeyId,
		dbp::package::SecureBuffer::FromBytes(
			m_packageMasterKey.CopyBytes()));
	dbp::package::PackageWriter writer(
		crypto,
		compression,
		publisher);
	const auto written = writer.Write(
		{outputDirectory, m_packageKeyId, m_packageEntries},
		keys);
	if(!written)
	{
		std::string message =
			"DBP3105: Failed to publish the DBPAK package: " +
			written.error().message;
		g_pErrorReport->AddErrorString(message.data());
		return false;
	}
	if(PublicationFailureRequested("after-package"))
	{
		g_pErrorReport->AddErrorString(
			"DBP3190: Simulated failure after package publication.");
		return false;
	}

	std::optional<dbp::package::ExecutablePackageKey> previousKey;
	const auto descriptorPath =
		GetPackageDescriptorFileFromEXEFile(pEXEFilename);
	std::error_code previousError;
	if(std::filesystem::is_regular_file(
			executablePath,
			previousError) &&
		!previousError &&
		std::filesystem::is_regular_file(
			descriptorPath,
			previousError) &&
		!previousError)
	{
		const auto previousDescriptor =
			dbp::package::ReadRuntimeDescriptor(descriptorPath);
		if(previousDescriptor)
		{
			auto resolvedPrevious =
				dbp::package::ReadExecutablePackageKey(
					executablePath,
					previousDescriptor.value().keyId);
			if(resolvedPrevious)
				previousKey.emplace(
					std::move(resolvedPrevious.value()));
		}
	}
	const auto injected =
		dbp::package::InjectExecutablePackageKeys(
			m_stagedExecutablePath,
			m_packageKeyId,
			m_packageMasterKey,
			previousKey ? &*previousKey : nullptr);
	if(!injected)
	{
		std::string message =
			"DBP3106: Failed to inject the executable package key: " +
			injected.error().message;
		g_pErrorReport->AddErrorString(message.data());
		return false;
	}
	if(!FlushFileForPublication(m_stagedExecutablePath))
	{
		g_pErrorReport->AddErrorString(
			"DBP3106: Flushing the staged executable failed.");
		return false;
	}

	dbp::package::RuntimeDescriptor descriptor;
	descriptor.mode = KindOfExecutable==0
		? dbp::package::RuntimeMode::Application
		: dbp::package::RuntimeMode::Installer;
	descriptor.packageId = written.value().packageId;
	descriptor.keyId = m_packageKeyId;
	descriptor.packageFileName =
		written.value().packagePath.filename().string();
	auto backupPath = executablePath;
	backupPath += L".dbp-backup-" + HexKeyId(m_packageKeyId);
	std::filesystem::remove(backupPath, previousError);
	const auto hadPreviousExecutable =
		std::filesystem::is_regular_file(
			executablePath,
			previousError) &&
		!previousError;
	const auto executablePublished = hadPreviousExecutable
		? ReplaceFileW(
			executablePath.c_str(),
			m_stagedExecutablePath.c_str(),
			backupPath.c_str(),
			REPLACEFILE_WRITE_THROUGH,
			nullptr,
			nullptr)!=FALSE
		: MoveFileExW(
			m_stagedExecutablePath.c_str(),
			executablePath.c_str(),
			MOVEFILE_WRITE_THROUGH)!=FALSE;
	if(!executablePublished)
	{
		g_pErrorReport->AddErrorString(
			"DBP3106: Atomically publishing the executable failed.");
		return false;
	}
	m_stagedExecutablePath.clear();

	if(PublicationFailureRequested("after-executable"))
	{
		g_pErrorReport->AddErrorString(
			"DBP3191: Simulated interruption after executable publication.");
		return false;
	}

	const auto descriptorWritten =
		dbp::package::WriteRuntimeDescriptorAtomically(
			descriptorPath,
			descriptor);
	if(!descriptorWritten)
	{
		if(hadPreviousExecutable)
		{
			ReplaceFileW(
				executablePath.c_str(),
				backupPath.c_str(),
				nullptr,
				REPLACEFILE_WRITE_THROUGH,
				nullptr,
				nullptr);
		}
		else
		{
			std::filesystem::remove(executablePath, previousError);
		}
		std::string message =
			"DBP3107: Failed to publish the runtime package descriptor: " +
			descriptorWritten.error().message;
		g_pErrorReport->AddErrorString(message.data());
		return false;
	}
	std::filesystem::remove(backupPath, previousError);
	RemoveStaleExecutableBackups(executablePath);
	m_packageSessionReady = false;
	return true;
}

bool CFileBuilder::ConstructEXE(LPSTR EXEfilename)
{
	// Create File and place EXE Runner Code
	DeleteFileW(TextConvert::UTF8ToUTF16(EXEfilename).c_str());
	m_hfile = CreateFileW(TextConvert::UTF8ToUTF16(EXEfilename).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(m_hfile==INVALID_HANDLE_VALUE)
	{
		char err[256];
		snprintf(err, sizeof(err), "Could not create %s", EXEfilename);
		g_pErrorReport->AddErrorString(err);
		return false;
	}

	// Use FULL or DEMO Core for demo (no silly code this time)
	WORD wCoreCode = IDR_CORE;

	// lee - 050406 - u6rc6 - If LOCAL EXE required, use the EXELOCAL version
	if ( g_bLocalTempFolder ) wCoreCode = IDR_X1;

	// Get EXE Runner Code
	m_SizeOfEXECode = SizeofResource(nullptr, FindResourceW(nullptr, MAKEINTRESOURCE(wCoreCode), L"X"));
	HGLOBAL hGlobal = LoadResource(nullptr, FindResourceW(nullptr, MAKEINTRESOURCE(wCoreCode), L"X"));
	LPVOID lpResDataBuffer = LockResource(hGlobal);
	if(m_SizeOfEXECode<=0)
	{
		g_pErrorReport->AddErrorString("Failed to 'CFileBuilder::ConstructEXE::LockResource'");
		return false;
	}

	// Write EXE Code first to launch core executable
	DWORD byteswritten;
	WriteFile(m_hfile, lpResDataBuffer, m_SizeOfEXECode, &byteswritten, nullptr); 

	// EXE is complete
	CloseHandle(m_hfile);
	return true;
}

struct newBITMAPINFO
{
    BITMAPINFOHEADER	 bmiHeader; 
    RGBQUAD		       bmiColors[256]; 
}; 

bool CFileBuilder::ReplaceVersionInfoBlockInEXE(LPSTR pFilenameEXE, LPSTR pVersioBlock, DWORD dwOffsetToFirstEntry, DWORD dwVersionBlockSize)
{
	// Simply scans the EXE and locates the Version Block, and directly replaces it
	DWORD dwSizeOfEXECode = 0;	
	HANDLE hreadfile = CreateFileW(TextConvert::UTF8ToUTF16(pFilenameEXE).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(hreadfile!=INVALID_HANDLE_VALUE)
	{
		// Read EXE into memory
		DWORD bytesread=0;
		dwSizeOfEXECode = GetFileSize(hreadfile, nullptr);	
		std::vector<char> exeData(dwSizeOfEXECode);
		ReadFile(hreadfile, exeData.data(), dwSizeOfEXECode, &bytesread, nullptr); 
		CloseHandle(hreadfile);

		// Modify this data
		LPSTR pPtr = exeData.data();
		LPSTR pPtrEnd = pPtr + dwSizeOfEXECode;
		while (pPtr<pPtrEnd)
		{
			// find a match with the first X bytes
			if ( pPtr+dwOffsetToFirstEntry<pPtrEnd )
			{
				// check byteblock
				bool bOkaySoFar=true;
				LPSTR pCheckByte=pPtr;
				for ( DWORD n=0; n<dwOffsetToFirstEntry; n++ )
				{
					if ( *pCheckByte != pVersioBlock[n] )
					{
						bOkaySoFar=false;
						break;
					}
					pCheckByte++;
				}

				// if it matches versionblock
				if ( bOkaySoFar==true )
				{
					// replace whole version block
					memcpy ( pPtr, pVersioBlock, dwVersionBlockSize );
					pPtr=pPtrEnd+1;
					break;
				}
			}

			// next byte
			pPtr++;
		}

		// Write EXE back out
		HANDLE hwritefile = CreateFileW(TextConvert::UTF8ToUTF16(pFilenameEXE).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if(hwritefile!=INVALID_HANDLE_VALUE)
		{
			DWORD byteswritten=0;
			WriteFile(hwritefile, exeData.data(), dwSizeOfEXECode, &byteswritten, nullptr); 
			CloseHandle(hwritefile);
		}
	}

	// complete
	return true;
}

bool CFileBuilder::ChangeEXE(LPSTR pFilenameEXE)
{
	std::string stagedExecutableUtf8;
	if(m_packageSessionReady &&
		!m_stagedExecutablePath.empty())
	{
		stagedExecutableUtf8 =
			TextConvert::UTF16ToUTF8(
				m_stagedExecutablePath.wstring());
		pFilenameEXE = stagedExecutableUtf8.data();
	}
	if(m_FileTable.size()!=10U)
	{
		g_pErrorReport->AddErrorString(
			"DBP3108: Executable resource metadata is incomplete.");
		return false;
	}

	// File 0-9 is version string info
	LPSTR pVerComments = const_cast<LPSTR>(m_FileTable[0].c_str());
	LPSTR pVerCompany = const_cast<LPSTR>(m_FileTable[1].c_str());
	LPSTR pVerFileDesc = const_cast<LPSTR>(m_FileTable[2].c_str());
	LPSTR pVerFileNumber = const_cast<LPSTR>(m_FileTable[3].c_str());
	LPSTR pVerInternal = const_cast<LPSTR>(m_FileTable[4].c_str());
	LPSTR pVerCopyright = const_cast<LPSTR>(m_FileTable[5].c_str());
	LPSTR pVerTrademark = const_cast<LPSTR>(m_FileTable[6].c_str());
	LPSTR pVerFilename = const_cast<LPSTR>(m_FileTable[7].c_str());
	LPSTR pVerProduct = const_cast<LPSTR>(m_FileTable[8].c_str());
	LPSTR pVerProductNumber = const_cast<LPSTR>(m_FileTable[9].c_str());

	// Absolute Path for Modulename (std::string for safe concatenation)
	std::string moduleNameStr;
	if(pFilenameEXE[1]==':')
	{
		// Filename is absolute
		moduleNameStr = pFilenameEXE;
	}
	else
	{
		// File is relative
		moduleNameStr = (std::filesystem::current_path() / pFilenameEXE).string();
	}

	// Change VERSION INFORMATION
	if(pVerComments)
	{
		// Access Resource from EXE
		HMODULE hEXE = LoadLibraryExW(TextConvert::UTF8ToUTF16(moduleNameStr.c_str()).c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE);
		HRSRC hRes=FindResource(hEXE, (LPCTSTR)1, RT_VERSION);
		DWORD dwDataSize = SizeofResource(hEXE, hRes);
		HGLOBAL hGlobal = LoadResource(hEXE, hRes);
		LPVOID lpResReal = LockResource(hGlobal);

		// Get Version Data in UNICODE (vector owns the buffer on all paths)
		std::vector<char> versionData(dwDataSize);
		LPSTR pVersonData = versionData.data();
		memcpy(pVersonData, (LPSTR)lpResReal, dwDataSize);

		// Construct WideCharacter
		int iIndex=0;
		LPSTR pPtr = pVersonData;
		LPSTR pVersonDataEnd=pVersonData+dwDataSize;
		DWORD dwOffsetToFirstEntry=0;
		while(pPtr<pVersonDataEnd)
		{
			// Get Src
			DWORD dwLength = 0;
			LPSTR pCharStr = nullptr;
			if(iIndex==0) { dwLength=32; pCharStr=pVerComments; }
			if(iIndex==1) { dwLength=32; pCharStr=pVerCompany; }
			if(iIndex==2) { dwLength=32; pCharStr=pVerFileDesc; }
			if(iIndex==3) { dwLength=10; pCharStr=pVerFileNumber; }
			if(iIndex==4) { dwLength=32; pCharStr=pVerInternal; }
			if(iIndex==5) { dwLength=32; pCharStr=pVerCopyright; }
			if(iIndex==6) { dwLength=32; pCharStr=pVerTrademark; }
			if(iIndex==7) { dwLength=32; pCharStr=pVerFilename; }
			if(iIndex==8) { dwLength=32; pCharStr=pVerProduct; }
			if(iIndex==9) { dwLength=10; pCharStr=pVerProductNumber; }

			// Build match string
			char pMatchStr[100];
			int c=0;
			for(DWORD b=0; b<dwLength; b++)
			{
				int d=iIndex+1;
				if(iIndex==9) d=0;
				pMatchStr[c++]=static_cast<char>(48+d);
				pMatchStr[c++]=0;
			}
			pMatchStr[c++]=0;

			// Is there a match?
			bool bMatch=true;
			for(DWORD e=0; e<dwLength*2; e++)
			{
				if(pPtr+e>=pVersonDataEnd)
				{
					bMatch=false;
					break;
				}
				if(pPtr[e]!=pMatchStr[e])
					bMatch=false;
			}
				
			// Write direct to version data
			if(bMatch==true)
			{
				if(iIndex==0) dwOffsetToFirstEntry=static_cast<DWORD>(pPtr-pVersonData);
				MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, pCharStr, -1, (LPWSTR)pPtr, dwLength*2);
				iIndex++;
			}


			// Next char in version data
			pPtr++;
		}

		// Finished with EXE Access
		FreeLibrary(hEXE);

		// Works for all Operating Systems - replace VersionBlock
		ReplaceVersionInfoBlockInEXE ( const_cast<LPSTR>(moduleNameStr.c_str()), pVersonData, dwOffsetToFirstEntry, dwDataSize );
	}

	// Progress Reporting Tool
	g_pErrorReport->ProgressReport("Linker now at line ",g_pErrorReport->GetPerc(70));

	// Complete
	return true;
}

std::filesystem::path
CFileBuilder::GetPackageDescriptorFileFromEXEFile(
	LPSTR destEXEfilename) const
{
	auto descriptor = std::filesystem::path(
		TextConvert::UTF8ToUTF16(destEXEfilename));
	descriptor.replace_extension(L".dbpakref");
	return descriptor;
}

bool CFileBuilder::ReplaceDataBlockInEXE ( LPSTR pFilenameEXE, LPSTR pPattern, LPSTR pDataBlock, DWORD dwBlockSize )
{
	// Simply scans the EXE and locates the pattern in the data, and replaces it
	DWORD dwSizeOfEXECode = 0;	
	HANDLE hreadfile = CreateFileW(TextConvert::UTF8ToUTF16(pFilenameEXE).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(hreadfile!=INVALID_HANDLE_VALUE)
	{
		// Read EXE into memory (vector owns the buffer on all paths)
		DWORD bytesread=0;
		dwSizeOfEXECode = GetFileSize(hreadfile, nullptr);	
		std::vector<char> exeData(dwSizeOfEXECode);
		ReadFile(hreadfile, exeData.data(), dwSizeOfEXECode, &bytesread, nullptr); 
		CloseHandle(hreadfile);

		// Modify this data
		LPSTR pPtr = exeData.data();
		LPSTR pPtrEnd = pPtr + dwSizeOfEXECode;
		while (pPtr<pPtrEnd)
		{
			// find a match with the pattern
			if ( pPtr<pPtrEnd )
			{
				// check byteblock
				bool bOkaySoFar=true;
				LPSTR pCheckByte=pPtr;
				for ( DWORD n=0; n<dwBlockSize; n++ )
				{
					if ( *pCheckByte != pPattern[n] )
					{
						bOkaySoFar=false;
						break;
					}
					pCheckByte++;
				}

				// if it matches perfectly
				if ( bOkaySoFar==true )
				{
					// replace whole block
					memcpy ( pPtr, pDataBlock, dwBlockSize );
					pPtr=pPtrEnd+1;
					break;
				}
			}

			// next byte if still not found
			pPtr++;
		}

		// Write EXE back out
		HANDLE hwritefile = CreateFileW(TextConvert::UTF8ToUTF16(pFilenameEXE).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if(hwritefile!=INVALID_HANDLE_VALUE)
		{
			DWORD byteswritten=0;
			WriteFile(hwritefile, exeData.data(), dwSizeOfEXECode, &byteswritten, nullptr); 
			CloseHandle(hwritefile);
		}
	}

	// complete
	return true;
}
