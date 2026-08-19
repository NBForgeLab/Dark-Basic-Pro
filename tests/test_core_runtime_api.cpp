#include <gtest/gtest.h>

#include "CoreRuntimeApi.h"

#include <map>
#include <string>

namespace {

void StubOnePointer(void*) {}
void StubTwoPointers(char*, char*) {}
void StubStructurePatterns(void*, unsigned long) {}

// The SDK runtime is native x64: DBProCore.dll exports each required entry
// point in exactly one canonical x64-mangled form. The mock table mirrors
// those canonical names - there are no legacy 32-bit-era spellings.
std::map<std::string, void*> BaselineSymbols() {
    return {
        {"?PassCmdLineHandlerPtr@@YAXPEAX@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?PassErrorHandlerPtr@@YAXPEAX@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?PassEscapePtr@@YAXPEAX@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?PassBreakOutPtr@@YAXPEAX@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?PassDataStatementPtr@@YAXPEAD0@Z", reinterpret_cast<void*>(&StubTwoPointers)},
        {"?PassDLLs@@YAXXZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?ConstructDLLs@@YAXXZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?GetGlobPtr@@YAPEAUGlobStruct@@XZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?InitDisplay@@YAKKKKKPEAUHINSTANCE__@@PEAD@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?CloseDisplay@@YAKXZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?CreateVariableSpace@@YA_KK@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?DeleteVariableSpace@@YAXXZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?CreateDataSpace@@YA_KK@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?DeleteDataSpace@@YAXXZ", reinterpret_cast<void*>(&StubOnePointer)},
        {"?DeleteSingleVariableAllocation@@YAXPEA_K@Z", reinterpret_cast<void*>(&StubOnePointer)},
        {"?UnDimDD@@YA_K_K@Z", reinterpret_cast<void*>(&StubOnePointer)},
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
    symbols.erase("?PassErrorHandlerPtr@@YAXPEAX@Z");

    const auto result = ResolveCoreRuntimeApi(
        Lookup(std::move(symbols)), DeriveProgramRuntimeRequirements(0));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, CoreApiErrorCode::MissingRequiredExport);
    EXPECT_EQ(result.error().exportName, "?PassErrorHandlerPtr@@YAXPEAX@Z");
}

TEST(CoreRuntimeApiTest, OmitsUnusedStructureFunctionSafely) {
    const auto result = ResolveCoreRuntimeApi(
        Lookup(BaselineSymbols()), DeriveProgramRuntimeRequirements(0));

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result.value().passStructurePatterns, nullptr);
}

TEST(CoreRuntimeApiTest, RejectsRequiredStructureFunctionWhenAbsent) {
    const auto result = ResolveCoreRuntimeApi(
        Lookup(BaselineSymbols()), DeriveProgramRuntimeRequirements(3));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().exportName, "?PassStructurePatterns@@YAXPEAXK@Z");
}

TEST(CoreRuntimeApiTest, ResolvesRequiredModernStructureFunction) {
    auto symbols = BaselineSymbols();
    symbols["?PassStructurePatterns@@YAXPEAXK@Z"] =
        reinterpret_cast<void*>(&StubStructurePatterns);

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
