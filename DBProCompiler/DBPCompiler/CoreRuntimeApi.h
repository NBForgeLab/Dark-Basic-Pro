#pragma once

#include "RuntimeContract.h"

#include <Windows.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>

using CorePassPointer = void (*)(void*);
using CorePassDataStatements = void (*)(char*, char*);
using CorePassStructurePatterns = void (*)(void*, unsigned long);
using CoreVoid = void (*)();
using CoreDword = DWORD (*)();
using CoreDwordParameter = DWORD (*)(DWORD);
// Space factories return a pointer-sized address (DWORD_PTR in the runtime)
using CoreCreateSpace = void* (*)(DWORD);
using CoreVoidDwordPointer = void (*)(DWORD*);
using CoreInitializeDisplay = DWORD (*)(
    DWORD, DWORD, DWORD, DWORD, HINSTANCE, char*);
using CoreGetGlob = void* (*)();
using CoreSymbolLookup = std::function<void*(const char*)>;

struct CoreRuntimeApi {
    CorePassPointer passCommandLine = nullptr;
    CorePassPointer passError = nullptr;
    CorePassPointer passEscape = nullptr;
    CorePassPointer passBreakout = nullptr;
    CorePassDataStatements passDataStatements = nullptr;
    CorePassStructurePatterns passStructurePatterns = nullptr;
    CoreVoid passDlls = nullptr;
    CoreVoid constructDlls = nullptr;
    CoreGetGlob getGlob = nullptr;
    CoreInitializeDisplay initializeDisplay = nullptr;
    CoreDword closeDisplay = nullptr;
    CoreCreateSpace createVariableSpace = nullptr;
    CoreVoid deleteVariableSpace = nullptr;
    CoreCreateSpace createDataSpace = nullptr;
    CoreVoid deleteDataSpace = nullptr;
    CoreVoidDwordPointer deleteVariableItem = nullptr;
    CoreVoidDwordPointer unDim = nullptr;
    CoreVoid sync = nullptr;
};

enum class CoreApiErrorCode { MissingRequiredExport };

struct CoreApiError {
    CoreApiErrorCode code;
    std::string exportName;
    std::string message;
};

class CoreApiResult {
public:
    static CoreApiResult Success(CoreRuntimeApi value) {
        return CoreApiResult(std::move(value));
    }
    static CoreApiResult Failure(CoreApiError error) {
        return CoreApiResult(std::move(error));
    }
    bool has_value() const noexcept { return value_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }
    const CoreRuntimeApi& value() const { return value_.value(); }
    const CoreApiError& error() const { return error_.value(); }

private:
    explicit CoreApiResult(CoreRuntimeApi value) : value_(std::move(value)) {}
    explicit CoreApiResult(CoreApiError error) : error_(std::move(error)) {}
    std::optional<CoreRuntimeApi> value_;
    std::optional<CoreApiError> error_;
};

CoreApiResult ResolveCoreRuntimeApi(
    const CoreSymbolLookup& lookup,
    const ProgramRuntimeRequirements& requirements);
