#!/usr/bin/env python3
"""RC mangled-name audit & fix tool for Dark Basic Pro x64.

Scans .rc command string tables for DWORD (K) vs DWORD_PTR (_K) mismatches on
string-carrying parameters and string returns -- a fatal ABI break on x64
(64-bit string handles truncated to 32 bits).

The tool aligns the DBPro parameter format (``%S%`` / ``%L%`` / ...) with the
MSVC decorated signature, so it fixes exactly the tokens that carry strings:
  - every ``%S%`` parameter must be ``_K`` on x64 (string handle);
  - string-returning commands (``$[``) need ``_K`` for both the return token
    and the hidden destination-string parameter;
  - ``%D%`` (32-bit colour values) and other non-string codes stay ``K``.

Entries whose ``%S%`` position is neither ``K`` nor ``_K`` are reported as
warnings instead of being modified.

Usage:
    python tools/rc-audit/rc_audit.py --check   # audit only, exit 1 on issues
    python tools/rc-audit/rc_audit.py --list    # print mismatches and warnings
    python tools/rc-audit/rc_audit.py --fix     # apply known fixes in-place
"""
import argparse
import re
import sys
from pathlib import Path
from typing import List, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parents[2]
SDK_DIR = REPO_ROOT / "Dark Basic Public Shared" / "Dark Basic Pro SDK" / "DarkSDK"
# .rc files of the ported command core (64-bit ABI, DWORD_PTR sources).
# Plugins outside this set still use the legacy 32-bit calling layer and must
# not be touched until their sources are ported too.
BUILT_RC_NAMES = {
    "DBDLLCore.rc", "FTP.rc", "File.rc", "Input.rc",
    "Setup.rc", "Objects.rc", "ObjectsPlus.rc",
    "System.rc", "Text.rc",
}
ENTRY_RE = re.compile(
    r'"([^"\n]*\?([A-Za-z_][A-Za-z0-9_]*)@@Y([A-Z])([A-Z0-9_]+)@Z[^"]*)"'
)
CODES_RE = re.compile(r"%([A-Z0-9]+)%")
# Tokens that are pointer-sized on x64 (two-character MSVC mangling codes).
WIDE_TOKENS = {"_K", "_J", "_N", "_M"}

Mismatch = Tuple[Path, int, str, str, str]  # (rc, line, func, old, new)
Warning = Tuple[Path, int, str, str]  # (rc, line, func, message)


def tokenize(sig: str) -> List[str]:
    """Split an MSVC return+params signature into type tokens.

    Handles the x64 forms seen in the command tables: two-character wide
    tokens (``_K``), pointer tokens (``P`` + optional ``E`` + two-char pointee,
    e.g. ``PEAD`` = char*, ``PEBD`` = const char*), and the ``0`` abbreviation
    meaning "repeat the previous type".
    """
    toks: List[str] = []
    i = 0
    n = len(sig)
    while i < n:
        c = sig[i]
        if c == "_" and i + 1 < n:
            toks.append(sig[i:i + 2])
            i += 2
        elif c == "P" and i + 1 < n:
            j = i + 1
            if sig[j] == "E":
                j += 1
            toks.append(sig[i:min(j + 2, n)])
            i = min(j + 2, n)
        else:
            toks.append(c)
            i += 1
    return toks


def parse_entry(raw: str) -> Optional[Tuple[str, str, str, str, bool, bool, List[str], List[str]]]:
    """Return (func, callconv, ret_tok, ret_code, ret_is_string, hidden_dest,
    param_codes, param_toks)."""
    m = ENTRY_RE.match(raw)
    if not m:
        return None
    raw_text, func, callconv, sig = m.group(1), m.group(2), m.group(3), m.group(4)
    toks = tokenize(sig)
    if not toks:
        return None
    ret_tok, param_toks = toks[0], toks[1:]

    # DBPro format packs parameter codes into one %...% group per entry
    # (e.g. %LSL% is three params), so flatten every code into its letters.
    def codes_of(section: str) -> List[str]:
        return [c for grp in CODES_RE.findall(section) for c in grp]

    fmt = raw_text.split("?", 1)[0]
    if "[" in fmt:
        codes = codes_of(fmt.split("[", 1)[1])
        ret_code = codes[0] if codes else ""
        param_codes = [c for c in codes[1:] if c != "0"]
    else:
        ret_code = ""
        param_codes = [c for c in codes_of(fmt) if c != "0"]

    ret_is_string = "$[" in fmt or "[%S" in fmt
    hidden_dest = ret_is_string and bool(param_toks)
    return (func, callconv, ret_tok, ret_code, ret_is_string, hidden_dest,
            param_codes, param_toks)


def _read_bytes_preserved(path: Path) -> str:
    """Read as latin-1 with newline="": every byte maps 1:1 to a code point
    and CRLF stays intact, so writing back with the same settings is a
    byte-for-byte round-trip (cp1252 0xA9 copyright bytes and CRLF survive)."""
    return path.read_text(encoding="latin-1", newline="")


def audit_rc(rc_path: Path, mismatches: List[Mismatch], warnings: List[Warning]) -> None:
    text = _read_bytes_preserved(rc_path)
    for m in ENTRY_RE.finditer(text):
        parsed = parse_entry(m.group(0))
        if parsed is None:
            continue
        func, callconv, ret_tok, ret_code, ret_is_string, hidden_dest, param_codes, param_toks = parsed
        line = text[: m.start()].count("\n") + 1
        old_sig = m.group(4)

        def name(new_sig: str) -> str:
            return f"?{func}@@Y{callconv}{new_sig}@Z"

        fix_map = {}  # token index -> replacement token

        def make_resolver():
            """Resolve single-digit backref tokens to the type they refer to.

            MSVC emits a digit 'd' for a parameter whose type repeats the type
            of parameter #d (0-based, counting every parameter including
            non-pointer ones). Verified empirically on x64 MSVC 14.5x:
              (u64, char*, char*) -> ?f@@YA_K_KPEAD1@Z   ('1' refers to param 1)
              (u64, u64)          -> ?f@@YA_K_K0@Z       ('0' refers to param 0)
            """
            def resolve(tok: str, depth: int = 0) -> str:
                if depth < 4 and len(tok) == 1 and tok.isdigit():
                    j = int(tok)
                    if j < len(param_toks):
                        return resolve(param_toks[j], depth + 1)
                return tok
            return resolve

        resolve = make_resolver()

        def pointer_sized(tok: str) -> bool:
            """True for tokens that already carry a 64-bit value on x64:
            wide ints, pointers, and backref/repeat digits resolving to those."""
            t = resolve(tok)
            return t == "_K" or t.startswith("P")

        # String return: the return token and the hidden destination param
        # must both be pointer-sized on x64.
        if ret_code == "S":
            if ret_is_string and ret_tok == "K":
                fix_map[0] = "_K"
            if ret_is_string and ret_tok not in ("K", "_K") and not ret_tok.startswith("P"):
                warnings.append((rc_path, line, func,
                                 f"string return but mangled return is {ret_tok!r}"))
            if hidden_dest:
                if param_toks[0] == "K":
                    fix_map[1] = "_K"
                elif not pointer_sized(param_toks[0]):
                    warnings.append((rc_path, line, func,
                                     f"hidden dest param is {param_toks[0]!r}"))

        # %S% parameters: align format codes with mangled param tokens.
        for i, code in enumerate(param_codes):
            if code != "S":
                continue
            idx = i + (1 if hidden_dest else 0)
            if idx >= len(param_toks):
                warnings.append((rc_path, line, func, "%S% param has no mangled counterpart"))
                continue
            tok = param_toks[idx]
            if tok == "K":
                fix_map[idx + 1] = "_K"
            elif not pointer_sized(tok):
                warnings.append((rc_path, line, func,
                                 f"%S% param maps to non-string token {resolve(tok)!r}"))

        if not fix_map:
            continue
        toks = [ret_tok] + param_toks
        for idx, repl in fix_map.items():
            toks[idx] = repl
        new_sig = "".join(toks)
        mismatches.append((rc_path, line, func, name(old_sig), name(new_sig)))


def collect() -> Tuple[List[Mismatch], List[Warning]]:
    mismatches: List[Mismatch] = []
    warnings: List[Warning] = []
    for rc in sorted(SDK_DIR.rglob("*.rc")):
        if rc.name in BUILT_RC_NAMES:
            audit_rc(rc, mismatches, warnings)
    return mismatches, warnings


def apply_fixes(items: List[Mismatch]) -> int:
    fixed = 0
    for rc, line, func, old, new in items:
        text = _read_bytes_preserved(rc)
        n = text.count(old)
        if n == 0:
            print(f"  SKIP: {rc.name}:{line} {func} (name not found)")
            continue
        rc.write_text(text.replace(old, new), encoding="latin-1", newline="")
        print(f"  FIXED {rc.name}:{line} {func} ({n}x) {old} -> {new}")
        fixed += n
    return fixed


def parse_args(argv: Optional[List[str]]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--check", action="store_true")
    group.add_argument("--list", action="store_true")
    group.add_argument("--fix", action="store_true")
    return parser.parse_args(argv)


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv)
    mismatches, warnings = collect()
    if args.list or args.check:
        print(f"Found {len(mismatches)} DWORD_K -> DWORD_PTR mismatches, "
              f"{len(warnings)} warnings.")
        for _, line, func, old, new in mismatches:
            print(f"  {func} at line {line}: {old} -> {new}")
        for _, line, func, msg in warnings:
            print(f"  WARN {func} at line {line}: {msg}")
    if args.fix:
        print(f"Applied {apply_fixes(mismatches)} replacements.")
    return 1 if args.check and mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
