//
// Common Memory Management Utilities (ISO C++20)
//

#pragma once
#include <cstdint>
#include <utility>

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

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) SafeRelease(p)
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) SafeDelete(p)
#endif

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) SafeDeleteArray(p)
#endif
