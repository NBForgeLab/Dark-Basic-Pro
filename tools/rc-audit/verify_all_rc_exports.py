#!/usr/bin/env python3
"""Verify every decorated command name in ALL DarkSDK .rc string tables exists
as an actual export of the plugin DLL that embeds it (x64 ABI consistency).

Unlike verify_rc_exports.py (curated 9-table map), this derives the owning
plugin automatically: the .rc under DarkSDK/<Name>/ maps to
Shared/<Name>/CMakeLists.txt OUTPUT_NAME, so newly ported plugins are covered
without editing a map.

Usage:
    python tools/rc-audit/verify_all_rc_exports.py
Exit code 1 when any .rc name is missing from its DLL's exports.
"""
import re
import sys
from pathlib import Path

import pefile

REPO_ROOT = Path(__file__).resolve().parents[2]
SDK_DIR = REPO_ROOT / "Dark Basic Public Shared" / "Dark Basic Pro SDK" / "DarkSDK"
SHARED_DIR = REPO_ROOT / "Dark Basic Public Shared" / "Dark Basic Pro SDK" / "Shared"
BIN_DIR = REPO_ROOT / "build_x64" / "bin" / "Debug" / "plugins"

NAME_RE = re.compile(r"\?[A-Za-z_][A-Za-z0-9_]*@@Y[A-Z0-9_]+@Z")

# .rc stem -> Shared/<dir> when the names differ.
ALIAS = {
    "DBDLLCore": "Core",
    "ObjectsPlus": "Objects",
    "FilePlus": "File",
    "AdvancedMatrix": "EnhancedMatrix",
    "Particles": "SpecialEffects",
}
# Tables with no owning CMake target (legacy/vendored/dormant).
SKIP = {"Script1", "SetupDEMO", "Multiplayer"}


def dll_for_rc(rc: Path):
    name = rc.stem
    if name in SKIP:
        return None
    cmake = SHARED_DIR / ALIAS.get(name, name) / "CMakeLists.txt"
    if not cmake.exists():
        return None
    m = re.search(r'OUTPUT_NAME\s+"([^"]+)"', cmake.read_text(errors="replace"))
    return BIN_DIR / (m.group(1) + ".dll") if m else None


def rc_names(rc: Path) -> set:
    text = rc.read_text(encoding="utf-8", errors="replace")
    return set(NAME_RE.findall(text))


def dll_exports(dll: Path) -> set:
    pe = pefile.PE(str(dll), fast_load=True)
    pe.parse_data_directories(
        directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]]
    )
    names = set()
    if hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            if e.name:
                names.add(e.name.decode(errors="replace"))
    return names


def main() -> int:
    total_missing = 0
    for rc in sorted(SDK_DIR.rglob("*.rc")):
        names = rc_names(rc)
        if not names:
            continue
        dll = dll_for_rc(rc)
        if dll is None or not dll.exists():
            print(f"-- SKIP {rc.name}: no built DLL")
            continue
        exp = dll_exports(dll)
        missing = sorted(n for n in names if n not in exp)
        state = "OK " if not missing else "BAD"
        print(f"{state} {rc.name:18} rc={len(names):3} dll={len(exp):3} "
              f"missing={len(missing)}")
        for n in missing:
            print(f"      MISSING {n}")
        total_missing += len(missing)
    print(f"\nTOTAL missing across ALL audited rc: {total_missing}")
    return 1 if total_missing else 0


if __name__ == "__main__":
    sys.exit(main())
