// DBPCompiler.h: interface for the CDBPCompiler class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DBPCOMPILER_H__59BB1DE5_04A2_4BBC_9790_52F5C94E07F9__INCLUDED_)
#define AFX_DBPCOMPILER_H__59BB1DE5_04A2_4BBC_9790_52F5C94E07F9__INCLUDED_

// Common Includes
#include "windows.h"
#include "RuntimeBundleResolver.h"
#include "Str.h"
#include <memory>
#include <vector>
#include <string>

// Internal file-table identifiers (constexpr, type-safe)
namespace dbp_paths {
constexpr DWORD MAX = 20;
constexpr DWORD ROOTPATH = 1;
constexpr DWORD SETUPFILE = 2;
constexpr DWORD ERRORSFILE = 3;
constexpr DWORD PLUGINSFOLDER = 4;
constexpr DWORD TEMPFOLDER = 5;
constexpr DWORD TEMPDBMFILE = 6;
constexpr DWORD TEMPEXBFILE = 7;
constexpr DWORD TEMPERRORFILE = 8;
constexpr DWORD DEBUGGERFILE = 9;
constexpr DWORD WORDSFILE = 10;
constexpr DWORD PLUGINSUSERFOLDER = 11;
constexpr DWORD PLUGINSLICENSEDFOLDER = 12;
constexpr DWORD CURRENTFOLDER = 13;
}

#define PATH_MAX 20
#define PATH_ROOTPATH dbp_paths::ROOTPATH
#define PATH_SETUPFILE dbp_paths::SETUPFILE
#define PATH_ERRORSFILE dbp_paths::ERRORSFILE
#define PATH_PLUGINSFOLDER dbp_paths::PLUGINSFOLDER
#define PATH_TEMPFOLDER dbp_paths::TEMPFOLDER
#define PATH_TEMPDBMFILE dbp_paths::TEMPDBMFILE
#define PATH_TEMPEXBFILE dbp_paths::TEMPEXBFILE
#define PATH_TEMPERRORFILE dbp_paths::TEMPERRORFILE
#define PATH_DEBUGGERFILE dbp_paths::DEBUGGERFILE
#define PATH_WORDSFILE dbp_paths::WORDSFILE
#define PATH_PLUGINSUSERFOLDER dbp_paths::PLUGINSUSERFOLDER
#define PATH_PLUGINSLICENSEDFOLDER dbp_paths::PLUGINSLICENSEDFOLDER
#define PATH_CURRENTFOLDER dbp_paths::CURRENTFOLDER

// External Words Array
#define EXTWORDSMAX			32
#define MAX_EXCLUSIONS		256

class CompilerContext;
class CompilationInput;

// Define Class
class CDBPCompiler  
{
	public:
		CDBPCompiler(LPSTR pCompilerFilename);
		virtual ~CDBPCompiler();

	public:
		bool			PerformCompileOnProject(void);
		bool			PrepareCompilationInput(const char* pInputFilename, bool emitFinalSource = false);
		bool			LoadPreparedSource(void);
		bool			LoadDBA(LPSTR pDBAFilename);
		bool			LoadRaw(LPSTR pDBAFilename, LPSTR* ppData, DWORD* pdwDataSize);
		bool			LoadRawFromMMF(LPSTR pDBAFilename, LPSTR* ppData, DWORD* pdwDataSize);
		bool			UnfoldFileDataIncludes(void);
		void			EnsureDataMemBugEnough(LPSTR pPtr, DWORD dwPredictSize, LPSTR* pNewData, DWORD* dwNewDataSize, LPSTR* pWritePtrOut);
		bool			UnfoldFileDataConstants(void);
		bool			CopyData(LPSTR* ppData, DWORD* pdwDataSize, LPSTR pAdd, DWORD dwAddSize);
		bool			SeekIncludeToken(LPSTR* ppData, LPSTR pPtrEnd, DWORD* pdwAdvance, LPSTR* ppIncludeFilename);
		bool			MakeProgram(void);

		LPSTR			ReplaceTokens(LPSTR pFilename);

		bool			ProjectExists(void) { return m_bProjectExists; }
		bool			LoadProjectFile(LPSTR pFilename);
		bool			GetAllProjectFields(LPSTR pFilename);
		LPSTR			GetProjectFile(LPCSTR pFieldName);
		LPSTR			GetProjectMediaRoot(void);
		LPSTR			GetProjectField(LPCSTR pFieldName);
		bool			GetProjectState(LPCSTR pFieldName, bool bDefault);
		bool			GetProjectState(LPCSTR pFieldName);
		bool			GetProjectStateMatch(LPCSTR pFieldName, LPCSTR pCompareStr);
		DWORD			GetProjectDisplayInfo(LPCSTR pFieldName, DWORD dwDisplayItem);
		bool			FreeProjectFile(void);

		LPSTR			GetProgramName(void);

	public:
		DWORD			GetFileData(void) { return m_FileDataSize; }
		LPSTR			GetFilePtr(void) { return m_pFileData; }

	public:
		bool			PathExists(LPCSTR pPath);
		void			SetInternalFile(DWORD dwFileID, const char* pFilename);
		LPSTR			GetInternalFile(DWORD dwFileID);
		bool			FileExists(LPCSTR pFilename);
		void			GatherAllExternalWords(LPSTR pWordsFile);
		LPSTR			GetWord ( int iID );
		bool			EstablishRequiredBaseFiles(void);
		LPSTR			GetWordString(int id) { return m_pWord[id]; }

		bool			GetDebugMode(void) { return m_bDebugModeOn; }
		bool			GetRuntimeErrorMode(void) { return m_bRuntimeErrorsOn; }
		bool			GetProduceDBMFile(void) { return m_bProduceDBMFileOn; }
		bool			GetFullScreenMode(void) { return m_bFullScreenModeOn; }
		bool			GetFullDesktopMode(void) { return m_bFullDesktopModeOn; }
		bool			GetDesktopMode(void) { return m_bDesktopModeOn; }
		DWORD			GetStartDisplayWidth(void) { return m_dwStartDisplayWidth; }
		DWORD			GetStartDisplayHeight(void) { return m_dwStartDisplayHeight; }
		DWORD			GetStartDisplayDepth(void) { return m_dwStartDisplayDepth; }
		bool			GetHiddenMode(void) { return m_bHiddenModeOn; }
		bool			GetGenerateHelpTxtMode(void) { return m_bGenerateHelpTxtOn; }
		bool			GetEXEAloneState(void) { return m_bEXEAloneState; }
		bool			GetEXEInstallerState(void) { return m_bEXEInstallerState; }
		bool			GetCompressPCKState(void) { return m_bCompressPCKState; }
		bool			GetInternalMediaState(void) { return m_bInternalMediaState; }
		bool			GetEncryptionState(void) { return m_bEncryptionState; }
		bool			GetSpeedOverStabilityFlag(void) { return m_bSpeedOverStabilityState; }
		void SetExecutableOutputOverride(std::optional<std::filesystem::path> outputPath);
		bool PrepareExecutableOutputDirectory(void) const;
		void SetRuntimeRootOverride(std::optional<std::filesystem::path> runtimeRoot);
		void SetPackageKeyFile(std::optional<std::filesystem::path> keyFile);
		const std::optional<std::filesystem::path>& GetPackageKeyFile(void) const;
		bool ValidateRuntimeBundle(DWORD structurePatternCount);
		const ResolvedRuntimeBundle* GetResolvedRuntimeBundle(void) const;

	public:
		bool			RemoveAndRecordBreakpoints(void);
		bool			ClearBreakPointList(void);
		bool			AddToBreakPointList(DWORD dwLine);
		bool			FinishBreakPointList(void);

		DWORD			GetBreakPointIndex(void) { return m_dwBreakpointIndex; }
		DWORD			GetBreakPointLine(DWORD nIndex) { return m_BreakpointList[nIndex]; }
		void			IncBreakPointIndex(void) { m_dwBreakpointIndex++; }
		DWORD			GetBreakPointMax(void) { return m_dwBreakpointMax; }

	public:

		// Compiler Path Info
		std::unique_ptr<CStr>	m_pCompilerFilename;
		std::unique_ptr<CStr>	m_pCompilerPathOnly;

		// Original Source File Data for display in debugger
		DWORD			m_dwOriginalFileDataSize;
		std::vector<char>	m_OriginalFileData;

		// Main Source File Data for parsing
		DWORD			m_FileDataSize;
		LPSTR			m_pFileData;

		// Project File Data
		bool			m_bProjectExists;
		DWORD			m_ProjectFileDataSize;
		LPSTR			m_pProjectFileData;

		// Project Settings
		std::unique_ptr<CStr>	m_pAbsolutePathToProjectFile;
		std::unique_ptr<CStr>	m_pRelativePathToProjectFile;
		LPSTR			m_pFinalDBASource;
		LPSTR			m_pEXEFilename;
		bool			m_bSourceIsMMF;

		// Project Compiler Debug Settings
		bool			m_bDebugModeOn;
		bool			m_bRuntimeErrorsOn;
		bool			m_bProduceDBMFileOn;

		// Project Compiler Misc Settings
		bool			m_bFullScreenModeOn;
		bool			m_bFullDesktopModeOn;
		bool			m_bDesktopModeOn;
		DWORD			m_dwStartDisplayWidth;
		DWORD			m_dwStartDisplayHeight;
		DWORD			m_dwStartDisplayDepth;
		bool			m_bHiddenModeOn;
		bool			m_bGenerateHelpTxtOn;
		bool			m_bEXEAloneState;
		bool			m_bEXEInstallerState;
		bool			m_bCompressPCKState;
		bool			m_bInternalMediaState;
		bool			m_bEncryptionState;
		bool			m_bDoubleLiterals;
		bool			m_bSpeedOverStabilityState;
		bool			m_bRemoveSafetyCode;
		bool			m_bSafeArrays;
		bool			m_bLocalTempFolder;

		// Internal Files Database
		std::unique_ptr<CStr>	m_pInternalFile[PATH_MAX];

		// Breakpoint List Data
		DWORD			m_dwBreakpointSize;
		std::vector<DWORD>	m_BreakpointList;
		DWORD			m_dwBreakpointIndex;
		DWORD			m_dwBreakpointMax;

		// External Words Array
		char			m_pWord[EXTWORDSMAX][_MAX_PATH];

		// Exclusion Files
		DWORD			g_dwExcludeFilesCount;
		std::string		g_ExcludeFiles [ MAX_EXCLUSIONS ];

		CompilerContext* m_pContext;
		std::unique_ptr<CompilationInput> m_compilationInput;
		std::optional<std::filesystem::path> m_executableOutputOverride;
		std::string m_executableOutputOverrideText;
		std::optional<std::filesystem::path> m_runtimeRootOverride;
		std::optional<std::filesystem::path> m_packageKeyFile;
		std::optional<ResolvedRuntimeBundle> m_resolvedRuntimeBundle;
};

#endif // !defined(AFX_DBPCOMPILER_H__59BB1DE5_04A2_4BBC_9790_52F5C94E07F9__INCLUDED_)
