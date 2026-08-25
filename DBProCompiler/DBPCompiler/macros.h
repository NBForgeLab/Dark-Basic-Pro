#pragma once
#include "resource.h"
#include <cstdint>
#include <utility>

//
// Common Memory Management Utilities (ISO C++20)
//

template<typename T>
constexpr void SafeDelete(T*& ptr) noexcept {
    delete ptr;
    ptr = nullptr;
}

template<typename T>
constexpr void SafeDeleteArray(T*& ptr) noexcept {
    delete[] ptr;
    ptr = nullptr;
}

template<typename T>
constexpr void SafeRelease(T*& ptr) noexcept {
    if (ptr) {
        ptr->Release();
        ptr = nullptr;
    }
}

#include "DB3.h"

#ifdef UNICODE
#undef GetPrivateProfileString
#define GetPrivateProfileString GetPrivateProfileStringA
#undef WritePrivateProfileString
#define WritePrivateProfileString WritePrivateProfileStringA
#undef wsprintf
#define wsprintf wsprintfA
#endif
