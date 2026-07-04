# Phase 9: Unicode Transition

## 🎯 Goal
Upgrade the compiler and engine to support **Unicode (UTF-8 / UTF-16)** instead of obsolete single-byte ANSI encoding. This ensures robust handling of non-English characters (especially Arabic) and prevents character corruption (Mojibake) on systems with different default locales.

---

## ⚠️ Current Issues with ANSI
The engine is currently designed to treat strings as single-byte ANSI buffers (`char*`):
* **Language Support**: Non-ASCII characters (e.g., Arabic text) display as garbage (question marks or square blocks) on machines where the default system locale is set to English.
* **Win32 API Binding**: The engine invokes deprecated `A` suffix Win32 API functions (`CreateWindowExA`, `MessageBoxA`), preventing Unicode-level interactions with the OS.

---

## 🛠️ Design: Hybrid UTF-8 / UTF-16 Encoding

We recommend adopting a modern standard practice (similar to Rust and modern C++):
1. **Internally**: Store all string variables in **UTF-8** (`std::string`) for compact memory usage and compatibility with standard file utilities.
2. **Win32 Boundary**: Convert UTF-8 to **UTF-16 (`std::wstring`)** when calling Windows API functions that require wide characters (`W` APIs).

### Text Conversion Utility
```cpp
#include <windows.h>
#include <string>

class TextConvert {
public:
    // Convert UTF-8 (std::string) to UTF-16 (std::wstring)
    static std::wstring UTF8ToUTF16(const std::string& utf8Str) {
        if (utf8Str.empty()) return L"";
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8Str[0], (int)utf8Str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &utf8Str[0], (int)utf8Str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    // Convert UTF-16 (std::wstring) to UTF-8 (std::string)
    static std::string UTF16ToUTF8(const std::wstring& utf16Str) {
        if (utf16Str.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &utf16Str[0], (int)utf16Str.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &utf16Str[0], (int)utf16Str.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }
};
```

---

## 🔄 Implementation Steps

1. **Enable Unicode Definitions**:
   * Configure compile flags in the root `CMakeLists.txt`:
     ```cmake
     add_compile_definitions(UNICODE _UNICODE)
     ```
2. **Refactor Win32 APIs**:
   * Change explicit `A` APIs to `W` APIs (e.g., `MessageBoxW`) and convert argument strings using `TextConvert::UTF8ToUTF16`.
3. **Update DarkBasic String Commands**:
   * Refactor string manipulation operations inside `String.dll` and `Core.dll` (e.g., `LEN`, `LEFT$`, `MID$`). The `LEN` function must count character code points rather than raw bytes to support multibyte languages properly.
4. **Unicode Font Rendering**:
   * Update the rendering engine (`Text.dll`) to load TrueType Fonts using Unicode maps to render non-English glyphs correctly in the game viewport.

---

## 🚀 Benefits
* **Arabic Support**: Allows complete localization of game text, interfaces, and console outputs without layout corruption.
* **Global Compatibility**: Games compile and run identically on any Windows machine worldwide regardless of default system language settings.
