// SteamMultiplayer stub (x64 build).
//
// The real SteamMultiplayer.dll is backed by the Valve Steamworks SDK
// (steam_api.h / steam_api.lib), which is not vendored in this repository.
// Building the full implementation under CMake is therefore not possible
// without adding that external dependency.
//
// This stub provides minimal no-op implementations of the exports that the
// Dark Basic Pro Core actually consumes at runtime via LoadLibrary +
// GetProcAddress (DBDLLCore.cpp and SteamCheckForWorkshop.cpp). Supplying
// these exports lets the engine start and run without Steam features instead
// of crashing on a NULL function pointer when the real DLL is absent.
//
// The exact decorated export names are mandated by the Core's GetProcAddress
// lookups, which still use 32-bit name mangling (e.g. "@@YAXPAD@Z"). They are
// produced by the alias table in steam_stub.def.

#include <string.h>

extern "C" int SteamInitImpl(void)
{
    // Steam is unavailable: report failure, do nothing.
    return 0;
}

extern "C" void SteamGetWorkshopItemPathDLLImpl(char* pPath)
{
    if (pPath != nullptr)
    {
        pPath[0] = '\0';
    }
}

extern "C" int SteamIsWorkshopLoadingOnDLLImpl(void)
{
    return 0;
}
