// FileBuilder.h: interface for the CFileBuilder class.
#include "direct.h"
#include "io.h"
#include "dbp/package/KeyProvider.h"
#include "dbp/package/PackageWriter.h"
#include <filesystem>
#include <optional>
#include <vector>
#include <string>

/* 3.0 icon/cursor header  */
typedef struct {
    WORD iReserved;            /* always 0 */
    WORD iResourceType;
    WORD iResourceCount;       /* number of resources in file */
} ICOCURSORHDR;

/* 3.0 icon/cursor descriptor  */
typedef struct {
    BYTE iWidth;               /* width of image (icons only ) */
    BYTE iHeight;              /* height of image(icons only) */
    BYTE iColorCount;          /* number of colors in image */
    BYTE iUnused;              /*  */
    WORD iHotspotX;            /* hotspot x coordinate (CURSORS only) */
    WORD iHotspotY;            /* hotspot y coordinate (CURSORS only) */
    DWORD DIBSize;             /* size of DIB for this image */
    DWORD DIBOffset;           /* offset to DIB for this image */
} ICOCURSORDESC;

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
		bool ChangeEXE(LPSTR pFilenameEXE, LPSTR gPathToPluginFolderForBuilder);
		bool MakeICOFromBMP(LPSTR pBMPFilename, LPSTR pDestICOFilename);

		bool SaveIconCursorFileFromInfo(LPSTR pszFullFileName, int iWidth, int iHeight, int iColors, int iHotspotX, int iHotSpotY, LPSTR pImg, DWORD dwImgSize);
		bool MakeCURFromBMP(LPSTR pBMPFilename, LPSTR pDestCURFilename);

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
