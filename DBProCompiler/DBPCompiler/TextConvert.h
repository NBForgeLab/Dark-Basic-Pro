#pragma once
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
