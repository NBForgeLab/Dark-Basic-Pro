#include "PreparedExecutablePackager.h"
#include "StringUtils.h"

#include "DBPCompiler.h"
#include "DebuggerInterface.h"
#include "Error.h"
#include "EXEBlock.h"
#include "TextConvert.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>

extern CDBPCompiler* g_pDBPCompiler;
extern CEXEBlock* g_pEXE;
extern CError* g_pErrorReport;

namespace {

[[nodiscard]] std::string ToUtf8(const std::filesystem::path& path) {
    return TextConvert::UTF16ToUTF8(path.wstring());
}

[[nodiscard]] std::unique_ptr<char[]> ProjectField(
    const char* const fieldName) {
    return std::unique_ptr<char[]>(g_pDBPCompiler->GetProjectField(
        const_cast<char*>(fieldName)));
}

[[nodiscard]] bool IsSafeRelativePath(
    const std::filesystem::path& path) noexcept {
    if (path.empty() || path.is_absolute() || path.has_root_path()) {
        return false;
    }
    for (const auto& component : path.lexically_normal()) {
        if (component == L"..") {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool IsWithinRoot(
    const std::filesystem::path& path,
    const std::filesystem::path& root) noexcept {
    const auto relative = path.lexically_normal().lexically_relative(
        root.lexically_normal());
    return IsSafeRelativePath(relative) || relative == L".";
}

[[nodiscard]] std::filesystem::path ResolveMediaPath(
    const char* const rawPath,
    const std::filesystem::path& mediaRoot) {
    if (rawPath == nullptr || rawPath[0] == '\0') {
        return {};
    }

    const std::filesystem::path path{
        TextConvert::UTF8ToUTF16(rawPath)};
    if (path.is_absolute()) {
        const auto normalized = path.lexically_normal();
        return IsWithinRoot(normalized, mediaRoot) ? normalized
                                                   : std::filesystem::path{};
    }
    if (!IsSafeRelativePath(path)) {
        return {};
    }
    return (mediaRoot / path).lexically_normal();
}

[[nodiscard]] bool AddFile(
    CFileBuilder& builder,
    const std::filesystem::path& source,
    const std::string& placement) {
    auto sourceText = ToUtf8(source);
    auto placementText = placement;
    return builder.AddFile(sourceText.data(), placementText.data());
}

[[nodiscard]] bool AddMetadata(
    CFileBuilder& builder,
    const std::string& value) {
    auto mutableValue = value;
    char empty[] = "";
    return builder.AddFile(mutableValue.data(), empty);
}

[[nodiscard]] std::string Truncate(
    const char* const value,
    const std::size_t maximumLength) {
    if (value == nullptr) {
        return {};
    }
    return std::string{
        value, (std::min)(std::strlen(value), maximumLength)};
}

void Report(const char* const message) noexcept {
    if (g_pErrorReport != nullptr) {
        g_pErrorReport->AddErrorString(message);
    }
}

void Report(std::string message) noexcept {
    if (g_pErrorReport != nullptr) {
        g_pErrorReport->AddErrorString(message.data());
    }
}

} // namespace

bool PreparedExecutablePackager::Package(
    const StandalonePackagingRequest& request,
    IStandalonePackagingServices& services) const noexcept {
    if (request.outputPath.empty()) {
        Report("DBP3100: The standalone executable output path is empty.");
        return false;
    }

    if (!services.ResolveRuntimeBundle()) {
        Report("DBP3101: Resolving the runtime bundle for packaging failed.");
        return false;
    }
    if (!services.StageExecutable(request.outputPath)) {
        Report("DBP3102: Staging the standalone executable failed.");
        return false;
    }
    if (!services.CustomizeResources()) {
        Report("DBP3103: Customizing executable resources failed.");
        return false;
    }
    if (!services.Publish()) {
        Report("DBP3104: Publishing the standalone executable failed.");
        return false;
    }
    return true;
}

bool ASMWriterStandalonePackagingServices::ResolveRuntimeBundle() noexcept {
    if (g_pDBPCompiler == nullptr || g_pEXE == nullptr ||
        g_pErrorReport == nullptr) {
        return false;
    }

    runtimeBundle_ = g_pDBPCompiler->GetResolvedRuntimeBundle();
    if (runtimeBundle_ == nullptr) {
        Report("DBP3002: Runtime bundle was not resolved before packaging.");
        return false;
    }

    std::error_code error;
    if (!std::filesystem::is_regular_file(
            runtimeBundle_->corePath, error) || error) {
        Report("DBP3002: The validated runtime DBProCore.dll is unavailable.");
        return false;
    }
    return true;
}

bool ASMWriterStandalonePackagingServices::StageExecutable(
    const std::filesystem::path& outputPath) noexcept {
    if (runtimeBundle_ == nullptr || outputPath.empty()) {
        Report("DBP3110: Executable staging was called without a runtime bundle or output path.");
        return false;
    }

    try {
        std::error_code error;
        outputPath_ = std::filesystem::absolute(outputPath, error)
                          .lexically_normal();
        if (error || outputPath_.filename().empty()) {
            Report(
                std::string{"DBP3111: The executable output path could not be normalized: "} +
                ToUtf8(outputPath) + " (" + error.message() + ")");
            return false;
        }

        std::unique_ptr<char[]> projectMediaRoot(
            g_pDBPCompiler->GetProjectMediaRoot());
        if (projectMediaRoot != nullptr && projectMediaRoot[0] != '\0') {
            mediaRoot_ = std::filesystem::absolute(
                std::filesystem::path{
                    TextConvert::UTF8ToUTF16(projectMediaRoot.get())},
                error).lexically_normal();
        } else {
            mediaRoot_ = std::filesystem::current_path(error);
        }
        if (error || mediaRoot_.empty()) {
            Report("DBP3112: The project media root could not be resolved.");
            return false;
        }

        g_pErrorReport->ProgressReport(
            "Linker now at line ", g_pErrorReport->GetPerc(0));

        LPSTR temporaryExeBlock =
            g_pDBPCompiler->GetInternalFile(PATH_TEMPEXBFILE);
        if (temporaryExeBlock == nullptr || temporaryExeBlock[0] == '\0' ||
            !g_pEXE->Save(temporaryExeBlock)) {
            Report("DBP3113: The prepared executable block could not be serialized.");
            return false;
        }
        builder_.SetPackageKeyFile(g_pDBPCompiler->GetPackageKeyFile());
        if (!builder_.NewFileTable()) {
            Report("DBP3114: The executable package file table could not be initialized.");
            return false;
        }
        if (!AddFile(
                builder_,
                std::filesystem::path{
                    TextConvert::UTF8ToUTF16(temporaryExeBlock)},
                "_virtual.dat")) {
            Report("DBP3115: The prepared executable block could not be staged.");
            return false;
        }
        if (!AddRuntimeLibraries()) {
            Report("DBP3116: Runtime libraries could not be staged.");
            return false;
        }
        if (!AddProjectMedia()) {
            Report("DBP3117: Project media could not be staged.");
            return false;
        }
        if (!AddApplicationAssets()) {
            Report("DBP3118: Application icons or cursors could not be staged.");
            return false;
        }
        if (!AddEffects()) {
            Report("DBP3119: Runtime effect files could not be staged.");
            return false;
        }

        g_pErrorReport->ProgressReport(
            "Linker now at line ", g_pErrorReport->GetPerc(10));
        auto outputText = ToUtf8(outputPath_);
        const bool staged = builder_.MakeEXE(
            outputText.data(),
            g_pDBPCompiler->GetEncryptionState(),
            nullptr) && builder_.HasStagedExecutable();
        if (!staged) {
            Report("DBP3120: The executable host image could not be staged.");
        }
        return staged;
    } catch (...) {
        Report("DBP3121: An exception interrupted executable staging.");
        return false;
    }
}

bool ASMWriterStandalonePackagingServices::AddRuntimeLibraries() noexcept {
    if (CDebuggerInterface::ShouldExternaliseDLLs()) {
        return true;
    }
    if (g_pEXE->m_dwNumberOfDLLs > 0U &&
        g_pEXE->m_pDLLFilenameArray == nullptr) {
        return false;
    }

    try {
        for (DWORD index = 0U; index < g_pEXE->m_dwNumberOfDLLs; ++index) {
            const auto* name = reinterpret_cast<const char*>(
                g_pEXE->m_pDLLFilenameArray[index]);
            if (name == nullptr || name[0] == '\0') {
                return false;
            }

            const std::filesystem::path namePath{
                TextConvert::UTF8ToUTF16(name)};
            if (namePath.filename() != namePath) {
                return false;
            }

            const bool isCore = dbp::iequals(name, "dbprocore.dll");
            std::filesystem::path source = isCore
                ? runtimeBundle_->corePath
                : runtimeBundle_->pluginsDirectory / namePath;
            std::error_code error;
            if (!std::filesystem::is_regular_file(source, error) || error) {
                if (isCore) {
                    Report("DBP3002: The validated runtime DBProCore.dll is no longer available.");
                    return false;
                }

                source = runtimeBundle_->userPluginsDirectory / namePath;
                error.clear();
                if (!std::filesystem::is_regular_file(source, error) || error) {
                    source = runtimeBundle_->licensedPluginsDirectory / namePath;
                    error.clear();
                }
            }

            if (!std::filesystem::is_regular_file(source, error) || error) {
                Report(
                    std::string{
                        "DBP3002: Referenced runtime library is unavailable: "} +
                    name);
                return false;
            }
            if (!AddFile(builder_, source, name)) {
                return false;
            }
            if (dbp::iequals(name, "dbprobasic3ddebug.dll")) {
                effectsRequired_ = true;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool ASMWriterStandalonePackagingServices::AddProjectMedia() noexcept {
    if (!g_pDBPCompiler->GetInternalMediaState()) {
        return true;
    }

    try {
        for (DWORD index = 1U; index < 65535U; ++index) {
            const std::string fieldName = "media" + std::to_string(index);
            auto mediaName = ProjectField(fieldName.c_str());
            if (mediaName == nullptr || mediaName[0] == '\0') {
                break;
            }

            const std::filesystem::path relative{
                TextConvert::UTF8ToUTF16(mediaName.get())};
            const bool hasWildcard =
                std::strpbrk(mediaName.get(), "*?") != nullptr;
            if (hasWildcard) {
                if (!IsSafeRelativePath(relative)) {
                    return false;
                }
                auto rootText = ToUtf8(mediaRoot_);
                if (!rootText.empty() && rootText.back() != '\\' &&
                    rootText.back() != '/') {
                    rootText.push_back('\\');
                }
                auto wildcardText = ToUtf8(relative);
                if (!builder_.AddWildcardFiles(
                        rootText.data(), wildcardText.data())) {
                    return false;
                }
            } else {
                const auto source = ResolveMediaPath(
                    mediaName.get(), mediaRoot_);
                if (source.empty()) {
                    return false;
                }
                const auto placementPath =
                    std::filesystem::path{L"media"} /
                    source.lexically_relative(mediaRoot_);
                if (!AddFile(
                        builder_, source, ToUtf8(placementPath))) {
                    return false;
                }
                if (dbp::iequals(mediaName.get(), "icon.ico")) {
                    replacementIcon_ = source;
                }
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool ASMWriterStandalonePackagingServices::AddApplicationAssets() noexcept {
    try {
        if (replacementIcon_.empty()) {
            auto configuredIcon = ProjectField("icon1");
            if (configuredIcon != nullptr && configuredIcon[0] != '\0') {
                replacementIcon_ = ResolveMediaPath(
                    configuredIcon.get(), mediaRoot_);
                if (replacementIcon_.empty()) {
                    return false;
                }
            }
        }
        if (replacementIcon_.empty()) {
            replacementIcon_ =
                runtimeBundle_->pluginsDirectory / L"icon.ico";
        }

        auto packagedIcon = replacementIcon_;
        if (_wcsicmp(packagedIcon.extension().c_str(), L".bmp") == 0) {
            auto sourceText = ToUtf8(packagedIcon);
            auto destinationText =
                ToUtf8(runtimeBundle_->pluginsDirectory) + "\\";
            if (!builder_.MakeICOFromBMP(
                    sourceText.data(), destinationText.data())) {
                return false;
            }
            packagedIcon =
                runtimeBundle_->pluginsDirectory / L"workicon.ico";
        }
        std::error_code iconError;
        if (!std::filesystem::is_regular_file(packagedIcon, iconError) ||
            iconError) {
            Report("DBP3108: The configured application icon is unavailable.");
            return false;
        }
        if (!AddFile(builder_, packagedIcon, "icon.ico")) {
            return false;
        }

        for (DWORD index = 0U; index < 32U; ++index) {
            const std::string fieldName = index == 0U
                ? "cursorarrow"
                : index == 1U ? "cursorwait"
                              : "pointer" + std::to_string(index);
            const std::string placement = index == 0U
                ? "arrow.cur"
                : index == 1U ? "hourglass.cur"
                              : "pointer" + std::to_string(index) + ".cur";
            auto cursorName = ProjectField(fieldName.c_str());
            if (cursorName == nullptr || cursorName[0] == '\0') {
                continue;
            }
            auto cursorPath = ResolveMediaPath(
                cursorName.get(), mediaRoot_);
            if (cursorPath.empty()) {
                return false;
            }
            if (_wcsicmp(cursorPath.extension().c_str(), L".bmp") == 0) {
                const auto converted = runtimeBundle_->pluginsDirectory /
                    (L"workcursor" + std::to_wstring(index) + L".cur");
                auto sourceText = ToUtf8(cursorPath);
                auto convertedText = ToUtf8(converted);
                if (!builder_.MakeCURFromBMP(
                        sourceText.data(), convertedText.data())) {
                    return false;
                }
                cursorPath = converted;
            }
            if (!AddFile(builder_, cursorPath, placement)) {
                return false;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool ASMWriterStandalonePackagingServices::AddEffects() noexcept {
    if (!effectsRequired_) {
        return true;
    }

    static constexpr std::array<const wchar_t*, 6U> effectNames{
        L"bump.fx",
        L"cartoon.fx",
        L"rainbow.fx",
        L"stencilshadow.fx",
        L"stencilshadowbone.fx",
        L"quad.fx",
    };
    try {
        for (const auto* const name : effectNames) {
            if (!AddFile(
                    builder_,
                    runtimeBundle_->effectsDirectory / name,
                    ToUtf8(std::filesystem::path{name}))) {
                return false;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool ASMWriterStandalonePackagingServices::CustomizeResources() noexcept {
    if (!builder_.HasStagedExecutable() || outputPath_.empty() ||
        runtimeBundle_ == nullptr) {
        return false;
    }

    try {
        g_pErrorReport->ProgressReport(
            "Linker now at line ", g_pErrorReport->GetPerc(50));
        if (!AddResourceMetadata()) {
            return false;
        }
        g_pErrorReport->ProgressReport(
            "Linker now at line ", g_pErrorReport->GetPerc(60));
        auto outputText = ToUtf8(outputPath_);
        auto pluginRoot = ToUtf8(runtimeBundle_->pluginsDirectory) + "\\";
        return builder_.ChangeEXE(
            outputText.data(), pluginRoot.data());
    } catch (...) {
        return false;
    }
}

bool ASMWriterStandalonePackagingServices::AddResourceMetadata() noexcept {
    if (!builder_.NewFileTable()) {
        return false;
    }

    try {
        auto titleField = ProjectField("app title");
        const std::string appName = titleField != nullptr &&
                titleField[0] != '\0'
            ? Truncate(titleField.get(), 24U)
            : "DBPro Application";
        const std::string executableName =
            Truncate(ToUtf8(outputPath_.filename()).c_str(), 24U);

        static constexpr std::array<const char*, 10U> fieldNames{
            "VerComments",
            "VerCompany",
            "VerFileDesc",
            "VerFileNumber",
            "VerInternal",
            "VerCopyright",
            "VerTrademark",
            "VerFilename",
            "VerProduct",
            "VerProductNumber",
        };
        const std::array<std::string, 10U> defaults{
            appName,
            appName + " Ltd",
            appName,
            "V1.0",
            appName,
            "(C) " + appName + " Ltd",
            "",
            executableName,
            appName,
            "V1.0",
        };

        for (std::size_t index = 0U; index < fieldNames.size(); ++index) {
            auto configured = ProjectField(fieldNames[index]);
            const auto maximum =
                index == 3U || index == 9U ? 10U : 32U;
            const auto value = configured != nullptr
                ? Truncate(configured.get(), maximum)
                : Truncate(defaults[index].c_str(), maximum);
            if (!AddMetadata(builder_, value)) {
                return false;
            }
        }

        auto largeIcon = ProjectField("icon1");
        auto smallIcon = ProjectField("icon2");
        auto largePath = largeIcon != nullptr && largeIcon[0] != '\0'
            ? ResolveMediaPath(largeIcon.get(), mediaRoot_)
            : replacementIcon_;
        auto smallPath = smallIcon != nullptr && smallIcon[0] != '\0'
            ? ResolveMediaPath(smallIcon.get(), mediaRoot_)
            : largePath;
        if (largePath.empty()) {
            largePath = runtimeBundle_->pluginsDirectory / L"icon.ico";
        }
        if (smallPath.empty()) {
            smallPath = largePath;
        }
        std::error_code iconError;
        if (!std::filesystem::is_regular_file(largePath, iconError) ||
            iconError) {
            Report("DBP3108: The large executable icon is unavailable.");
            return false;
        }
        iconError.clear();
        if (!std::filesystem::is_regular_file(smallPath, iconError) ||
            iconError) {
            Report("DBP3108: The small executable icon is unavailable.");
            return false;
        }
        return AddFile(builder_, largePath, "") &&
               AddFile(builder_, smallPath, "");
    } catch (...) {
        return false;
    }
}

bool ASMWriterStandalonePackagingServices::Publish() noexcept {
    if (!builder_.HasStagedExecutable() || outputPath_.empty()) {
        return false;
    }

    try {
        const DWORD executableKind =
            g_pDBPCompiler->GetEXEInstallerState() ? 1U : 0U;
        g_pErrorReport->ProgressReport(
            "Linker now at line ", g_pErrorReport->GetPerc(100));
        auto outputText = ToUtf8(outputPath_);
        return builder_.FinalizePackage(
            outputText.data(), executableKind);
    } catch (...) {
        return false;
    }
}
