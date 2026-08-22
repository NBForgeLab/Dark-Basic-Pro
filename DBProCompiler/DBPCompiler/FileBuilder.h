// FileBuilder.h: interface for the CFileBuilder class.
#pragma once

#include <windows.h>

#include "direct.h"
#include "io.h"
#include "dbp/package/KeyProvider.h"
#include "dbp/package/PackageWriter.h"
#include <filesystem>
#include <optional>
#include <vector>
#include <string>

// Class Defs
class CFileBuilder  
{
	public:
		CFileBuilder();
		~CFileBuilder();
		void DeleteFileTable(void);

		bool NewFileTable(void);
		bool AddFile(LPSTR pFilename, LPSTR pPlacement);
		bool AddWildcardFiles(LPSTR pMediaRoot, LPSTR pMediaWidlcardFile);
		bool MakeEXE(LPSTR destEXEfilename, bool bEncryptionState, LPSTR pCompressDLL);
		void SetPackageKeyFile(
			std::optional<std::filesystem::path> packageKeyFile);
		bool FinalizePackage(LPSTR pEXEFilename, DWORD KindOfExe);
		[[nodiscard]] bool HasStagedExecutable() const noexcept;

		bool ConstructEXE(LPSTR EXEfilename);

		bool ReplaceVersionInfoBlockInEXE(LPSTR pFilenameEXE, LPSTR pVersioBlock, DWORD dwOffsetToFirstEntry, DWORD dwVersionBlockSize);
		bool ChangeEXE(LPSTR pFilenameEXE);

		std::filesystem::path GetPackageDescriptorFileFromEXEFile(
			LPSTR destEXEfilename) const;

		bool ReplaceDataBlockInEXE ( LPSTR pFilenameEXE, LPSTR pPattern, LPSTR pDataBlock, DWORD dwBlockSize );

	private:

		// EXE Construction
		HANDLE		m_hfile;
		DWORD		m_SizeOfEXECode;

		// File Table (RAII vectors)
		std::vector<std::string>	m_FileTable;
		std::vector<std::string>	m_FileTablePlacement;

		// Encryption Vars
		bool		m_bEncryptionState;
		std::optional<std::filesystem::path> m_packageKeyFile;
		std::vector<dbp::package::PackageSourceEntry> m_packageEntries;
		dbp::package::KeyId m_packageKeyId{};
		dbp::package::SecureBuffer m_packageMasterKey;
		bool m_packageSessionReady = false;
		std::filesystem::path m_finalExecutablePath;
		std::filesystem::path m_stagedExecutablePath;
};
