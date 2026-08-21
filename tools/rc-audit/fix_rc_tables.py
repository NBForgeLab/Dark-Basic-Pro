#!/usr/bin/env python3
"""Apply verified x64 name-mangling fixes to DarkSDK command tables.

Each entry is (rc_file_relative_to_repo_root, old_name, new_name).
The new names are derived from the CURRENT source signatures via
check_rc_sources.py's mangler (which was verified empirically against
cl.exe/dumpbin on real x64 compiles).

I/O is byte-preserving: files are read/written in binary mode so the
cp1252 "Copyright (c)" byte (0xA9) and CRLF line endings are untouched.
Every replacement is count-checked (must occur exactly once).
"""
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RC = REPO / "Dark Basic Public Shared" / "Dark Basic Pro SDK" / "DarkSDK"

# (relative_rc_path, old_name, new_name)
FIXES = [
    # ---- Core: 28 entries still carrying 32-bit DWORD (K) manglings ----
    ("Core/DBDLLCore.rc", "?InputS@@YAKK@Z", "?InputS@@YA_K_K@Z"),
    ("Core/DBDLLCore.rc", "?RestoreD@@YAXK@Z", "?RestoreD@@YAX_K@Z"),
    ("Core/DBDLLCore.rc", "?ReadS@@YAKK@Z", "?ReadS@@YA_K_K@Z"),
    ("Core/DBDLLCore.rc", "?ArrayIndexToBottom@@YAXK@Z", "?ArrayIndexToBottom@@YAX_K@Z"),
    ("Core/DBDLLCore.rc", "?ArrayIndexToTop@@YAXK@Z", "?ArrayIndexToTop@@YAX_K@Z"),
    ("Core/DBDLLCore.rc", "?ArrayIndexValid@@YAKK@Z", "?ArrayIndexValid@@YAK_K@Z"),
    ("Core/DBDLLCore.rc", "?NextArrayIndex@@YAXK@Z", "?NextArrayIndex@@YAX_K@Z"),
    ("Core/DBDLLCore.rc", "?PreviousArrayIndex@@YAXK@Z", "?PreviousArrayIndex@@YAX_K@Z"),
    ("Core/DBDLLCore.rc", "?ArrayCount@@YAKK@Z", "?ArrayCount@@YAK_K@Z"),
    ("Core/DBDLLCore.rc", "?ArrayInsertAtBottom@@YAKK@Z", "?ArrayInsertAtBottom@@YA_K_K@Z"),
    ("Core/DBDLLCore.rc", "?ArrayInsertAtBottom@@YAKKH@Z", "?ArrayInsertAtBottom@@YA_K_KH@Z"),
    ("Core/DBDLLCore.rc", "?ArrayInsertAtTop@@YAKK@Z", "?ArrayInsertAtTop@@YA_K_K@Z"),
    ("Core/DBDLLCore.rc", "?ArrayInsertAtTop@@YAKKH@Z", "?ArrayInsertAtTop@@YA_K_KH@Z"),
    ("Core/DBDLLCore.rc", "?ArrayInsertAtElement@@YAKKH@Z", "?ArrayInsertAtElement@@YA_K_KH@Z"),
    ("Core/DBDLLCore.rc", "?ArrayDeleteElement@@YAXK@Z", "?ArrayDeleteElement@@YAX_K@Z"),
    ("Core/DBDLLCore.rc", "?ArrayDeleteElement@@YAXKH@Z", "?ArrayDeleteElement@@YAX_KH@Z"),
    ("Core/DBDLLCore.rc", "?EmptyArray@@YAXK@Z", "?EmptyArray@@YAX_K@Z"),
    ("Core/DBDLLCore.rc", "?AddToQueue@@YAKK@Z", "?AddToQueue@@YA_K_K@Z"),
    ("Core/DBDLLCore.rc", "?RemoveFromQueue@@YAXK@Z", "?RemoveFromQueue@@YAX_K@Z"),
    ("Core/DBDLLCore.rc", "?PushToStack@@YAKK@Z", "?PushToStack@@YA_K_K@Z"),
    ("Core/DBDLLCore.rc", "?PopFromStack@@YAXK@Z", "?PopFromStack@@YAX_K@Z"),
    ("Core/DBDLLCore.rc", "?ArrayIndexToQueue@@YAXK@Z", "?ArrayIndexToQueue@@YAX_K@Z"),
    ("Core/DBDLLCore.rc", "?ArrayIndexToStack@@YAXK@Z", "?ArrayIndexToStack@@YAX_K@Z"),
    ("Core/DBDLLCore.rc", "?FillByteMemory@@YAXKHH@Z", "?FillByteMemory@@YAX_KHH@Z"),
    ("Core/DBDLLCore.rc", "?CopyByteMemory@@YAXKKH@Z", "?CopyByteMemory@@YAX_K0H@Z"),
    ("Core/DBDLLCore.rc", "?MakeByteMemory@@YAKH@Z", "?MakeByteMemory@@YA_KH@Z"),
    ("Core/DBDLLCore.rc", "?DeleteByteMemory@@YAXK@Z", "?DeleteByteMemory@@YAX_K@Z"),
    ("Core/DBDLLCore.rc", "?GetArrayType@@YAKK@Z", "?GetArrayType@@YAK_K@Z"),
    # ---- FTP: repeated DWORD_PTR must use the '0' abbreviation ----
    ("FTP/FTP.rc", "?Connect@@YAX_K_K_K@Z", "?Connect@@YAX_K00@Z"),
    ("FTP/FTP.rc", "?GetFile@@YAX_K_K@Z", "?GetFile@@YAX_K0@Z"),
    ("FTP/FTP.rc", "?GetFile@@YAX_K_KH@Z", "?GetFile@@YAX_K0H@Z"),
    ("FTP/FTP.rc", "?ConnectEx@@YAX_K_K_KH@Z", "?ConnectEx@@YAX_K00H@Z"),
    ("FTP/FTP.rc", "?HTTPRequestData@@YA_K_K_K_K_K@Z", "?HTTPRequestData@@YA_K_K000@Z"),
    ("FTP/FTP.rc", "?HTTPRequestData@@YA_K_K_K_K_KK@Z", "?HTTPRequestData@@YA_K_K000K@Z"),
    # FTP: table said DeleteFileA but source exports DeleteFile
    ("FTP/FTP.rc", "?DeleteFileA@@YAX_K@Z", "?DeleteFile@@YAX_K@Z"),
    # ---- System ----
    ("System/System.rc", "?ExitPrompt@@YAX_K_K@Z", "?ExitPrompt@@YAX_K0@Z"),
    ("System/System.rc", "?CallDLLX@@YA_KH_KKK@Z", "?CallDLLX@@YA_KH_K0K@Z"),
    # ---- Objects ----
    ("Objects/Objects.rc", "?Convert3DStoX@@YAX_K_K@Z", "?Convert3DStoX@@YAX_K0@Z"),
    ("Objects/Objects.rc", "?CreateNodeTree@@YAXH@Z", "?CreateNodeTree@@YAXMMM@Z"),
    # ---- ObjectsPlus ----
    ("Objects/ObjectsPlus.rc", "?Convert3DStoX@@YAX_K_K@Z", "?Convert3DStoX@@YAX_K0@Z"),
]


def restore_copyright():
    """Restore cp1252 (c) byte 0xA9 where a UTF-8 round-trip replaced it
    with the U+FFFD replacement sequence (EF BF BD)."""
    bad = b"\xef\xbf\xbd"
    good = b"\xa9"
    n_files = n_fixes = 0
    for rc in RC.rglob("*.rc"):
        data = rc.read_bytes()
        if bad not in data:
            continue
        n = data.count(bad)
        data = data.replace(bad, good)
        rc.write_bytes(data)
        n_files += 1
        n_fixes += n
        print("  ok  %-24s restored %d x 0xA9" % (str(rc.relative_to(RC)), n))
    if n_fixes:
        print("restored %d copyright bytes across %d files" % (n_fixes, n_files))
    else:
        print("copyright bytes already clean")


def main():
    by_file = {}
    for rel, old, new in FIXES:
        if old == new:
            raise SystemExit("no-op fix: %s %s" % (rel, old))
        by_file.setdefault(rel, []).append((old, new))

    total = 0
    for rel, pairs in by_file.items():
        path = RC / rel
        data = path.read_bytes()
        for old, new in pairs:
            n = data.count(old.encode("ascii"))
            if n == 0:
                continue  # already fixed (idempotent)
            if n > 1:
                raise SystemExit(
                    "ABORT %s: %r occurs %d times (expected 1)" % (rel, old, n)
                )
            data = data.replace(old.encode("ascii"), new.encode("ascii"))
            total += 1
            print("  ok  %-24s %s -> %s" % (rel, old, new))
        path.write_bytes(data)
    print("applied %d fixes across %d files" % (total, len(by_file)))


if __name__ == "__main__":
    main()
    restore_copyright()
