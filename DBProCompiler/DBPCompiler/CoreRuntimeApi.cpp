#include "CoreRuntimeApi.h"

namespace {

CoreApiResult Missing(const char* exportName) {
    return CoreApiResult::Failure({
        CoreApiErrorCode::MissingRequiredExport,
        exportName,
        std::string("DBProCore.dll is missing required export '") +
            exportName + "'."});
}

template <typename T>
T Resolve(const CoreSymbolLookup& lookup, const char* name) {
    return reinterpret_cast<T>(lookup(name));
}

} // namespace

CoreApiResult ResolveCoreRuntimeApi(
    const CoreSymbolLookup& lookup,
    const ProgramRuntimeRequirements& requirements) {
    CoreRuntimeApi api;
    api.passCommandLine = Resolve<CorePassPointer>(
        lookup, "?PassCmdLineHandlerPtr@@YAXPAX@Z");
    if (!api.passCommandLine) return Missing("?PassCmdLineHandlerPtr@@YAXPAX@Z");
    api.passError = Resolve<CorePassPointer>(
        lookup, "?PassErrorHandlerPtr@@YAXPAX@Z");
    if (!api.passError) return Missing("?PassErrorHandlerPtr@@YAXPAX@Z");
    api.passEscape = Resolve<CorePassPointer>(lookup, "?PassEscapePtr@@YAXPAX@Z");
    if (!api.passEscape) return Missing("?PassEscapePtr@@YAXPAX@Z");
    api.passBreakout = Resolve<CorePassPointer>(lookup, "?PassBreakOutPtr@@YAXPAX@Z");
    if (!api.passBreakout) return Missing("?PassBreakOutPtr@@YAXPAX@Z");
    api.passDataStatements = Resolve<CorePassDataStatements>(
        lookup, "?PassDataStatementPtr@@YAXPAD0@Z");
    if (!api.passDataStatements) return Missing("?PassDataStatementPtr@@YAXPAD0@Z");

    api.passStructurePatterns = Resolve<CorePassStructurePatterns>(
        lookup, "?PassStructurePatterns@@YAXPAXK@Z");
    if (requirements.count(RuntimeCapability::CoreStructurePatternsV1) != 0 &&
        !api.passStructurePatterns) {
        return Missing("?PassStructurePatterns@@YAXPAXK@Z");
    }

#define RESOLVE_REQUIRED(member, type, name) \
    api.member = Resolve<type>(lookup, name); \
    if (!api.member) return Missing(name)
    RESOLVE_REQUIRED(passDlls, CoreVoid, "?PassDLLs@@YAXXZ");
    RESOLVE_REQUIRED(constructDlls, CoreVoid, "?ConstructDLLs@@YAXXZ");
    RESOLVE_REQUIRED(getGlob, CoreDword, "?GetGlobPtr@@YAKXZ");
    RESOLVE_REQUIRED(initializeDisplay, CoreInitializeDisplay,
                     "?InitDisplay@@YAKKKKKPAUHINSTANCE__@@PAD@Z");
    RESOLVE_REQUIRED(closeDisplay, CoreDword, "?CloseDisplay@@YAKXZ");
    RESOLVE_REQUIRED(createVariableSpace, CoreDwordParameter,
                     "?CreateVariableSpace@@YAKK@Z");
    RESOLVE_REQUIRED(deleteVariableSpace, CoreVoid,
                     "?DeleteVariableSpace@@YAXXZ");
    RESOLVE_REQUIRED(createDataSpace, CoreDwordParameter,
                     "?CreateDataSpace@@YAKK@Z");
    RESOLVE_REQUIRED(deleteDataSpace, CoreVoid, "?DeleteDataSpace@@YAXXZ");
    RESOLVE_REQUIRED(deleteVariableItem, CoreVoidDwordPointer,
                     "?DeleteSingleVariableAllocation@@YAXPAK@Z");
    RESOLVE_REQUIRED(unDim, CoreVoidDwordPointer, "?UnDimDD@@YAKK@Z");
    RESOLVE_REQUIRED(sync, CoreVoid, "?Sync@@YAXXZ");
#undef RESOLVE_REQUIRED
    return CoreApiResult::Success(std::move(api));
}
