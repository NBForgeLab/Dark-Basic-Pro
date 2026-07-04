# Phase 10: Modern Windows & DirectX APIs

## 🎯 Goal
Replace obsolete and deprecated Win32 and DirectX technologies. These old modules are prone to security vulnerabilities, perform poorly, or are disabled by default on modern Windows 10/11 platforms. Modernizing these APIs ensures games run out-of-the-box without requiring legacy operating system compatibility configurations.

---

## 🛠️ Targeted Technologies & Replacements

### 1. Replace DirectPlay (Multiplayer Networking)
* **Problem**: The networking and multiplayer SDK modules rely on **DirectPlay**, which Microsoft deprecated in 2004. DirectPlay is disabled by default on modern Windows, forcing players to manually install legacy features to run multiplayer games.
* **Modern Replacement**:
   * Rewrite networking modules using **ENet** (a fast, UDP-based multiplayer library designed specifically for games).
   * Alternatively, integrate **Valve GameNetworkingSockets** for encrypted, high-quality P2P and client-server communication.

### 2. Upgrade DirectInput8 (Keyboard & Mouse input)
* **Problem**: Keyboard and mouse inputs are processed via **DirectInput8**. Microsoft recommends against using DirectInput for keyboard and mouse because it misses system messages (e.g., standard layout switches, focus changes, high-DPI scaling) and performs poorly under multi-monitor setups.
* **Modern Replacement**:
   * Process mouse and keyboard events directly using standard Windows message events (`WM_KEYDOWN`, `WM_MOUSEMOVE`) or use **Raw Input (`WM_INPUT`)** for high-precision, low-latency gaming mouse movement.

### 3. Replace Legacy D3DX9 Utilities
* **Problem**: Math, texture loading, and mesh utilities rely on `d3dx9.lib`, which is deprecated and missing from modern Windows SDKs.
* **Modern Replacement**:
   * Replace with Microsoft's modern, open-source replacements:
     * **DirectXTex**: For robust, secure image and texture compression/decompression.
     * **DirectXMesh**: For high-performance mesh transformations and optimizations.
     * **DirectXMath**: A high-performance, SIMD-accelerated math library for vector and matrix calculations.

### 4. Upgrade DirectShow for Media Playback
* **Problem**: Audio and video playback relies on DirectShow (`BaseClasses` in the SDK folder), which is legacy and difficult to set up on CMake.
* **Modern Replacement**:
   * Use **Windows Media Foundation (WMF)** or integrate a modern audio library like **miniaudio** for cross-platform, lightweight audio stream decoding.

---

## 🚀 Benefits
* **Zero Configuration**: Games execute instantly on Windows 11 without requesting legacy component installations.
* **Low Input Latency**: Raw Input integration minimizes input lag.
* **Modern Math & Graphics**: DirectXMath takes advantage of SSE/AVX vectors on modern CPUs, accelerating 3D math operations.
