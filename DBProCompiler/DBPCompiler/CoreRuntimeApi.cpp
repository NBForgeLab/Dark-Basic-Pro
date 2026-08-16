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

template <typename T>
T ResolveWithFallback(const CoreSymbolLookup& lookup, const char* name1, const char* name2) {
    T fn = reinterpret_cast<T>(lookup(name1));
    if (!fn && name2) {
        fn = reinterpret_cast<T>(lookup(name2));
    }
    return fn;
}

} // namespace

CoreApiResult ResolveCoreRuntimeApi(
    const CoreSymbolLookup& lookup,
    const ProgramRuntimeRequirements& requirements) {
    CoreRuntimeApi api;

#if defined(_WIN64)
    api.passCommandLine = ResolveWithFallback<CorePassPointer>(
        lookup, "?PassCmdLineHandlerPtr@@YAXPEAX@Z", "?PassCmdLineHandlerPtr@@YAXPAX@Z");
    if (!api.passCommandLine) return Missing("?PassCmdLineHandlerPtr@@YAXPEAX@Z");

    api.passError = ResolveWithFallback<CorePassPointer>(
        lookup, "?PassErrorHandlerPtr@@YAXPEAX@Z", "?PassErrorHandlerPtr@@YAXPAX@Z");
    if (!api.passError) return Missing("?PassErrorHandlerPtr@@YAXPEAX@Z");

    api.passEscape = ResolveWithFallback<CorePassPointer>(
        lookup, "?PassEscapePtr@@YAXPEAX@Z", "?PassEscapePtr@@YAXPAX@Z");
    if (!api.passEscape) return Missing("?PassEscapePtr@@YAXPEAX@Z");

    api.passBreakout = ResolveWithFallback<CorePassPointer>(
        lookup, "?PassBreakOutPtr@@YAXPEAX@Z", "?PassBreakOutPtr@@YAXPAX@Z");
    if (!api.passBreakout) return Missing("?PassBreakOutPtr@@YAXPEAX@Z");

    api.passDataStatements = ResolveWithFallback<CorePassDataStatements>(
        lookup, "?PassDataStatementPtr@@YAXPEAD0@Z", "?PassDataStatementPtr@@YAXPAD0@Z");
    if (!api.passDataStatements) return Missing("?PassDataStatementPtr@@YAXPEAD0@Z");

    api.passStructurePatterns = ResolveWithFallback<CorePassStructurePatterns>(
        lookup, "?PassStructurePatterns@@YAXPEAXK@Z", "?PassStructurePatterns@@YAXPAXK@Z");
    if (requirements.count(RuntimeCapability::CoreStructurePatternsV1) != 0 &&
        !api.passStructurePatterns) {
        return Missing("?PassStructurePatterns@@YAXPEAXK@Z");
    }

    api.initializeDisplay = ResolveWithFallback<CoreInitializeDisplay>(
        lookup, "?InitDisplay@@YAKKKKKPEAUHINSTANCE__@@PEAD@Z", "?InitDisplay@@YAKKKKKPAUHINSTANCE__@@PAD@Z");
    if (!api.initializeDisplay) return Missing("?InitDisplay@@YAKKKKKPEAUHINSTANCE__@@PEAD@Z");

    api.deleteVariableItem = ResolveWithFallback<CoreVoidDwordPointer>(
        lookup, "?DeleteSingleVariableAllocation@@YAXPEAK@Z", "?DeleteSingleVariableAllocation@@YAXPAK@Z");
    if (!api.deleteVariableItem) return Missing("?DeleteSingleVariableAllocation@@YAXPEAK@Z");
#else
    api.passCommandLine = ResolveWithFallback<CorePassPointer>(
        lookup, "?PassCmdLineHandlerPtr@@YAXPAX@Z", "?PassCmdLineHandlerPtr@@YAXPEAX@Z");
    if (!api.passCommandLine) return Missing("?PassCmdLineHandlerPtr@@YAXPAX@Z");

    api.passError = ResolveWithFallback<CorePassPointer>(
        lookup, "?PassErrorHandlerPtr@@YAXPAX@Z", "?PassErrorHandlerPtr@@YAXPEAX@Z");
    if (!api.passError) return Missing("?PassErrorHandlerPtr@@YAXPAX@Z");

    api.passEscape = ResolveWithFallback<CorePassPointer>(
        lookup, "?PassEscapePtr@@YAXPAX@Z", "?PassEscapePtr@@YAXPEAX@Z");
    if (!api.passEscape) return Missing("?PassEscapePtr@@YAXPAX@Z");

    api.passBreakout = ResolveWithFallback<CorePassPointer>(
        lookup, "?PassBreakOutPtr@@YAXPAX@Z", "?PassBreakOutPtr@@YAXPEAX@Z");
    if (!api.passBreakout) return Missing("?PassBreakOutPtr@@YAXPAX@Z");

    api.passDataStatements = ResolveWithFallback<CorePassDataStatements>(
        lookup, "?PassDataStatementPtr@@YAXPAD0@Z", "?PassDataStatementPtr@@YAXPEAD0@Z");
    if (!api.passDataStatements) return Missing("?PassDataStatementPtr@@YAXPAD0@Z");

    api.passStructurePatterns = ResolveWithFallback<CorePassStructurePatterns>(
        lookup, "?PassStructurePatterns@@YAXPAXK@Z", "?PassStructurePatterns@@YAXPEAXK@Z");
    if (requirements.count(RuntimeCapability::CoreStructurePatternsV1) != 0 &&
        !api.passStructurePatterns) {
        return Missing("?PassStructurePatterns@@YAXPAXK@Z");
    }

    api.initializeDisplay = ResolveWithFallback<CoreInitializeDisplay>(
        lookup, "?InitDisplay@@YAKKKKKPAUHINSTANCE__@@PAD@Z", "?InitDisplay@@YAKKKKKPEAUHINSTANCE__@@PEAD@Z");
    if (!api.initializeDisplay) return Missing("?InitDisplay@@YAKKKKKPAUHINSTANCE__@@PAD@Z");

    api.deleteVariableItem = ResolveWithFallback<CoreVoidDwordPointer>(
        lookup, "?DeleteSingleVariableAllocation@@YAXPAK@Z", "?DeleteSingleVariableAllocation@@YAXPEAK@Z");
    if (!api.deleteVariableItem) return Missing("?DeleteSingleVariableAllocation@@YAXPAK@Z");
#endif

#define RESOLVE_REQUIRED(member, type, name) \
    api.member = Resolve<type>(lookup, name); \
    if (!api.member) return Missing(name)
    RESOLVE_REQUIRED(passDlls, CoreVoid, "?PassDLLs@@YAXXZ");
    RESOLVE_REQUIRED(constructDlls, CoreVoid, "?ConstructDLLs@@YAXXZ");
#if defined(_WIN64)
    api.getGlob = ResolveWithFallback<CoreGetGlob>(
        lookup, "?GetGlobPtr@@YAPEAUGlobStruct@@XZ", "?GetGlobPtr@@YAPEAXXZ");
    if (!api.getGlob) {
        api.getGlob = Resolve<CoreGetGlob>(lookup, "?GetGlobPtr@@YAKXZ");
    }
    if (!api.getGlob) return Missing("?GetGlobPtr@@YAPEAUGlobStruct@@XZ");
#else
    api.getGlob = ResolveWithFallback<CoreGetGlob>(
        lookup, "?GetGlobPtr@@YAPEAUGlobStruct@@XZ", "?GetGlobPtr@@YAKXZ");
    if (!api.getGlob) return Missing("?GetGlobPtr@@YAKXZ");
#endif
    RESOLVE_REQUIRED(closeDisplay, CoreDword, "?CloseDisplay@@YAKXZ");
    api.createVariableSpace = ResolveWithFallback<CoreCreateSpace>(
        lookup, "?CreateVariableSpace@@YA_KK@Z", "?CreateVariableSpace@@YAKK@Z");
    if (!api.createVariableSpace) return Missing("?CreateVariableSpace@@YA_KK@Z");
    RESOLVE_REQUIRED(deleteVariableSpace, CoreVoid,
                     "?DeleteVariableSpace@@YAXXZ");
    api.createDataSpace = ResolveWithFallback<CoreCreateSpace>(
        lookup, "?CreateDataSpace@@YA_KK@Z", "?CreateDataSpace@@YAKK@Z");
    if (!api.createDataSpace) return Missing("?CreateDataSpace@@YA_KK@Z");
    RESOLVE_REQUIRED(deleteDataSpace, CoreVoid, "?DeleteDataSpace@@YAXXZ");
    RESOLVE_REQUIRED(unDim, CoreVoidDwordPointer, "?UnDimDD@@YAKK@Z");
    RESOLVE_REQUIRED(sync, CoreVoid, "?Sync@@YAXXZ");
#undef RESOLVE_REQUIRED
    return CoreApiResult::Success(std::move(api));
}
