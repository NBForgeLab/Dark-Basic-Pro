#!/usr/bin/env python3
"""Verify every decorated command name in the audited .rc files exists as an
actual export of the plugin DLL that embeds it (x64 ABI consistency check).

Usage: python tools/rc-audit/verify_rc_exports.py
Exit code 1 when any .rc name is missing from its DLL's exports.
"""
import re
import sys
from pathlib import Path

import pefile

REPO_ROOT = Path(__file__).resolve().parents[2]
SDK_DIR = REPO_ROOT / "Dark Basic Public Shared" / "Dark Basic Pro SDK" / "DarkSDK"
BIN_DIR = REPO_ROOT / "build_x64" / "bin" / "Debug" / "plugins"

# .rc file -> plugin DLL whose exports must contain the decorated names.
RC_TO_DLL = {
    "DBDLLCore.rc": "DBProCore.dll",
    "FTP.rc": "DBProFTPDebug.dll",
    "File.rc": "DBProFileDebug.dll",
    "Input.rc": "DBProInputDebug.dll",
    "Setup.rc": "DBProSetupDebug.dll",
    "Objects.rc": "DBProBasic3DDebug.dll",
    "ObjectsPlus.rc": "DBProBasic3DDebug.dll",
    "System.rc": "DBProSystemDebug.dll",
    "Text.rc": "DBProTextDebug.dll",
}

NAME_RE = re.compile(r"\?[A-Za-z_][A-Za-z0-9_]*@@Y[A-Z0-9_]+@Z")


def rc_names(rc: Path) -> set:
    text = rc.read_text(encoding="utf-8", errors="replace")
    return set(NAME_RE.findall(text))


def dll_exports(dll: Path) -> set:
    pe = pefile.PE(dll, fast_load=True)
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
    missing_total = 0
    for rc_name, dll_name in RC_TO_DLL.items():
        rc = SDK_DIR / next(p for p in SDK_DIR.rglob("*.rc") if p.name == rc_name)
        dll = BIN_DIR / dll_name
        names = rc_names(rc)
        if not dll.exists():
            print(f"### {rc_name}: DLL missing ({dll}) - skipping export check")
            continue
        exports = dll_exports(dll)
        missing = sorted(names - exports)
        print(f"### {rc_name} ({len(names)} names) vs {dll_name} "
              f"({len(exports)} exports): {len(missing)} missing")
        for m in missing:
            print(f"    MISSING {m}")
        missing_total += len(missing)
    print(f"\nTOTAL missing .rc names: {missing_total}")
    return 1 if missing_total else 0


if __name__ == "__main__":
    sys.exit(main())
