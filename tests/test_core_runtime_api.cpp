#include <gtest/gtest.h>

#include "CoreRuntimeApi.h"

#include <map>
#include <string>

namespace {

void StubOnePointer(void*) {}
void StubTwoPointers(char*, char*) {}
void StubStructurePatterns(void*, unsigned long) {}

std::map<std::string, void*> BaselineSymbols() {
    return {
        {"?PassCmdLineHandlerPtr@@YAXPAX@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?PassCmdLineHandlerPtr@@YAXPEAX@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?PassErrorHandlerPtr@@YAXPAX@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?PassErrorHandlerPtr@@YAXPEAX@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?PassEscapePtr@@YAXPAX@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?PassEscapePtr@@YAXPEAX@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?PassBreakOutPtr@@YAXPAX@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?PassBreakOutPtr@@YAXPEAX@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?PassDataStatementPtr@@YAXPAD0@Z", reinterpret_cast<void*>(&StubTwoPointers)},
        {"?PassDataStatementPtr@@YAXPEAD0@Z", reinterpret_cast<void*>(&StubTwoPointers)},
        {"?PassDLLs@@YAXXZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?ConstructDLLs@@YAXXZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?GetGlobPtr@@YAKXZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?GetGlobPtr@@YAPEAUGlobStruct@@XZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?GetGlobPtr@@YAPEAXXZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?InitDisplay@@YAKKKKKPAUHINSTANCE__@@PAD@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?InitDisplay@@YAKKKKKPEAUHINSTANCE__@@PEAD@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?CloseDisplay@@YAKXZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?CreateVariableSpace@@YAKK@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?DeleteVariableSpace@@YAXXZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?CreateDataSpace@@YAKK@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?DeleteDataSpace@@YAXXZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?DeleteSingleVariableAllocation@@YAXPAK@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?DeleteSingleVariableAllocation@@YAXPEAK@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?UnDimDD@@YAKK@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?Sync@@YAXXZ", reinterpret_cast<void*>(&StubOnePointer)}};
}

CoreSymbolLookup Lookup(std::map<std::string, void*> symbols) {
    return [symbols = std::move(symbols)](const char* name) -> void* {
        const auto found = symbols.find(name);
        return found == symbols.end() ? nullptr : found->second;
    };
}

} // namespace

TEST(CoreRuntimeApiTest, RejectsMissingRequiredBootstrapFunction) {
    auto symbols = BaselineSymbols();
#if defined(_WIN64)
    symbols.erase("?PassErrorHandlerPtr@@YAXPEAX@Z");
    symbols.erase("?PassErrorHandlerPtr@@YAXPAX@Z");
#else
    symbols.erase("?PassErrorHandlerPtr@@YAXPAX@Z");
#endif

    const auto result = ResolveCoreRuntimeApi(
        Lookup(std::move(symbols)), DeriveProgramRuntimeRequirements(0));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, CoreApiErrorCode::MissingRequiredExport);
#if defined(_WIN64)
    EXPECT_EQ(result.error().exportName, "?PassErrorHandlerPtr@@YAXPEAX@Z");
#else
    EXPECT_EQ(result.error().exportName, "?PassErrorHandlerPtr@@YAXPAX@Z");
#endif
}

TEST(CoreRuntimeApiTest, OmitsUnusedLegacyStructureFunctionSafely) {
    const auto result = ResolveCoreRuntimeApi(
        Lookup(BaselineSymbols()), DeriveProgramRuntimeRequirements(0));

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result.value().passStructurePatterns, nullptr);
}

TEST(CoreRuntimeApiTest, RejectsRequiredLegacyStructureFunction) {
    const auto result = ResolveCoreRuntimeApi(
        Lookup(BaselineSymbols()), DeriveProgramRuntimeRequirements(3));

    ASSERT_FALSE(result.has_value());
#if defined(_WIN64)
    EXPECT_EQ(result.error().exportName, "?PassStructurePatterns@@YAXPEAXK@Z");
#else
    EXPECT_EQ(result.error().exportName, "?PassStructurePatterns@@YAXPAXK@Z");
#endif
}

TEST(CoreRuntimeApiTest, ResolvesRequiredModernStructureFunction) {
    auto symbols = BaselineSymbols();
#if defined(_WIN64)
    symbols["?PassStructurePatterns@@YAXPEAXK@Z"] =
        reinterpret_cast<void*>(&StubStructurePatterns);
#else
    symbols["?PassStructurePatterns@@YAXPAXK@Z"] =
        reinterpret_cast<void*>(&StubStructurePatterns);
#endif

    const auto result = ResolveCoreRuntimeApi(
        Lookup(std::move(symbols)), DeriveProgramRuntimeRequirements(2));

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_NE(result.value().passStructurePatterns, nullptr);
}

TEST(CoreRuntimeApiTest, RejectsMissingRequiredLifecycleFunction) {
    auto symbols = BaselineSymbols();
    symbols.erase("?PassDLLs@@YAXXZ");

    const auto result = ResolveCoreRuntimeApi(
        Lookup(std::move(symbols)), DeriveProgramRuntimeRequirements(0));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().exportName, "?PassDLLs@@YAXXZ");
}
