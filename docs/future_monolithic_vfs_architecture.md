# Future Architecture Blueprint: Monolithic Static-Linking & Native Stream VFS

This document outlines the target design for a fully modernized **DarkBasic Pro** compiler and runtime environment. It is designed to be implemented once all legacy, closed-source dependencies (e.g., old converter DLLs, proprietary plugins) have been removed, recompiled, or replaced with modern open-source equivalents.

---

## 🗺️ Architectural Transition Overview

The current modernization phase implements a **Virtualization Layer** (using PE memory loading and Win32 API hooking) to support legacy, closed-source binary DLLs. 

The future architecture transitions the engine to a **Modern Monolithic Design**:

```
                              [ Legacy Design ]
              DarkEXE.exe ──> Extracts DLLs & Assets to Disk ──> Run
                                      │
                                      ▼
                         [ Phase 13 Virtualization ]
          DarkEXE.exe ──> Maps DLLs & Hooks APIs in Memory ──> Run
                                      │
                                      ▼
                           [ Future Modern Design ]
        Single Monolithic.exe (Statically Linked Engine + Embedded Assets)
              └── Native Stream-based Asset Manager (RAM Only)
```

---

## 🛠️ Step 1: Monolithic Static-Linking Model

Instead of splitting commands across 50+ dynamic link libraries (DLLs) like `Image.dll`, `Sound.dll`, or `Basic3D.dll`, all modules will be compiled as static libraries (`.lib`) and linked directly into a single, cohesive engine executable.

### 1. CMake Target Configuration
In the future CMake system, dynamic plugin libraries are converted into static targets:

```cmake
# Example CMake configuration for modern static modules
add_library(dbp_image_static STATIC
    ImageLoader.cpp
    TextureManager.cpp
)

add_library(dbp_sound_static STATIC
    SoundSystem.cpp
    AudioStream.cpp
)

# Link all static engine modules directly into the unified runner
add_executable(DarkRunner
    RunnerMain.cpp
    BytecodeInterpreter.cpp
)

target_link_libraries(DarkRunner PRIVATE
    dbp_image_static
    dbp_sound_static
    PhysFS::physfs # Static VFS library
)
```

### 2. Static Command Dispatching
Replace the legacy runtime dynamic linking lookup (`GetProcAddress`) with compile-time symbol resolution:

```cpp
// Future Unified Dispatch Table
#include "ImageStatic.h"
#include "SoundStatic.h"

typedef void (*CommandFunc)();

struct CommandEntry {
    const char* name;
    CommandFunc func;
};

// Dispatch table resolved statically at compile time
CommandEntry g_DispatchTable[] = {
    { "LOAD IMAGE",  (CommandFunc)ImageModule::LoadImage },
    { "PLAY SOUND",  (CommandFunc)SoundModule::PlaySound },
    // Additional engine commands...
};
```

---

## 📦 Step 2: Native Stream-Based Virtual File System (VFS)

Eliminate the need for Win32 API file hooking (`CreateFile` interception) by refactoring the asset loaders to natively accept memory buffers, streams, or virtual file handles provided by a library like **PhysicsFS (PhysFS)**.

### 1. Unified Stream API
Define a standard stream interface for reading assets directly from packed archives:

```cpp
#pragma once
#include <vector>
#include <string>
#include <memory>

class ResourceStream {
public:
    virtual ~ResourceStream() = default;
    virtual size_t Read(void* buffer, size_t size) = 0;
    virtual bool Seek(size_t offset) = 0;
    virtual size_t Tell() const = 0;
    virtual size_t GetSize() const = 0;
};

class VFSManager {
public:
    static bool Initialize(const std::string& archivePath);
    static void Shutdown();
    
    // Opens an asset directly from the mounted virtual archive
    static std::unique_ptr<ResourceStream> OpenRead(const std::string& path);
};
```

### 2. Stream-Based Asset Loading
Refactor all subsystem command libraries (e.g., Image, Sound) to load data directly from the stream interface instead of file paths:

```cpp
// Future modern Image subsystem loading from memory stream
void ImageModule::LoadImage(const char* virtualPath, int imageID) {
    auto stream = VFSManager::OpenRead(virtualPath);
    if (!stream) {
        // Report error natively
        return;
    }

    std::vector<uint8_t> buffer(stream->GetSize());
    stream->Read(buffer.data(), buffer.size());

    // Load texture using modern GPU APIs from memory buffer (DirectX 12 / Vulkan / D3D11)
    ID3D11ShaderResourceView* textureView = nullptr;
    HRESULT hr = DirectX::CreateWICTextureFromMemory(
        g_pd3dDevice, 
        buffer.data(), 
        buffer.size(), 
        nullptr, 
        &textureView
    );

    if (SUCCEEDED(hr)) {
        TextureRegistry::Register(imageID, textureView);
    }
}
```

---

## 📈 Architectural Comparison

| Metric / Dimension | Phase 13 Virtualization (Current) | Future Monolithic VFS (Target) |
| :--- | :--- | :--- |
| **Executable Target** | Multi-file (EXE + 50 DLLs packaged in PCK) | Single Monolithic EXE (All code statically linked) |
| **Memory Allocation** | Dynamic section copying & DLL entry patching | Standard OS process image mapping (fast and secure) |
| **API Detour Hooks** | Required (intercepting Win32 file calls) | **None** (assets read natively via custom stream classes) |
| **Startup Overhead** | Minor (due to in-memory relocation & hooking) | **Zero** (instantly initialized via native code) |
| **Code Access** | Mixed (some closed-source DLL dependencies) | 100% Modern Open-Source / Recompiled codebase |

---

## 🚀 Migration Strategy Roadmap

1. **Deprecate Unused Libraries**: Identify and remove legacy subsystems (e.g., DirectPlay-based multiplayer, DirectShow-based movie rendering) and replace them with modern lightweight libraries (e.g., ENet/WebRTC for network, libtheora/FFmpeg for video).
2. **Convert Subsystems to Static Libraries**: Re-write and wrap the source code of each core DBP subsystem (e.g., Input, System, Image, Sound) into individual C++ static library projects (`.lib`).
3. **Refactor Resource Handlers**: Rewrite functions that call file-loading APIs to read from memory buffers or stream descriptors.
4. **Remove Unpacking & Hooking Codes**: Delete `FileReader.cpp`, `MemoryPE.cpp`, and `VFSHooks.cpp`, relying entirely on native static linking and direct stream mounting.
