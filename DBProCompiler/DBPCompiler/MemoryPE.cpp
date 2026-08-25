#include "MemoryPE.h"
#include "VFSHooks.h"
#include "SafeDLLLoading.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <limits>
#include <memory>

struct MemoryModuleInfo {
    BYTE* baseAddress = nullptr;
    IMAGE_NT_HEADERS* ntHeaders = nullptr;
    std::string name;
    bool initialized = false;
};

static std::unordered_map<HMODULE, std::unique_ptr<MemoryModuleInfo>> g_memoryModules;

HMODULE MemoryPE::LoadFromVFS(std::string_view filename) {
    const auto opened = VFSRegistry::Open(filename);
    if (!opened) {
        return nullptr;
    }
    const auto size = opened.value()->Size();
    if (size > (std::numeric_limits<std::size_t>::max)()) {
        return nullptr;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    std::size_t totalRead = 0;
    while (totalRead < bytes.size()) {
        const auto read = opened.value()->Read(
            bytes.data() + totalRead,
            bytes.size() - totalRead);
        if (!read || read.value() == 0) {
            return nullptr;
        }
        totalRead += read.value();
    }
    return LoadFromMemory(
        reinterpret_cast<const char*>(bytes.data()),
        bytes.size(),
        filename);
}

HMODULE MemoryPE::LoadFromMemory(const char* data, size_t size, std::string_view name) {
    if (size < sizeof(IMAGE_DOS_HEADER)) return nullptr;
    
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)data;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    
    if (dosHeader->e_lfanew < 0 || (size_t)dosHeader->e_lfanew > size - sizeof(IMAGE_NT_HEADERS)) return nullptr;
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)(data + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return nullptr;
    
    // Native x64 loader: only 64-bit PE images are mappable. PE32 (x86) images
    // cannot be relocated or executed in this process and are rejected here.
    if (ntHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || ntHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return nullptr;
    }

    if (ntHeaders->OptionalHeader.SizeOfHeaders > size) return nullptr;
    if (ntHeaders->OptionalHeader.SizeOfHeaders < sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS)) return nullptr;
    if (ntHeaders->FileHeader.NumberOfSections > 96) return nullptr;

    size_t sectionTableOffset = dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS);
    size_t sectionTableSize = ntHeaders->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
    if (sectionTableOffset + sectionTableSize > ntHeaders->OptionalHeader.SizeOfHeaders) return nullptr;

    // Allocate memory for image (initially PAGE_READWRITE for mapping and patching)
    BYTE* baseAddress = (BYTE*)VirtualAlloc(nullptr, ntHeaders->OptionalHeader.SizeOfImage, 
                                            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!baseAddress) return nullptr;
    
    // Copy headers
    memcpy(baseAddress, data, ntHeaders->OptionalHeader.SizeOfHeaders);
    
    // Update NtHeaders pointer to the allocated memory copy
    IMAGE_NT_HEADERS* destNtHeaders = (IMAGE_NT_HEADERS*)(baseAddress + dosHeader->e_lfanew);
    
    // Copy sections
    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(destNtHeaders);
    for (int i = 0; i < destNtHeaders->FileHeader.NumberOfSections; i++, section++) {
        if (section->SizeOfRawData == 0) continue;
        if (section->PointerToRawData > size || section->SizeOfRawData > size - section->PointerToRawData) {
            VirtualFree(baseAddress, 0, MEM_RELEASE);
            return nullptr;
        }
        if (section->VirtualAddress > destNtHeaders->OptionalHeader.SizeOfImage || 
            section->SizeOfRawData > destNtHeaders->OptionalHeader.SizeOfImage - section->VirtualAddress) {
            VirtualFree(baseAddress, 0, MEM_RELEASE);
            return nullptr;
        }
        BYTE* destSection = baseAddress + section->VirtualAddress;
        memcpy(destSection, data + section->PointerToRawData, section->SizeOfRawData);
    }
    
    // Apply base relocations
    IMAGE_DATA_DIRECTORY* relocDir = &destNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (relocDir->Size > 0) {
        if (relocDir->VirtualAddress > destNtHeaders->OptionalHeader.SizeOfImage || 
            relocDir->Size > destNtHeaders->OptionalHeader.SizeOfImage - relocDir->VirtualAddress) {
            VirtualFree(baseAddress, 0, MEM_RELEASE);
            return nullptr;
        }
        DWORD_PTR delta = (DWORD_PTR)(baseAddress - destNtHeaders->OptionalHeader.ImageBase);
        if (delta != 0) {
            IMAGE_BASE_RELOCATION* reloc = (IMAGE_BASE_RELOCATION*)(baseAddress + relocDir->VirtualAddress);
            size_t parsedBytes = 0;
            while (reloc->VirtualAddress != 0 && parsedBytes < relocDir->Size) {
                DWORD sizeOfBlock = reloc->SizeOfBlock;
                if (sizeOfBlock == 0 || sizeOfBlock < sizeof(IMAGE_BASE_RELOCATION)) {
                    VirtualFree(baseAddress, 0, MEM_RELEASE);
                    return nullptr;
                }
                if (parsedBytes + sizeOfBlock > relocDir->Size) {
                    VirtualFree(baseAddress, 0, MEM_RELEASE);
                    return nullptr;
                }
                WORD* relInfo = (WORD*)((BYTE*)reloc + sizeof(IMAGE_BASE_RELOCATION));
                DWORD count = (sizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;
                for (DWORD i = 0; i < count; i++) {
                    WORD type = relInfo[i] >> 12;
                    WORD offset = relInfo[i] & 0x0FFF;
                    if (type == IMAGE_REL_BASED_HIGHLOW) {
                        DWORD rva = reloc->VirtualAddress + offset;
                        if (rva + 4 > destNtHeaders->OptionalHeader.SizeOfImage) {
                            VirtualFree(baseAddress, 0, MEM_RELEASE);
                            return nullptr;
                        }
                        DWORD* patchAddr = (DWORD*)(baseAddress + rva);
                        *patchAddr += (DWORD)delta;
                    }
                    else if (type == IMAGE_REL_BASED_DIR64) {
                        DWORD rva = reloc->VirtualAddress + offset;
                        if (rva + 8 > destNtHeaders->OptionalHeader.SizeOfImage) {
                            VirtualFree(baseAddress, 0, MEM_RELEASE);
                            return nullptr;
                        }
                        uintptr_t* patchAddr = (uintptr_t*)(baseAddress + rva);
                        *patchAddr += (uintptr_t)delta;
                    }
                }
                parsedBytes += sizeOfBlock;
                reloc = (IMAGE_BASE_RELOCATION*)((BYTE*)reloc + sizeOfBlock);
            }
        }
    }
    
    // Resolve imports
    IMAGE_DATA_DIRECTORY* importDir = &destNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir->Size > 0) {
        if (importDir->VirtualAddress > destNtHeaders->OptionalHeader.SizeOfImage || 
            importDir->Size > destNtHeaders->OptionalHeader.SizeOfImage - importDir->VirtualAddress) {
            VirtualFree(baseAddress, 0, MEM_RELEASE);
            return nullptr;
        }
        IMAGE_IMPORT_DESCRIPTOR* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)(baseAddress + importDir->VirtualAddress);
        size_t parsedDescBytes = 0;
        HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
        while (parsedDescBytes + sizeof(IMAGE_IMPORT_DESCRIPTOR) <= importDir->Size && importDesc->Name != 0) {
            if (importDesc->Name > destNtHeaders->OptionalHeader.SizeOfImage) {
                VirtualFree(baseAddress, 0, MEM_RELEASE);
                return nullptr;
            }
            const char* dllName = (const char*)(baseAddress + importDesc->Name);
            size_t maxDllNameLen = destNtHeaders->OptionalHeader.SizeOfImage - importDesc->Name;
            size_t dllNameLen = 0;
            while (dllNameLen < maxDllNameLen && dllName[dllNameLen] != '\0') dllNameLen++;
            if (dllNameLen == maxDllNameLen) {
                VirtualFree(baseAddress, 0, MEM_RELEASE);
                return nullptr;
            }
            
            // Check VFS first
            HMODULE hImport = LoadFromVFS(dllName);
            if (!hImport) {
                hImport = dbp::dll::LoadApplicationDLLA(dllName);
            }
            
            if (!hImport) {
                VirtualFree(baseAddress, 0, MEM_RELEASE);
                return nullptr;
            }
            
            bool isKernel32 = (hImport == hKernel32);
            
            if (importDesc->FirstThunk > destNtHeaders->OptionalHeader.SizeOfImage) {
                VirtualFree(baseAddress, 0, MEM_RELEASE);
                return nullptr;
            }
            IMAGE_THUNK_DATA* thunk = (IMAGE_THUNK_DATA*)(baseAddress + importDesc->FirstThunk);
            IMAGE_THUNK_DATA* origThunk = importDesc->OriginalFirstThunk ? 
                (IMAGE_THUNK_DATA*)(baseAddress + importDesc->OriginalFirstThunk) : thunk;
            
            if ((BYTE*)origThunk < baseAddress || (BYTE*)origThunk > baseAddress + destNtHeaders->OptionalHeader.SizeOfImage) {
                VirtualFree(baseAddress, 0, MEM_RELEASE);
                return nullptr;
            }
                
            while (origThunk->u1.AddressOfData != 0) {
                FARPROC proc = nullptr;
                if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) {
                    LPCSTR ordinal = (LPCSTR)IMAGE_ORDINAL(origThunk->u1.Ordinal);
                    proc = IsMemoryModule(hImport) ? GetProcAddress(hImport, ordinal) : ::GetProcAddress(hImport, ordinal);
                } else {
                    if (origThunk->u1.AddressOfData > destNtHeaders->OptionalHeader.SizeOfImage) {
                        VirtualFree(baseAddress, 0, MEM_RELEASE);
                        return nullptr;
                    }
                    IMAGE_IMPORT_BY_NAME* importName = (IMAGE_IMPORT_BY_NAME*)(baseAddress + origThunk->u1.AddressOfData);
                    const char* funcName = (const char*)importName->Name;
                    
                    // IAT Redirection / Hook Interception for memory loaded modules
                    if (strcmp(funcName, "CreateFileW") == 0) proc = (FARPROC)Hook_CreateFileW;
                    else if (strcmp(funcName, "CreateFileA") == 0) proc = (FARPROC)Hook_CreateFileA;
                    else if (strcmp(funcName, "ReadFile") == 0) proc = (FARPROC)Hook_ReadFile;
                    else if (strcmp(funcName, "GetFileSize") == 0) proc = (FARPROC)Hook_GetFileSize;
                    else if (strcmp(funcName, "SetFilePointer") == 0) proc = (FARPROC)Hook_SetFilePointer;
                    else if (strcmp(funcName, "SetFilePointerEx") == 0) proc = (FARPROC)Hook_SetFilePointerEx;
                    else if (strcmp(funcName, "CloseHandle") == 0) proc = (FARPROC)Hook_CloseHandle;
                    else if (strcmp(funcName, "GetProcAddress") == 0) proc = (FARPROC)Hook_GetProcAddress;
                    else if (strcmp(funcName, "LoadLibraryA") == 0) proc = (FARPROC)Hook_LoadLibraryA;
                    else if (strcmp(funcName, "LoadLibraryW") == 0) proc = (FARPROC)Hook_LoadLibraryW;
                    
                    if (!proc) {
                        proc = IsMemoryModule(hImport) ? GetProcAddress(hImport, funcName) : ::GetProcAddress(hImport, funcName);
                    }
                }
                
                if (!proc) {
                    VirtualFree(baseAddress, 0, MEM_RELEASE);
                    return nullptr;
                }
                
                thunk->u1.Function = (DWORD_PTR)proc;
                thunk++;
                origThunk++;
            }
            importDesc++;
            parsedDescBytes += sizeof(IMAGE_IMPORT_DESCRIPTOR);
        }
    }
    
    // Apply dynamic section permissions (W^X & DEP Protection)
    {
        DWORD oldProtect;
        // Protect headers first (Read-only)
        VirtualProtect(baseAddress, destNtHeaders->OptionalHeader.SizeOfHeaders, PAGE_READONLY, &oldProtect);
        
        // Protect each section based on characteristics
        IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(destNtHeaders);
        for (int i = 0; i < destNtHeaders->FileHeader.NumberOfSections; i++, sec++) {
            const DWORD mappedSize = (std::max)(
                sec->Misc.VirtualSize, sec->SizeOfRawData);
            if (mappedSize == 0U) continue;
            if (sec->VirtualAddress >
                    destNtHeaders->OptionalHeader.SizeOfImage ||
                mappedSize >
                    destNtHeaders->OptionalHeader.SizeOfImage -
                        sec->VirtualAddress) {
                VirtualFree(baseAddress, 0, MEM_RELEASE);
                return nullptr;
            }
            DWORD protect = PAGE_NOACCESS;
            bool isExecutable = (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
            bool isReadable = (sec->Characteristics & IMAGE_SCN_MEM_READ) != 0;
            bool isWritable = (sec->Characteristics & IMAGE_SCN_MEM_WRITE) != 0;
            
            if (isExecutable) {
                if (isWritable) protect = PAGE_EXECUTE_READWRITE;
                else if (isReadable) protect = PAGE_EXECUTE_READ;
                else protect = PAGE_EXECUTE;
            } else {
                if (isWritable) protect = PAGE_READWRITE;
                else if (isReadable) protect = PAGE_READONLY;
                else protect = PAGE_NOACCESS;
            }
            if (!VirtualProtect(
                    baseAddress + sec->VirtualAddress,
                    mappedSize,
                    protect,
                    &oldProtect)) {
                VirtualFree(baseAddress, 0, MEM_RELEASE);
                return nullptr;
            }
        }
    }
    
    // Register module
    HMODULE hModule = (HMODULE)baseAddress;
    auto info = std::make_unique<MemoryModuleInfo>(MemoryModuleInfo{baseAddress, destNtHeaders, std::string(name), false});
    MemoryModuleInfo* infoPtr = info.get();
    g_memoryModules[hModule] = std::move(info);

    char dbgBuf[512];
    sprintf_s(dbgBuf, "[MemoryPE] Loaded module '%.*s' at %p (Entry=%x, Size=%x)\n", static_cast<int>(name.size()), name.data(), baseAddress, destNtHeaders->OptionalHeader.AddressOfEntryPoint, destNtHeaders->OptionalHeader.SizeOfImage);
    OutputDebugStringA(dbgBuf);
    
    // Invoke DllMain
    typedef BOOL (WINAPI *DllMain_t)(HINSTANCE, DWORD, LPVOID);
    if (destNtHeaders->OptionalHeader.AddressOfEntryPoint != 0) {
        DllMain_t pDllMain = (DllMain_t)(baseAddress + destNtHeaders->OptionalHeader.AddressOfEntryPoint);
        pDllMain((HINSTANCE)baseAddress, DLL_PROCESS_ATTACH, nullptr);
        infoPtr->initialized = true;
    }
    
    // Call TLS callbacks if they exist
    /*
    IMAGE_DATA_DIRECTORY* tlsDir = &destNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
    if (tlsDir->Size > 0) {
        IMAGE_TLS_DIRECTORY* tls = (IMAGE_TLS_DIRECTORY*)(baseAddress + tlsDir->VirtualAddress);
        PIMAGE_TLS_CALLBACK* callback = (PIMAGE_TLS_CALLBACK*)tls->AddressOfCallbacks;
        if (callback) {
            while (*callback) {
                (*callback)((void*)baseAddress, DLL_PROCESS_ATTACH, nullptr);
                callback++;
            }
        }
    }
    */
    
    return hModule;
}

bool MemoryPE::IsMemoryModule(HMODULE hModule) {
    return g_memoryModules.find(hModule) != g_memoryModules.end();
}

FARPROC MemoryPE::GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    auto it = g_memoryModules.find(hModule);
    if (it == g_memoryModules.end()) return nullptr;
    
    MemoryModuleInfo* mod = it->second.get();
    BYTE* baseAddress = mod->baseAddress;
    IMAGE_DATA_DIRECTORY* exportDir = &mod->ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDir->Size == 0) return nullptr;
    
    IMAGE_EXPORT_DIRECTORY* exports = (IMAGE_EXPORT_DIRECTORY*)(baseAddress + exportDir->VirtualAddress);
    DWORD* functions = (DWORD*)(baseAddress + exports->AddressOfFunctions);
    DWORD* names = (DWORD*)(baseAddress + exports->AddressOfNames);
    WORD* ordinals = (WORD*)(baseAddress + exports->AddressOfNameOrdinals);
    
    const auto procedureValue = reinterpret_cast<uintptr_t>(lpProcName);
    if ((procedureValue >> 16) == 0) {
        const DWORD requestedOrdinal =
            static_cast<DWORD>(procedureValue & 0xFFFFu);
        if (requestedOrdinal < exports->Base) return nullptr;
        const DWORD functionIndex = requestedOrdinal - exports->Base;
        if (functionIndex >= exports->NumberOfFunctions) return nullptr;
        return reinterpret_cast<FARPROC>(
            baseAddress + functions[functionIndex]);
    }
    
    // 1. Exact match
    for (DWORD i = 0; i < exports->NumberOfNames; i++) {
        const char* name = (const char*)(baseAddress + names[i]);
        if (strcmp(name, lpProcName) == 0) {
            const DWORD functionIndex = ordinals[i];
            if (functionIndex >= exports->NumberOfFunctions) return nullptr;
            return reinterpret_cast<FARPROC>(
                baseAddress + functions[functionIndex]);
        }
    }

    // 2. Mangling compatibility fallback: Handle 32-bit (YAPA) vs 64-bit (YAPEA) / undecorated match
    const char* atAt = strstr(lpProcName, "@@");
    if (lpProcName[0] == '?' && atAt) {
        size_t baseLen = (atAt - lpProcName) + 2;
        for (DWORD i = 0; i < exports->NumberOfNames; i++) {
            const char* name = (const char*)(baseAddress + names[i]);
            if (strncmp(name, lpProcName, baseLen) == 0) {
                const DWORD functionIndex = ordinals[i];
                if (functionIndex >= exports->NumberOfFunctions) return nullptr;
                return reinterpret_cast<FARPROC>(
                    baseAddress + functions[functionIndex]);
            }
        }
    }
    return nullptr;
}

std::optional<MemoryPEAddressInfo> MemoryPE::InspectAddress(
    const void* const address) {
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    for (const auto& [module, ownedInfo] : g_memoryModules) {
        const auto* const info = ownedInfo.get();
        const auto base = reinterpret_cast<std::uintptr_t>(
            info->baseAddress);
        const auto imageSize =
            info->ntHeaders->OptionalHeader.SizeOfImage;
        if (value < base || value - base >= imageSize) {
            continue;
        }

        const auto rva = static_cast<DWORD>(value - base);
        IMAGE_SECTION_HEADER* section =
            IMAGE_FIRST_SECTION(info->ntHeaders);
        for (WORD index = 0U;
             index < info->ntHeaders->FileHeader.NumberOfSections;
             ++index, ++section) {
            const DWORD mappedSize = (std::max)(
                section->Misc.VirtualSize, section->SizeOfRawData);
            if (rva < section->VirtualAddress ||
                rva - section->VirtualAddress >= mappedSize) {
                continue;
            }

            char sectionName[IMAGE_SIZEOF_SHORT_NAME + 1U]{};
            std::memcpy(
                sectionName,
                section->Name,
                IMAGE_SIZEOF_SHORT_NAME);
            return MemoryPEAddressInfo{
                info->name,
                sectionName,
                rva,
                section->Characteristics};
        }
        return MemoryPEAddressInfo{info->name, {}, rva, 0U};
    }
    return std::nullopt;
}

void MemoryPE::UnloadModule(HMODULE hModule) {
    auto it = g_memoryModules.find(hModule);
    if (it == g_memoryModules.end()) return;
    
    MemoryModuleInfo* info = it->second.get();
    if (info->initialized && info->ntHeaders->OptionalHeader.AddressOfEntryPoint != 0) {
        typedef BOOL (WINAPI *DllMain_t)(HINSTANCE, DWORD, LPVOID);
        DllMain_t pDllMain = (DllMain_t)(info->baseAddress + info->ntHeaders->OptionalHeader.AddressOfEntryPoint);
        pDllMain((HINSTANCE)info->baseAddress, DLL_PROCESS_DETACH, nullptr);
    }
    
    VirtualFree(info->baseAddress, 0, MEM_RELEASE);
    g_memoryModules.erase(it);
}

void MemoryPE::FreeAll() {
    auto it = g_memoryModules.begin();
    while (it != g_memoryModules.end()) {
        MemoryModuleInfo* info = it->second.get();
        if (info->initialized && info->ntHeaders->OptionalHeader.AddressOfEntryPoint != 0) {
            typedef BOOL (WINAPI *DllMain_t)(HINSTANCE, DWORD, LPVOID);
            DllMain_t pDllMain = (DllMain_t)(info->baseAddress + info->ntHeaders->OptionalHeader.AddressOfEntryPoint);
            pDllMain((HINSTANCE)info->baseAddress, DLL_PROCESS_DETACH, nullptr);
        }
        VirtualFree(info->baseAddress, 0, MEM_RELEASE);
        it = g_memoryModules.erase(it);
    }
}
