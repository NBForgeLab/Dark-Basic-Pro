#pragma once

// Common Includes
#include "ParserHeader.h"
#include "RuntimeBundleResolver.h"
#include "Str.h"
#include <memory>
#include <vector>
#include <string>

// Internal file-table identifiers (constexpr, type-safe)
namespace dbp_paths {
constexpr uint32_t MAX = 20;
constexpr uint32_t ROOTPATH = 1;
constexpr uint32_t SETUPFILE = 2;
constexpr uint32_t ERRORSFILE = 3;
constexpr uint32_t PLUGINSFOLDER = 4;
constexpr uint32_t TEMPFOLDER = 5;
constexpr uint32_t TEMPDBMFILE = 6;
constexpr uint32_t TEMPEXBFILE = 7;
constexpr uint32_t TEMPERRORFILE = 8;
constexpr uint32_t DEBUGGERFILE = 9;
constexpr uint32_t WORDSFILE = 10;
constexpr uint32_t PLUGINSUSERFOLDER = 11;
constexpr uint32_t PLUGINSLICENSEDFOLDER = 12;
constexpr uint32_t CURRENTFOLDER = 13;
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
		CDBPCompiler(const char* pCompilerFilename);
		virtual ~CDBPCompiler();

	public:
		bool			PerformCompileOnProject(void);
		bool			PrepareCompilationInput(const char* pInputFilename, bool emitFinalSource = false);
		bool			LoadPreparedSource(void);
		bool			LoadDBA(const char* pDBAFilename);
		bool			LoadRaw(const char* pDBAFilename, char** ppData, uint32_t* pdwDataSize);
		bool			LoadRawFromMMF(const char* pDBAFilename, char** ppData, uint32_t* pdwDataSize);
		bool			UnfoldFileDataIncludes(void);
		void			EnsureDataMemBugEnough(char* pPtr, uint32_t dwPredictSize, char** pNewData, uint32_t* dwNewDataSize, char** pWritePtrOut);
		bool			UnfoldFileDataConstants(void);
		bool			CopyData(char** ppData, uint32_t* pdwDataSize, const char* pAdd, uint32_t dwAddSize);
		bool			SeekIncludeToken(char** ppData, char* pPtrEnd, uint32_t* pdwAdvance, char** ppIncludeFilename);
		bool			MakeProgram(void);

		char*			ReplaceTokens(const char* pFilename);

		bool			ProjectExists(void) const noexcept { return m_bProjectExists; }
		bool			LoadProjectFile(const char* pFilename);
		bool			GetAllProjectFields(const char* pFilename);
		char*			GetProjectFile(std::string_view fieldName);
		char*			GetProjectFile(const char* pFieldName) {
			return pFieldName ? GetProjectFile(std::string_view(pFieldName)) : nullptr;
		}
		char*			GetProjectMediaRoot(void);
		char*			GetProjectField(std::string_view fieldName);
		char*			GetProjectField(const char* pFieldName) {
			return pFieldName ? GetProjectField(std::string_view(pFieldName)) : nullptr;
		}
		bool			GetProjectState(std::string_view fieldName, bool bDefault);
		bool			GetProjectState(std::string_view fieldName);
		bool			GetProjectStateMatch(std::string_view fieldName, std::string_view compareStr);
		uint32_t		GetProjectDisplayInfo(std::string_view fieldName, uint32_t dwDisplayItem);
		bool			FreeProjectFile(void);

		char*			GetProgramName(void);

	public:
		uint32_t		GetFileData(void) const noexcept { return m_FileDataSize; }
		char*			GetFilePtr(void) const noexcept { return m_pFileData; }

	public:
		bool			PathExists(std::string_view path);
		bool			PathExists(const char* pPath) {
			return pPath ? PathExists(std::string_view(pPath)) : false;
		}
		void			SetInternalFile(uint32_t dwFileID, const char* pFilename);
		char*			GetInternalFile(uint32_t dwFileID);
		bool			FileExists(std::string_view filename);
		bool			FileExists(const char* pFilename) {
			return pFilename ? FileExists(std::string_view(pFilename)) : false;
		}
		void			GatherAllExternalWords(const char* pWordsFile);
		char*			GetWord ( int iID );
		bool			EstablishRequiredBaseFiles(void);
		const char*		GetWordString(int id) const { return m_pWord[id]; }

		bool			GetDebugMode(void) const noexcept { return m_bDebugModeOn; }
		bool			GetRuntimeErrorMode(void) const noexcept { return m_bRuntimeErrorsOn; }
		bool			GetProduceDBMFile(void) const noexcept { return m_bProduceDBMFileOn; }
		bool			GetFullScreenMode(void) const noexcept { return m_bFullScreenModeOn; }
		bool			GetFullDesktopMode(void) const noexcept { return m_bFullDesktopModeOn; }
		bool			GetDesktopMode(void) const noexcept { return m_bDesktopModeOn; }
		uint32_t		GetStartDisplayWidth(void) const noexcept { return m_dwStartDisplayWidth; }
		uint32_t		GetStartDisplayHeight(void) const noexcept { return m_dwStartDisplayHeight; }
		uint32_t		GetStartDisplayDepth(void) const noexcept { return m_dwStartDisplayDepth; }
		bool			GetHiddenMode(void) const noexcept { return m_bHiddenModeOn; }
		bool			GetGenerateHelpTxtMode(void) const noexcept { return m_bGenerateHelpTxtOn; }
		bool			GetEXEAloneState(void) const noexcept { return m_bEXEAloneState; }
		bool			GetEXEInstallerState(void) const noexcept { return m_bEXEInstallerState; }
		bool			GetCompressPCKState(void) const noexcept { return m_bCompressPCKState; }
		bool			GetInternalMediaState(void) const noexcept { return m_bInternalMediaState; }
		bool			GetEncryptionState(void) const noexcept { return m_bEncryptionState; }
		bool			GetSpeedOverStabilityFlag(void) const noexcept { return m_bSpeedOverStabilityState; }
		void SetExecutableOutputOverride(std::optional<std::filesystem::path> outputPath);
		bool PrepareExecutableOutputDirectory(void) const;
		void SetRuntimeRootOverride(std::optional<std::filesystem::path> runtimeRoot);
		void SetPackageKeyFile(std::optional<std::filesystem::path> keyFile);
		const std::optional<std::filesystem::path>& GetPackageKeyFile(void) const;
		bool ValidateRuntimeBundle(uint32_t structurePatternCount);
		const ResolvedRuntimeBundle* GetResolvedRuntimeBundle(void) const;

	public:
		bool			RemoveAndRecordBreakpoints(void);
		bool			ClearBreakPointList(void);
		bool			AddToBreakPointList(uint32_t dwLine);
		bool			FinishBreakPointList(void);

		uint32_t		GetBreakPointIndex(void) const noexcept { return m_dwBreakpointIndex; }
		uint32_t		GetBreakPointLine(uint32_t nIndex) const { return m_BreakpointList[nIndex]; }
		void			IncBreakPointIndex(void) noexcept { m_dwBreakpointIndex++; }
		uint32_t		GetBreakPointMax(void) const noexcept { return m_dwBreakpointMax; }

	public:

		// Compiler Path Info
		std::unique_ptr<CStr>	m_pCompilerFilename;
		std::unique_ptr<CStr>	m_pCompilerPathOnly;

		// Original Source File Data for display in debugger
		uint32_t		m_dwOriginalFileDataSize;
		std::vector<char>	m_OriginalFileData;

		// Main Source File Data for parsing
		uint32_t		m_FileDataSize;
		char*			m_pFileData;

		// Project File Data
		bool			m_bProjectExists;
		uint32_t		m_ProjectFileDataSize;
		char*			m_pProjectFileData;

		// Project Settings
		std::unique_ptr<CStr>	m_pAbsolutePathToProjectFile;
		std::unique_ptr<CStr>	m_pRelativePathToProjectFile;
		char*			m_pFinalDBASource;
		char*			m_pEXEFilename;
		bool			m_bSourceIsMMF;

		// Project Compiler Debug Settings
		bool			m_bDebugModeOn;
		bool			m_bRuntimeErrorsOn;
		bool			m_bProduceDBMFileOn;

		// Project Compiler Misc Settings
		bool			m_bFullScreenModeOn;
		bool			m_bFullDesktopModeOn;
		bool			m_bDesktopModeOn;
		uint32_t		m_dwStartDisplayWidth;
		uint32_t		m_dwStartDisplayHeight;
		uint32_t		m_dwStartDisplayDepth;
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
		uint32_t		m_dwBreakpointSize;
		std::vector<uint32_t>	m_BreakpointList;
		uint32_t		m_dwBreakpointIndex;
		uint32_t		m_dwBreakpointMax;

		// External Words Array
		char			m_pWord[EXTWORDSMAX][_MAX_PATH];

		// Exclusion Files
		uint32_t		g_dwExcludeFilesCount;
		std::string		g_ExcludeFiles [ MAX_EXCLUSIONS ];

		std::unique_ptr<CompilerContext> m_pContext;
		std::unique_ptr<CompilationInput> m_compilationInput;
		std::optional<std::filesystem::path> m_executableOutputOverride;
		std::string m_executableOutputOverrideText;
		std::optional<std::filesystem::path> m_runtimeRootOverride;
		std::optional<std::filesystem::path> m_packageKeyFile;
		std::optional<ResolvedRuntimeBundle> m_resolvedRuntimeBundle;
};