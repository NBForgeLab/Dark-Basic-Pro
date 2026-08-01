#include "ASMWriterPreparationServices.h"

#include "ASMWriter.h"
#include "DBPCompiler.h"
#include "EXEBlock.h"
#include "Error.h"
#include "PEBuilder.h"
#include "PreparedExecutableDebugger.h"
#include "StatementList.h"

#include <utility>

class CDataTable;
class CStructTable;
class CVarTable;

extern CDBPCompiler* g_pDBPCompiler;
extern CEXEBlock* g_pEXE;
extern CStatementList* g_pStatementList;
extern CDataTable* g_pDLLTable;
extern CDataTable* g_pCommandTable;
extern CDataTable* g_pStringTable;
extern CDataTable* g_pDataTable;
extern CVarTable* g_pVarTable;
extern CStructTable* g_pStructTable;
extern CError* g_pErrorReport;

namespace {

template <typename Operation>
[[nodiscard]] bool InvokeSafely(Operation&& operation) noexcept {
    try {
        return std::forward<Operation>(operation)();
    } catch (...) {
        return false;
    }
}

[[nodiscard]] bool HasCoreCollaborators() noexcept {
    return g_pDBPCompiler != nullptr && g_pEXE != nullptr &&
           g_pStatementList != nullptr;
}

} // namespace

ASMWriterPreparationServices::ASMWriterPreparationServices(
    CASMWriter& writer,
    IPreparedExecutableOutputServices* const outputServices) noexcept
    : writer_(writer), outputServices_(outputServices) {}

bool ASMWriterPreparationServices::ValidateTarget() noexcept {
    return HasCoreCollaborators() &&
           CPEBuilder{}.ValidatePEHeaderRequirements(0x00400000U, 4096U, 512U);
}

bool ASMWriterPreparationServices::UpdateMachineCode() noexcept {
    return g_pEXE != nullptr && InvokeSafely([this] {
        return writer_.UpdateMCB(writer_.GetCurrentMCPosition());
    });
}

bool ASMWriterPreparationServices::UpdateReferences() noexcept {
    return g_pEXE != nullptr &&
           InvokeSafely([this] { return writer_.UpdateMCBRefData(); });
}

bool ASMWriterPreparationServices::UpdateDllData() noexcept {
    return g_pEXE != nullptr && g_pStatementList != nullptr &&
           g_pDLLTable != nullptr &&
           InvokeSafely([this] { return writer_.UpdateDLLData(); });
}

bool ASMWriterPreparationServices::UpdateCommandData() noexcept {
    return g_pEXE != nullptr && g_pStatementList != nullptr &&
           g_pCommandTable != nullptr &&
           InvokeSafely([this] { return writer_.UpdateCommandData(); });
}

bool ASMWriterPreparationServices::UpdateStringData() noexcept {
    return g_pEXE != nullptr && g_pStatementList != nullptr &&
           g_pStringTable != nullptr &&
           InvokeSafely([this] { return writer_.UpdateStringData(); });
}

bool ASMWriterPreparationServices::UpdateDataData() noexcept {
    return g_pEXE != nullptr && g_pStatementList != nullptr &&
           g_pDataTable != nullptr &&
           InvokeSafely([this] { return writer_.UpdateDataData(); });
}

bool ASMWriterPreparationServices::UpdateDynamicData() noexcept {
    return g_pEXE != nullptr && g_pVarTable != nullptr &&
           InvokeSafely([this] { return writer_.UpdateDynamicData(); });
}

bool ASMWriterPreparationServices::UpdateStructurePatterns() noexcept {
    return g_pEXE != nullptr && g_pStructTable != nullptr &&
           InvokeSafely(
               [this] { return writer_.UpdateStructurePatternData(); });
}

bool ASMWriterPreparationServices::ValidateRuntime() noexcept {
    return g_pDBPCompiler != nullptr && g_pEXE != nullptr &&
           InvokeSafely([] {
               return g_pDBPCompiler->ValidateRuntimeBundle(
                   g_pEXE->m_dwUsertypeStringPatternQuantity);
           });
}

bool ASMWriterPreparationServices::FinalizeSpaceSizes() noexcept {
    if (g_pEXE == nullptr || g_pStatementList == nullptr) {
        return false;
    }

    g_pEXE->m_dwVariableSpaceSize =
        g_pStatementList->GetVarOffsetCounter();
    g_pEXE->m_dwDataSpaceSize =
        g_pStatementList->GetDataIndexCounter() * 10U;
    return true;
}

bool ASMWriterPreparationServices::RunDebug(
    const ExecutablePreparationRequest& request) noexcept {
    if (outputServices_ != nullptr) {
        return outputServices_->RunDebug(writer_, request);
    }

    ASMWriterDebugRuntime runtime(writer_);
    return PreparedExecutableDebugger{}.Run(
        {request.parsingMainProgram, request.hasNewCode}, runtime);
}

bool ASMWriterPreparationServices::PackageStandalone(
    const ExecutablePreparationRequest& request) noexcept {
    return outputServices_ != nullptr &&
           outputServices_->PackageStandalone(writer_, request);
}

void ASMWriterPreparationServices::ReportFailure(
    const ExecutablePreparationResult& result) const noexcept {
    if (g_pErrorReport != nullptr && result.message != nullptr) {
        g_pErrorReport->AddErrorString(const_cast<char*>(result.message));
    }
}
