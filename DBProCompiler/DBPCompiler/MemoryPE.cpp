#include "MemoryPE.h"
#include "VFSHooks.h"
#include <unordered_map>
#include <vector>
#include <iostream>

struct MemoryModuleInfo {
    BYTE* baseAddress = nullptr;
    IMAGE_NT_HEADERS* ntHeaders = nullptr;
    std::string name;
    bool initialized = false;
};

static std::unordered_map<HMODULE, MemoryModuleInfo*> g_memoryModules;

HMODULE MemoryPE::LoadFromVFS(const std::string& filename) {
    if (!VFSRegistry::Exists(filename)) {
        return nullptr;
    }
    const VFSFile* f = VFSRegistry::Get(filename);
    return LoadFromMemory(f->dataPtr, f->size, filename);
}

HMODULE MemoryPE::LoadFromMemory(const char* data, size_t size, const std::string& name) {
    if (size < sizeof(IMAGE_DOS_HEADER)) return nullptr;
    
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)data;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    
    if (size < dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS)) return nullptr;
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)(data + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return nullptr;
    
    // Allocate memory for image
    BYTE* baseAddress = (BYTE*)VirtualAlloc(nullptr, ntHeaders->OptionalHeader.SizeOfImage, 
                                            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!baseAddress) return nullptr;
    
    // Copy headers
    memcpy(baseAddress, data, ntHeaders->OptionalHeader.SizeOfHeaders);
    
    // Update NtHeaders pointer to the allocated memory copy
    IMAGE_NT_HEADERS* destNtHeaders = (IMAGE_NT_HEADERS*)(baseAddress + dosHeader->e_lfanew);
    
    // Copy sections
    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(destNtHeaders);
    for (int i = 0; i < destNtHeaders->FileHeader.NumberOfSections; i++, section++) {
        if (section->SizeOfRawData == 0) continue;
        BYTE* destSection = baseAddress + section->VirtualAddress;
        memcpy(destSection, data + section->PointerToRawData, section->SizeOfRawData);
    }
    
    // Apply base relocations
    IMAGE_DATA_DIRECTORY* relocDir = &destNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (relocDir->Size > 0) {
        DWORD_PTR delta = (DWORD_PTR)(baseAddress - destNtHeaders->OptionalHeader.ImageBase);
        if (delta != 0) {
            IMAGE_BASE_RELOCATION* reloc = (IMAGE_BASE_RELOCATION*)(baseAddress + relocDir->VirtualAddress);
            while (reloc->VirtualAddress != 0) {
                DWORD size = reloc->SizeOfBlock;
                WORD* relInfo = (WORD*)((BYTE*)reloc + sizeof(IMAGE_BASE_RELOCATION));
                DWORD count = (size - sizeof(IMAGE_BASE_RELOCATION)) / 2;
                for (DWORD i = 0; i < count; i++) {
                    WORD type = relInfo[i] >> 12;
                    WORD offset = relInfo[i] & 0x0FFF;
                    if (type == IMAGE_REL_BASED_HIGHLOW) {
                        DWORD* patchAddr = (DWORD*)(baseAddress + reloc->VirtualAddress + offset);
                        *patchAddr += (DWORD)delta;
                    }
                }
                reloc = (IMAGE_BASE_RELOCATION*)((BYTE*)reloc + size);
            }
        }
    }
    
    // Resolve imports
    IMAGE_DATA_DIRECTORY* importDir = &destNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir->Size > 0) {
        IMAGE_IMPORT_DESCRIPTOR* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)(baseAddress + importDir->VirtualAddress);
        while (importDesc->Name != 0) {
            const char* dllName = (const char*)(baseAddress + importDesc->Name);
            
            // Check VFS first
            HMODULE hImport = LoadFromVFS(dllName);
            if (!hImport) {
                hImport = LoadLibraryA(dllName);
            }
            
            if (!hImport) {
                VirtualFree(baseAddress, 0, MEM_RELEASE);
                return nullptr;
            }
            
            IMAGE_THUNK_DATA* thunk = (IMAGE_THUNK_DATA*)(baseAddress + importDesc->FirstThunk);
            IMAGE_THUNK_DATA* origThunk = importDesc->OriginalFirstThunk ? 
                (IMAGE_THUNK_DATA*)(baseAddress + importDesc->OriginalFirstThunk) : thunk;
                
            while (origThunk->u1.AddressOfData != 0) {
                FARPROC proc = nullptr;
                if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) {
                    LPCSTR ordinal = (LPCSTR)IMAGE_ORDINAL(origThunk->u1.Ordinal);
                    proc = IsMemoryModule(hImport) ? GetProcAddress(hImport, ordinal) : ::GetProcAddress(hImport, ordinal);
                } else {
                    IMAGE_IMPORT_BY_NAME* importName = (IMAGE_IMPORT_BY_NAME*)(baseAddress + origThunk->u1.AddressOfData);
                    proc = IsMemoryModule(hImport) ? GetProcAddress(hImport, importName->Name) : ::GetProcAddress(hImport, importName->Name);
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
        }
    }
    
    // Register module
    HMODULE hModule = (HMODULE)baseAddress;
    MemoryModuleInfo* info = new MemoryModuleInfo{baseAddress, destNtHeaders, name, false};
    g_memoryModules[hModule] = info;
    
    // Invoke DllMain
    typedef BOOL (WINAPI *DllMain_t)(HINSTANCE, DWORD, LPVOID);
    if (destNtHeaders->OptionalHeader.AddressOfEntryPoint != 0) {
        DllMain_t pDllMain = (DllMain_t)(baseAddress + destNtHeaders->OptionalHeader.AddressOfEntryPoint);
        pDllMain((HINSTANCE)baseAddress, DLL_PROCESS_ATTACH, nullptr);
        info->initialized = true;
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
    
    MemoryModuleInfo* mod = it->second;
    BYTE* baseAddress = mod->baseAddress;
    IMAGE_DATA_DIRECTORY* exportDir = &mod->ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDir->Size == 0) return nullptr;
    
    IMAGE_EXPORT_DIRECTORY* exports = (IMAGE_EXPORT_DIRECTORY*)(baseAddress + exportDir->VirtualAddress);
    DWORD* functions = (DWORD*)(baseAddress + exports->AddressOfFunctions);
    DWORD* names = (DWORD*)(baseAddress + exports->AddressOfNames);
    WORD* ordinals = (WORD*)(baseAddress + exports->AddressOfNameOrdinals);
    
    if ((DWORD)lpProcName >> 16 == 0) {
        WORD ordinal = (WORD)(DWORD)lpProcName - exports->Base;
        if (ordinal >= exports->NumberOfFunctions) return nullptr;
        return (FARPROC)(baseAddress + functions[ordinal]);
    }
    
    for (DWORD i = 0; i < exports->NumberOfNames; i++) {
        const char* name = (const char*)(baseAddress + names[i]);
        if (strcmp(name, lpProcName) == 0) {
            WORD ordinal = ordinals[i];
            return (FARPROC)(baseAddress + functions[ordinal]);
        }
    }
    return nullptr;
}

void MemoryPE::UnloadModule(HMODULE hModule) {
    auto it = g_memoryModules.find(hModule);
    if (it == g_memoryModules.end()) return;
    
    MemoryModuleInfo* info = it->second;
    if (info->initialized && info->ntHeaders->OptionalHeader.AddressOfEntryPoint != 0) {
        typedef BOOL (WINAPI *DllMain_t)(HINSTANCE, DWORD, LPVOID);
        DllMain_t pDllMain = (DllMain_t)(info->baseAddress + info->ntHeaders->OptionalHeader.AddressOfEntryPoint);
        pDllMain((HINSTANCE)info->baseAddress, DLL_PROCESS_DETACH, nullptr);
    }
    
    VirtualFree(info->baseAddress, 0, MEM_RELEASE);
    delete info;
    g_memoryModules.erase(it);
}

void MemoryPE::FreeAll() {
    auto it = g_memoryModules.begin();
    while (it != g_memoryModules.end()) {
        MemoryModuleInfo* info = it->second;
        if (info->initialized && info->ntHeaders->OptionalHeader.AddressOfEntryPoint != 0) {
            typedef BOOL (WINAPI *DllMain_t)(HINSTANCE, DWORD, LPVOID);
            DllMain_t pDllMain = (DllMain_t)(info->baseAddress + info->ntHeaders->OptionalHeader.AddressOfEntryPoint);
            pDllMain((HINSTANCE)info->baseAddress, DLL_PROCESS_DETACH, nullptr);
        }
        VirtualFree(info->baseAddress, 0, MEM_RELEASE);
        delete info;
        it = g_memoryModules.erase(it);
    }
}
