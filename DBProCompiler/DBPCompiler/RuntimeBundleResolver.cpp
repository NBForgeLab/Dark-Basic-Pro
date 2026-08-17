#include "RuntimeBundleResolver.h"

#include "PeExportInspector.h"

#include <algorithm>
#include <array>
#include <system_error>

namespace {

bool HasExport(const PeImageInfo& image, const char* name1, const char* name2 = nullptr) {
    if (image.exports.count(name1) != 0) return true;
    if (name2 && image.exports.count(name2) != 0) return true;
    return false;
}

bool HasExportAny(const PeImageInfo& image, std::initializer_list<const char*> names) {
    for (const auto* name : names) {
        if (image.exports.count(name) != 0) return true;
    }
    return false;
}

RuntimeCapabilities DeriveCapabilities(const PeImageInfo& image) {
    RuntimeCapabilities capabilities;

    const bool hasPassCommandLine = HasExport(image, "?PassCmdLineHandlerPtr@@YAXPEAX@Z", "?PassCmdLineHandlerPtr@@YAXPAX@Z");
    const bool hasPassError = HasExport(image, "?PassErrorHandlerPtr@@YAXPEAX@Z", "?PassErrorHandlerPtr@@YAXPAX@Z");
    const bool hasPassEscape = HasExport(image, "?PassEscapePtr@@YAXPEAX@Z", "?PassEscapePtr@@YAXPAX@Z");
    const bool hasPassBreakout = HasExport(image, "?PassBreakOutPtr@@YAXPEAX@Z", "?PassBreakOutPtr@@YAXPAX@Z");
    const bool hasPassData = HasExport(image, "?PassDataStatementPtr@@YAXPEAD0@Z", "?PassDataStatementPtr@@YAXPAD0@Z");
    const bool hasPassStructurePatterns = HasExport(image, "?PassStructurePatterns@@YAXPEAXK@Z", "?PassStructurePatterns@@YAXPAXK@Z");

    const bool hasInitDisplay = HasExport(image, "?InitDisplay@@YAKKKKKPEAUHINSTANCE__@@PEAD@Z", "?InitDisplay@@YAKKKKKPAUHINSTANCE__@@PAD@Z");
    // The parameter was modernized from DWORD* to DWORD_PTR* (PEA_K on x64),
    // so accept all three spellings of DeleteSingleVariableAllocation.
    const bool hasDeleteVarItem = HasExportAny(image, {
        "?DeleteSingleVariableAllocation@@YAXPEAK@Z",
        "?DeleteSingleVariableAllocation@@YAXPAK@Z",
        "?DeleteSingleVariableAllocation@@YAXPEA_K@Z"});

    // The space factories and GetGlobPtr were historically declared with
    // 32-bit return types (DWORD) even on x64 builds of the legacy runtime.
    // The modernized SDK core exports the 64-bit-clean variants instead.
    // Accept both spellings so either runtime generation satisfies the
    // bootstrap contract; the app-side resolver (CoreRuntimeApi) mirrors
    // this fallback chain at load time.
    const std::array<const char*, 7> basicLifecycleExports{
        "?PassDLLs@@YAXXZ",
        "?ConstructDLLs@@YAXXZ",
        "?CloseDisplay@@YAKXZ",
        "?DeleteVariableSpace@@YAXXZ",
        "?DeleteDataSpace@@YAXXZ",
        "?UnDimDD@@YAKK@Z",
        "?Sync@@YAXXZ"};

    const bool hasBasicLifecycle =
        std::all_of(
            basicLifecycleExports.begin(), basicLifecycleExports.end(),
            [&image](const char* name) { return HasExport(image, name); }) &&
        HasExportAny(image, {
            "?GetGlobPtr@@YAPEAUGlobStruct@@XZ",
            "?GetGlobPtr@@YAPEAXXZ",
            "?GetGlobPtr@@YAKXZ"}) &&
        HasExportAny(image, {
            "?CreateVariableSpace@@YA_KK@Z",
            "?CreateVariableSpace@@YAKK@Z"}) &&
        HasExportAny(image, {
            "?CreateDataSpace@@YA_KK@Z",
            "?CreateDataSpace@@YAKK@Z"});

    const bool hasLifecycle = hasBasicLifecycle && hasInitDisplay && hasDeleteVarItem;

    if (hasPassCommandLine &&
        hasPassError &&
        hasPassEscape &&
        hasPassBreakout &&
        hasLifecycle) {
        capabilities.insert(RuntimeCapability::CoreBootstrapV1);
    }
    if (hasPassData) {
        capabilities.insert(RuntimeCapability::CoreDataStatementsV1);
    }
    if (hasPassStructurePatterns) {
        capabilities.insert(RuntimeCapability::CoreStructurePatternsV1);
    }
    if (hasPassError) {
        capabilities.insert(RuntimeCapability::CoreRuntimeErrorsV1);
    }
    return capabilities;
}

RuntimeResult<ResolvedRuntimeBundle> Failure(
    const RuntimeErrorCode code,
    const std::string& message,
    const std::filesystem::path& root,
    const std::filesystem::path& component = {},
    const std::optional<RuntimeCapability> capability = std::nullopt) {
    return RuntimeResult<ResolvedRuntimeBundle>::Failure(
        {code, message, root, component, capability});
}

} // namespace

RuntimeResult<ResolvedRuntimeBundle> RuntimeBundleResolver::Resolve(
    const RuntimeSelection& selection,
    const ProgramRuntimeRequirements& requirements) {
    std::filesystem::path selectedRoot;
    if (selection.explicitRoot) {
        selectedRoot = *selection.explicitRoot;
    } else {
        const auto versionedCandidate =
            selection.compilerDirectory.parent_path() / "runtime";
        std::error_code existsError;
        if (std::filesystem::is_directory(versionedCandidate, existsError)) {
            selectedRoot = versionedCandidate;
        } else {
            selectedRoot = selection.compilerDirectory;
        }
    }

    std::error_code canonicalError;
    const auto root = std::filesystem::weakly_canonical(
        selectedRoot, canonicalError);
    if (canonicalError || !std::filesystem::is_directory(root)) {
        return Failure(
            RuntimeErrorCode::MissingRoot,
            "The selected DBPro runtime root does not exist.",
            selectedRoot);
    }

    ResolvedRuntimeBundle bundle;
    bundle.root = root;
    bundle.pluginsDirectory = root / "plugins";
    bundle.userPluginsDirectory = root / "plugins-user";
    bundle.licensedPluginsDirectory = root / "plugins-licensed";
    bundle.effectsDirectory = root / "effects";
    bundle.corePath = bundle.pluginsDirectory / "DBProCore.dll";
    bundle.classification = std::filesystem::exists(
        root / "runtime-manifest.json")
        ? RuntimeBundleClassification::Versioned
        : RuntimeBundleClassification::LegacyUnversioned;

    if (!std::filesystem::is_regular_file(bundle.corePath)) {
        return Failure(
            RuntimeErrorCode::MissingComponent,
            "The selected runtime does not contain plugins/DBProCore.dll.",
            root,
            bundle.corePath);
    }

    const auto inspection = PeExportInspector::Inspect(bundle.corePath);
    if (!inspection) {
        return Failure(
            RuntimeErrorCode::InvalidComponent,
            inspection.error().message,
            root,
            bundle.corePath);
    }

#if defined(_WIN64)
    if (inspection.value().machine != PeMachine::X64 && inspection.value().machine != PeMachine::X86) {
        return Failure(
            RuntimeErrorCode::IncompatibleArchitecture,
            "DBProCore.dll must be a valid PE image.",
            root,
            bundle.corePath);
    }
#else
    if (inspection.value().machine != PeMachine::X86) {
        return Failure(
            RuntimeErrorCode::IncompatibleArchitecture,
            "DBProCore.dll must be a Win32 x86 image.",
            root,
            bundle.corePath);
    }
#endif

    bundle.capabilities = DeriveCapabilities(inspection.value());
    for (const auto required : requirements) {
        if (bundle.capabilities.count(required) == 0) {
            return Failure(
                RuntimeErrorCode::MissingCapability,
                "The selected runtime lacks required capability " +
                    std::to_string(static_cast<int>(required)) + ".",
                root,
                bundle.corePath,
                required);
        }
    }

    return RuntimeResult<ResolvedRuntimeBundle>::Success(std::move(bundle));
}
