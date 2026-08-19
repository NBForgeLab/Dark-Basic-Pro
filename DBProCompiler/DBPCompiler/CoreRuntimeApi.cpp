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

#define RESOLVE_REQUIRED(member, type, name) \
    api.member = Resolve<type>(lookup, name); \
    if (!api.member) return Missing(name)

    // The SDK is native x64: every required core export exists in exactly one
    // canonical x64-mangled form. There is no legacy/32-bit fallback chain -
    // anything else is a stale runtime and is rejected with a clear error.
    RESOLVE_REQUIRED(passCommandLine, CorePassPointer,
                     "?PassCmdLineHandlerPtr@@YAXPEAX@Z");
    RESOLVE_REQUIRED(passError, CorePassPointer,
                     "?PassErrorHandlerPtr@@YAXPEAX@Z");
    RESOLVE_REQUIRED(passEscape, CorePassPointer,
                     "?PassEscapePtr@@YAXPEAX@Z");
    RESOLVE_REQUIRED(passBreakout, CorePassPointer,
                     "?PassBreakOutPtr@@YAXPEAX@Z");
    RESOLVE_REQUIRED(passDataStatements, CorePassDataStatements,
                     "?PassDataStatementPtr@@YAXPEAD0@Z");

    api.passStructurePatterns = Resolve<CorePassStructurePatterns>(
        lookup, "?PassStructurePatterns@@YAXPEAXK@Z");
    if (requirements.count(RuntimeCapability::CoreStructurePatternsV1) != 0 &&
        !api.passStructurePatterns) {
        return Missing("?PassStructurePatterns@@YAXPEAXK@Z");
    }

    RESOLVE_REQUIRED(initializeDisplay, CoreInitializeDisplay,
                     "?InitDisplay@@YAKKKKKPEAUHINSTANCE__@@PEAD@Z");
    RESOLVE_REQUIRED(deleteVariableItem, CoreSlotFree,
                     "?DeleteSingleVariableAllocation@@YAXPEA_K@Z");
    RESOLVE_REQUIRED(passDlls, CoreVoid, "?PassDLLs@@YAXXZ");
    RESOLVE_REQUIRED(constructDlls, CoreVoid, "?ConstructDLLs@@YAXXZ");
    RESOLVE_REQUIRED(getGlob, CoreGetGlob, "?GetGlobPtr@@YAPEAUGlobStruct@@XZ");
    RESOLVE_REQUIRED(closeDisplay, CoreDword, "?CloseDisplay@@YAKXZ");
    RESOLVE_REQUIRED(createVariableSpace, CoreCreateSpace,
                     "?CreateVariableSpace@@YA_KK@Z");
    RESOLVE_REQUIRED(deleteVariableSpace, CoreVoid,
                     "?DeleteVariableSpace@@YAXXZ");
    RESOLVE_REQUIRED(createDataSpace, CoreCreateSpace,
                     "?CreateDataSpace@@YA_KK@Z");
    RESOLVE_REQUIRED(deleteDataSpace, CoreVoid, "?DeleteDataSpace@@YAXXZ");
    RESOLVE_REQUIRED(unDim, CoreArrayFree, "?UnDimDD@@YA_K_K@Z");
    RESOLVE_REQUIRED(sync, CoreVoid, "?Sync@@YAXXZ");
#undef RESOLVE_REQUIRED
    return CoreApiResult::Success(std::move(api));
}
