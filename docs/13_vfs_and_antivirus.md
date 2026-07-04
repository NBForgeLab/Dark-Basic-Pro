# Phase 13: Virtual File System (VFS) & Antivirus Protection

## 🎯 Goal
Replace the temporary directory unpacking mechanism with a **Virtual File System (VFS)**. This modernization prevents antivirus false-positive alerts on generated game executables and speeds up file asset loads by avoiding disk writes.

---

## ⚠️ Current Issues: Antivirus Triggers
The legacy runner [EXEBlock.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/EXEBlock.cpp) uses a self-extracting mechanism to execute games:
1. When the game executable starts, it creates a temporary subdirectory in Windows (`Temp`).
2. It extracts all engine components (`DLLs`) and assets (images, sounds, 3D meshes) to that disk directory.
3. It loads the DLLs dynamically from that directory.

* **Why is this a problem?**
  * Modern antivirus software (e.g., Windows Defender) flags any program that extracts DLLs/EXEs to temporary user directories and executes them as highly suspicious "Droppers" or "Trojan" installers.
  * Writing and reading files from disk causes heavy I/O overhead.

---

## 🛠️ Design: Memory-based Virtual File System

We recommend integrating **PhysicsFS (PhysFS)** or a similar lightweight archive-mounting system:

```
[Standalone EXE] ──> (Loads asset archive directly in memory)
                                │
                                ▼
                   (Virtual File System - VFS)
                                │
          ┌─────────────────────┴─────────────────────┐
          ▼                                           ▼
   [Image.dll Loader]                         [Sound.dll Loader]
  (Reads data from buffer)                   (Reads data from buffer)
```

### 1. How VFS Works
* The game's assets and DLLs are kept inside a single packed file (or embedded directly into the `.exe`).
* When the game requests an asset, the VFS reads the bytes directly from the archive in-memory.
* The asset data is loaded into RAM streams, bypassing disk writes entirely.

### 2. VFS API Design
```cpp
#pragma once
#include <vector>
#include <string>

class VFS {
public:
    static bool MountArchive(const std::string& archivePath);
    
    // Read file contents directly into a memory buffer
    static std::vector<uint8_t> ReadFileToBuffer(const std::string& virtualPath);
    
    // Check if file exists in the virtual archive
    static bool FileExists(const std::string& virtualPath);
};
```

---

## 🔄 Implementation Steps

1. **Integrate PhysFS in core modules**:
   * Add PhysFS as a static dependency in `Core.dll` and CMake lists.
2. **Refactor Resource DLLs**:
   * Modify texture and sound loader functions in `Image.dll` and `Sound.dll` to read from memory streams instead of file path strings. For instance, replace standard Direct3D file load functions with memory-based variants (`D3DXCreateTextureFromFileInMemory`).
3. **Asset Encryption**:
   * Optionally encrypt assets inside the VFS package to protect game developer content from piracy.

---

## 🚀 Benefits
* **Clean Antivirus Reputations**: Game executables will not be flagged or quarantined by Windows Defender because they do not drop executable code on user systems.
* **Instant Load Times**: Loading assets directly from a memory-mapped ZIP or PCK archive is significantly faster than standard disk I/O.
* **Compact Packaging**: Games distribute cleanly as a single executable containing all assets.
