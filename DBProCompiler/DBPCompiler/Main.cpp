//
// Main Compiler
//

// Common Includes
#include "macros.h"
#include <windows.h>
#include "DebugInfo.h"
#include "Error.h"
#include <direct.h>
#include <ctime>

// Custom Includes
#include "Str.h"
#include "DBPCompiler.h"
#include "DBPLogger.h"
#include "CrashHandler.h"
#include "CompilerArguments.h"
#include "TextConvert.h"

#include <DB3Time.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cctype>

// Internal data
HWND g_hTempWindow = nullptr;
HWND g_igLoader_HWND = nullptr;
char g_ActualCompilerFilename[256] = {};

// External Class Pointers
extern CDBPCompiler*		g_pDBPCompiler;
extern CError*				g_pErrorReport;
extern CDebugInfo			g_DebugInfo;

HRESULT GetDXVersion([[maybe_unused]] DWORD* pdwDirectXVersion, [[maybe_unused]] TCHAR* strDirectXVersion, [[maybe_unused]] int cchDirectXVersion)
{
	return S_OK;
}

LRESULT CALLBACK WindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch( message )
    {
		case WM_USER+51:
			{
				// Memory to be used to store string sent
				DWORD dwDataSize=0;
				LPSTR pData=nullptr;

				// First Four Bytes are Size of Message
				HANDLE hFileMap = OpenFileMappingW(FILE_MAP_READ,FALSE,L"DBPROCLITEXT");
				if(hFileMap)
				{
					LPVOID lpVoid = MapViewOfFile(hFileMap,FILE_MAP_READ,0,0,4);
					if(lpVoid)
					{
						DWORD dwUserMessageSize = *(DWORD*)lpVoid;
						UnmapViewOfFile(lpVoid);
						CloseHandle(hFileMap);

						// Open Message
						hFileMap = OpenFileMappingW(FILE_MAP_READ,FALSE,L"DBPROCLITEXT");
						lpVoid = MapViewOfFile(hFileMap,FILE_MAP_READ,0,0,dwUserMessageSize+4);
						if(lpVoid)
						{
							// Place data into datamem
							dwDataSize = dwUserMessageSize;
							pData = new char[dwUserMessageSize+1];
							ZeroMemory(pData, dwUserMessageSize+1);
							memcpy(pData, (LPSTR)lpVoid+4, dwUserMessageSize);

							// Close Message
							UnmapViewOfFile(lpVoid);
						}
					}
					CloseHandle(hFileMap);
				}

				// Set CLI Text to be used by parser-loop
				g_DebugInfo.SetMessageArrived(true);
				g_DebugInfo.SetCLISize(dwDataSize);
				g_DebugInfo.SetCLIText(pData);
			}
			return TRUE;

		case WM_CLOSE:
			PostQuitMessage(0);
			return TRUE;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;
    }

	// Default Action
    return DefWindowProc(hWnd, message, wParam, lParam);
}


void PrintHelp() {
    AttachConsole(ATTACH_PARENT_PROCESS);
    std::cout << "\nDarkBasic Pro Compiler (Modernized CLI Version)\n";
    std::cout << "Usage: DBPCompiler.exe [options] <project_file.dbpro>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help      Show this help message and exit\n";
    std::cout << "  --json          Output compiler status and diagnostics as streaming JSON lines\n";
    std::cout << "  --emit-final-source   Atomically publish the project's final source artifact\n";
    std::cout << "  --legacy-final-source Compile an existing final source artifact without assembly\n";
    std::cout << "  --runtime-root <path> Select and validate a DBPro runtime bundle\n";
    std::cout << "  --output <path>       Write the generated executable to an isolated path\n";
    std::cout << "  --package-key-file <path> Use an exact 32-byte binary package key file\n";
    std::cout << "\nExample:\n";
    std::cout << "  DBPCompiler.exe --json \"D:\\Projects\\MyGame\\project.dbpro\"\n\n" << std::flush;
}

// Program Code
int WINAPI WinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance, [[maybe_unused]] LPSTR lpCmdLine, [[maybe_unused]] int nCmdShow)
{
	db3::SetupDiagnosticHandlers();

	// Parse command line early using CommandLineToArgvW to configure logger & headless mode
	bool bJsonMode = false;
	spdlog::level::level_enum logLevel = spdlog::level::info;
	std::string logFilePath = "dbp.log";
	int nEarlyArgs = 0;
	LPWSTR* szEarlyArglist = CommandLineToArgvW(GetCommandLineW(), &nEarlyArgs);
	if (szEarlyArglist != nullptr) {
		for (int i = 1; i < nEarlyArgs; i++) {
			const std::wstring_view argW = szEarlyArglist[i];
			if (argW == L"--json") {
				bJsonMode = true;
				g_bJsonDiagnostics = true;
				db3::g_bHeadlessMode = true;
				AttachConsole(ATTACH_PARENT_PROCESS);
			} else if (argW == L"--trace") {
				logLevel = spdlog::level::trace;
			} else if (argW == L"--debug") {
				logLevel = spdlog::level::debug;
			} else if (argW == L"--log-level" && i + 1 < nEarlyArgs) {
				const std::wstring_view levelStr = szEarlyArglist[++i];
				if (levelStr == L"trace") logLevel = spdlog::level::trace;
				else if (levelStr == L"debug") logLevel = spdlog::level::debug;
				else if (levelStr == L"info") logLevel = spdlog::level::info;
				else if (levelStr == L"warn" || levelStr == L"warning") logLevel = spdlog::level::warn;
				else if (levelStr == L"err" || levelStr == L"error") logLevel = spdlog::level::err;
				else if (levelStr == L"off") logLevel = spdlog::level::off;
			} else if (argW == L"--log-file" && i + 1 < nEarlyArgs) {
				const wchar_t* pathStr = szEarlyArglist[++i];
				char pathA[MAX_PATH] = {};
				WideCharToMultiByte(CP_UTF8, 0, pathStr, -1, pathA, sizeof(pathA), NULL, NULL);
				logFilePath = pathA;
			}
		}
		LocalFree(szEarlyArglist);
	}

	// Initialize logger
	DBPLogger::Initialize(logFilePath, bJsonMode, logLevel);
	DBP_INFO("DarkBasic Pro Compiler initialized.");

#if _DEBUG
	db3::CProfile<> mainProf("WinMain:Debug");
#else
	db3::CProfile<> mainProf("WinMain:Release");
#endif

	// Vars
	WNDCLASSA wc = {};

	// Register window
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = nullptr;
    wc.hCursor = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = "DBProCompiler";
    RegisterClassA( &wc );

	// Create Hidden Window
	HWND hCompilerWnd = CreateWindowA(	"DBProCompiler",
										"DBProCompiler",
										0,
										0,
										0,
										0,
										0,
										nullptr,
										nullptr,
										hInstance,
										nullptr);

	// Error Handler
	g_pErrorReport = new CError;

	// Compiler needs to know where it is (file dependent)
	{
		wchar_t wPath[MAX_PATH];
		GetModuleFileNameW(hInstance, wPath, MAX_PATH);
		std::string utf8Path = TextConvert::UTF16ToUTF8(wPath);
		strncpy_s(g_ActualCompilerFilename, sizeof(g_ActualCompilerFilename), utf8Path.c_str(), _TRUNCATE);
	}

	bool bOverallSuccess = false;

	g_pDBPCompiler = new CDBPCompiler(g_ActualCompilerFilename);
	if(g_pDBPCompiler)
	{
		int nArgs = 0;
		LPWSTR* szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
		std::vector<std::wstring> arguments;
		if (szArglist != nullptr) {
			for (int i = 0; i < nArgs; i++) {
				arguments.emplace_back(szArglist[i]);
			}
			LocalFree(szArglist);
		}
		const auto parsedArguments = ParseWideCompilerArguments(arguments);
		const std::string argumentError = parsedArguments
			? std::string()
			: parsedArguments.error();
		if (!argumentError.empty()) {
			if (g_bJsonDiagnostics || db3::g_bHeadlessMode)
				std::cout << "{\"type\":\"error\",\"stage\":\"arguments\",\"message\":\""
					<< EscapeJSON(argumentError) << "\"}\n" << std::flush;
			else
				MessageBoxW(nullptr, TextConvert::UTF8ToUTF16(argumentError).c_str(), L"Compiler Error", MB_OK);
			SafeDelete(g_pDBPCompiler);
			SafeDelete(g_pErrorReport);
			return 1;
		}
		if (parsedArguments.value().help) {
			PrintHelp();
			SafeDelete(g_pDBPCompiler);
			SafeDelete(g_pErrorReport);
			return 0;
		}
		g_bJsonDiagnostics = parsedArguments.value().json;
		db3::g_bHeadlessMode = parsedArguments.value().json;
		const std::string projectPath = parsedArguments.value().inputPath.string();
		const bool emitFinalSource = parsedArguments.value().emitFinalSource;
		const bool legacyFinalSource = parsedArguments.value().legacyFinalSource;
		g_pDBPCompiler->SetRuntimeRootOverride(parsedArguments.value().runtimeRoot);
		g_pDBPCompiler->SetExecutableOutputOverride(parsedArguments.value().outputPath);
		g_pDBPCompiler->SetPackageKeyFile(parsedArguments.value().packageKeyFile);

		if (projectPath.empty()) {
			if (g_bJsonDiagnostics) {
				std::cout << "{\"type\":\"error\",\"message\":\"No project file specified.\"}\n" << std::flush;
			} else {
				MessageBoxW(nullptr, L"No project file specified.\nUsage: DBPCompiler.exe [options] <project_file.dbpro>", L"Compiler Error", MB_OK);
			}
			SafeDelete(g_pDBPCompiler);
			SafeDelete(g_pErrorReport);
			return 1;
		}

		if (g_bJsonDiagnostics) {
			std::cout << "{\"type\":\"status\",\"stage\":\"debug\",\"message\":\"Parsed project path: " << EscapeJSON(projectPath) << "\"}\n" << std::flush;
		}

		CStr strProjectFilename(const_cast<char*>(projectPath.c_str()));
		if(strProjectFilename.Length()>0)
		{
			// Load All Required Internal Files
			if(g_pDBPCompiler->EstablishRequiredBaseFiles())
			{
				// Certificate system removed - open-source project, always valid
				bool bCompileStepsSuccess = true;

				// Read in Project File
				if (bCompileStepsSuccess)
				{
					ReportStatus("project_manifest", "Loading project file...");
					db3::CProfile<> prof("CDBPCompiler::LoadProjectFile");
					bCompileStepsSuccess = g_pDBPCompiler->LoadProjectFile(strProjectFilename.GetStr());
				}

				// Load in all data from fields
				if (bCompileStepsSuccess)
				{
					ReportStatus("project_fields", "Loading project configuration fields...");
					db3::CProfile<> prof("CDBPCompiler::GetAllProjectFields");
					bCompileStepsSuccess = g_pDBPCompiler->GetAllProjectFields(strProjectFilename.GetStr());
				}

				if (bCompileStepsSuccess && !legacyFinalSource)
				{
					ReportStatus("source_assembly", "Assembling project source files...");
					bCompileStepsSuccess = g_pDBPCompiler->PrepareCompilationInput(
						strProjectFilename.GetStr(), emitFinalSource);
				}

				if (bCompileStepsSuccess)
				{
					ReportStatus("output_directory", "Preparing executable output directory...");
					bCompileStepsSuccess = g_pDBPCompiler->PrepareExecutableOutputDirectory();
					if(!bCompileStepsSuccess)
						g_pErrorReport->AddErrorString("Failed to prepare executable output directory.");
				}

				// Prepare Compiler With Debug Info
				if (bCompileStepsSuccess)
				{
					db3::CProfile<> prof("CDBPCompiler::SetDebugMode");
					g_DebugInfo.SetDebugMode(g_pDBPCompiler->GetDebugMode(), hInstance);
				}

				// Create EXE from DBA Filename
				if (bCompileStepsSuccess)
				{
					ReportStatus("compile_start", "Compiling project source files...");
					db3::CProfile<> prof("CDBPCompiler::PerformCompileOnProject");
					bCompileStepsSuccess = g_pDBPCompiler->PerformCompileOnProject();
				}

				// Free usages
				{
					db3::CProfile<> prof("CDBPCompiler::FreeProjectFile");
					g_pDBPCompiler->FreeProjectFile();
				}

				if (bCompileStepsSuccess && !(g_pErrorReport && g_pErrorReport->IsError())) {
					ReportStatus("success", "Compilation finished successfully.");
					bOverallSuccess = true;
				} else {
					if (g_bJsonDiagnostics && g_pErrorReport) {
						const char* errMsg = g_pErrorReport->IsParserError() ? g_pErrorReport->GetParserErrorString() : g_pErrorReport->GetErrorString();
						if (!errMsg) errMsg = "Unknown compilation error";
						std::cout << "{\"type\":\"error\",\"message\":\"" << EscapeJSON(errMsg) << "\"}\n" << std::flush;
					}
					ReportStatus("failed", "Compilation failed.");
				}
			}
			else
			{
				if (g_bJsonDiagnostics) {
					std::cout << "{\"type\":\"error\",\"message\":\"Failed to establish required base files.\"}\n" << std::flush;
				} else {
					MessageBoxW(nullptr, L"Failed to establish required base files.", L"Compiler Error", MB_OK);
				}
			}
		}

		// Delete DBPCompiler Object
		delete g_pDBPCompiler;
		g_pDBPCompiler = nullptr;
	}

	// Determine exit code
	int exitCode = bOverallSuccess ? 0 : 1;

	// Delete Error Object
	delete g_pErrorReport;
	g_pErrorReport = nullptr;

	// DestroyWindow
	if(hCompilerWnd)
	{
		DestroyWindow(hCompilerWnd);
		hCompilerWnd = nullptr;
	}

	// Exit
	return exitCode;
}
