#include "PreparedExecutableDebugger.h"

#include "ASMWriter.h"
#include "DBPCompiler.h"
#include "DebuggerInterface.h"
#include "DebugInfo.h"
#include "EXEBlock.h"
#include "TextConvert.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>

extern CDBPCompiler* g_pDBPCompiler;
extern CEXEBlock* g_pEXE;
extern CDebugInfo g_DebugInfo;
extern DWORD_PTR g_dwEscapeValueMem;
extern DWORD_PTR g_dwBreakOutPosition;
extern LPSTR g_pVarSpaceAddressInUse;
extern DWORD g_dwVarSpaceSizeInUse;
extern GDI_RetVoidParamVoidPFN g_CORE_SyncRefresh;

LRESULT DebugHookStatementFunctionCall(
    DWORD program, DWORD line, DWORD start, DWORD end);
void DebugHookJumpFunctionCall(
    DWORD program, DWORD line, DWORD start, DWORD end);
void DebugHookReturnFunctionCall();

namespace {

[[nodiscard]] bool HasDebugCollaborators() noexcept {
    return g_pDBPCompiler != nullptr && g_pEXE != nullptr;
}

[[nodiscard]] bool StoreBreakAddress() noexcept {
    if (g_dwBreakOutPosition == 0U) {
        return true;
    }

    const auto base = reinterpret_cast<std::uintptr_t>(
        g_pEXE->m_pMachineCodeBlock);
    const auto maximum = (std::numeric_limits<DWORD_PTR>::max)() - base;
    if (g_dwBreakOutPosition > maximum) {
        return false;
    }

    g_dwBreakOutPosition = base + g_dwBreakOutPosition;
    return true;
}

} // namespace

bool PreparedExecutableDebugger::Run(
    const PreparedExecutableDebugRequest& request,
    IPreparedExecutableDebugRuntime& runtime) const noexcept {
    if (!runtime.BeginSession()) {
        return false;
    }

    bool succeeded = false;
    if (request.parsingMainProgram) {
        succeeded = runtime.InitializeMain() && runtime.RunMain();
    } else {
        succeeded = runtime.InitializeMini();
        if (succeeded && request.hasNewCode) {
            succeeded = runtime.RunNewCode();
        }
        if (succeeded) {
            succeeded = runtime.ResumeMain();
        }
    }

    return runtime.EndSession() && succeeded;
}

ASMWriterDebugRuntime::ASMWriterDebugRuntime(CASMWriter& writer) noexcept
    : writer_(writer) {}

ASMWriterDebugRuntime::~ASMWriterDebugRuntime() = default;

bool ASMWriterDebugRuntime::BeginSession() noexcept {
    if (!HasDebugCollaborators()) {
        return false;
    }

    try {
        HWND debuggerWindow = FindWindowW(nullptr, L"DBProDebugger");
        if (debuggerWindow == nullptr &&
            CDebuggerInterface::IsInternalDebuggerActive()) {
            g_dwEscapeValueMem = 2U;
            return false;
        }

        if (debuggerWindow == nullptr) {
            STARTUPINFOW startupInfo{};
            startupInfo.cb = sizeof(startupInfo);
            auto& processInfo =
                CDebuggerInterface::GetDebuggerProcessInfo();
            processInfo = {};

            std::wstring commandLine = TextConvert::UTF8ToUTF16(
                g_pDBPCompiler->GetInternalFile(PATH_DEBUGGERFILE));
            const std::wstring compilerDirectory =
                TextConvert::UTF8ToUTF16(
                    g_pDBPCompiler->GetInternalFile(PATH_ROOTPATH));
            if (commandLine.empty() ||
                !CreateProcessW(
                    nullptr,
                    commandLine.data(),
                    nullptr,
                    nullptr,
                    FALSE,
                    NORMAL_PRIORITY_CLASS,
                    nullptr,
                    compilerDirectory.empty()
                        ? nullptr
                        : compilerDirectory.c_str(),
                    &startupInfo,
                    &processInfo)) {
                return false;
            }

            WaitForInputIdle(processInfo.hProcess, 5000U);
            CDebuggerInterface::SetInternalDebuggerActive(true);

            std::unique_ptr<char[]> mediaRoot(
                g_pDBPCompiler->GetProjectMediaRoot());
            if (mediaRoot != nullptr && mediaRoot[0] != '\0') {
                std::error_code error;
                std::filesystem::current_path(mediaRoot.get(), error);
                if (error) {
                    return false;
                }
            }
        }

        LPSTR programData = g_DebugInfo.GetProgramPtr();
        const DWORD programSize = g_DebugInfo.GetProgramSize();
        if (programData != nullptr && programSize > 0U) {
            CDebuggerInterface::HideAnyHiddenCode(
                programData, programSize);
            CDebuggerInterface::SendDataToDebugger(
                1U, programData, programSize);
        }

        DWORD variableDataSize = 0U;
        std::unique_ptr<char[]> variableData(
            writer_.MakeVarDataForTransfer(&variableDataSize));
        CDebuggerInterface::SendDataToDebugger(
            2U, variableData.get(), variableDataSize);

        LPSTR programName = g_pDBPCompiler->GetProgramName();
        if (programName == nullptr) {
            return false;
        }
        CDebuggerInterface::SendDataToDebugger(
            3U,
            programName,
            static_cast<DWORD>(std::strlen(programName)));

        if (!writer_.ReportAnyErrorsToCLI()) {
            return false;
        }

        statementHook_ = reinterpret_cast<void*>(
            DebugHookStatementFunctionCall);
        jumpHook_ = reinterpret_cast<void*>(DebugHookJumpFunctionCall);
        returnHook_ = reinterpret_cast<void*>(DebugHookReturnFunctionCall);

        g_pEXE->m_UnpackFolderName =
            g_pDBPCompiler->GetInternalFile(PATH_PLUGINSFOLDER);
        g_pEXE->m_dwEncryptionKey = 0U;
        g_pEXE->StartInfo(
            const_cast<char*>(g_pEXE->m_UnpackFolderName.c_str()),
            g_pEXE->m_dwEncryptionKey);
        return true;
    } catch (...) {
        return false;
    }
}

bool ASMWriterDebugRuntime::InitializeMain() noexcept {
    if (!HasDebugCollaborators()) {
        return false;
    }

    executionResult_ = g_pEXE->InitDebug(
        g_DebugInfo.GetInstance(),
        statementHook_,
        jumpHook_,
        returnHook_,
        executionResult_,
        &returnError_,
        nullptr,
        false);
    g_pVarSpaceAddressInUse = g_pEXE->m_pVariableSpace;
    g_dwVarSpaceSizeInUse = g_pEXE->m_dwVariableSpaceSize;
    return executionResult_;
}

bool ASMWriterDebugRuntime::RunMain() noexcept {
    if (g_pEXE == nullptr) {
        return false;
    }
    executionResult_ = g_pEXE->Run(executionResult_);
    return executionResult_;
}

bool ASMWriterDebugRuntime::InitializeMini() noexcept {
    if (g_pEXE == nullptr) {
        return false;
    }

    executionResult_ = g_pEXE->InitMini(
        statementHook_,
        jumpHook_,
        returnHook_,
        executionResult_,
        &returnError_);
    g_pVarSpaceAddressInUse = g_pEXE->m_pVariableSpace;
    g_dwVarSpaceSizeInUse = g_pEXE->m_dwVariableSpaceSize;
    return executionResult_;
}

bool ASMWriterDebugRuntime::RunNewCode() noexcept {
    if (g_pEXE == nullptr) {
        return false;
    }

    const DWORD_PTR storedBreakPosition = g_dwBreakOutPosition;
    g_dwEscapeValueMem = 0U;
    g_dwBreakOutPosition = 0U;
    executionResult_ = g_pEXE->RunFrom(
        executionResult_, g_pEXE->m_dwStartOfMiniMC);
    g_dwBreakOutPosition = storedBreakPosition;
    return executionResult_;
}

bool ASMWriterDebugRuntime::ResumeMain() noexcept {
    if (g_pEXE == nullptr || !StoreBreakAddress()) {
        return false;
    }

    if (g_CORE_SyncRefresh != nullptr) {
        g_CORE_SyncRefresh();
        g_CORE_SyncRefresh();
    }
    g_dwEscapeValueMem = 1U;
    if (!writer_.ReportAnyErrorsToCLI()) {
        return false;
    }

    executionResult_ = g_pEXE->Run(executionResult_);
    return executionResult_;
}

bool ASMWriterDebugRuntime::EndSession() noexcept {
    if (g_pEXE != nullptr && g_dwEscapeValueMem == 3U) {
        g_dwEscapeValueMem = 1U;
        g_pEXE->m_dwRuntimeErrorDWORD = 1U;
        g_pEXE->m_dwRuntimeErrorLineDWORD = 0U;
    }

    if (CDebuggerInterface::IsInternalDebuggerActive() &&
        g_dwEscapeValueMem == 2U) {
        auto& processInfo = CDebuggerInterface::GetDebuggerProcessInfo();
        DWORD exitCode = 0U;
        if (processInfo.hProcess != nullptr &&
            GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
            TerminateProcess(processInfo.hProcess, exitCode);
        }
    }

    returnErrorOwner_.reset(returnError_);
    returnError_ = nullptr;
    return true;
}
