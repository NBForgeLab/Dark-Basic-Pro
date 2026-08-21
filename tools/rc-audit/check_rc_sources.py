#!/usr/bin/env python3
"""check_rc_sources.py — cross-check .rc STRINGTABLE command names against DARKSDK
source signatures, using a mini MSVC x64 name mangler.

Ground truth for the mangler was verified against cl.exe/dumpbin
(Visual Studio 18 Community, x64):
  f(int,int)            -> ?f@@YAXHH@Z        (fundamentals never abbreviated)
  f(DWORD_PTR,DWORD_PTR) -> ?f@@YAX_K0@Z       (repeated long types -> '0')
  f(char*,int,char*)    -> ?f@@YAXPEADH0@Z     ('0' refers to any earlier occurrence)
  f(float,float)        -> ?f@@YAXMM@Z
  f(double,double)      -> ?f@@YAXNN@Z
  f(DWORD*,DWORD*,DWORD*) -> ?f@@YAXPEAK00@Z
  f(__int64,__int64)    -> ?f@@YAX_J0@Z
  f(bool,bool)          -> ?f@@YAX_N0@Z
  f(HINSTANCE,HINSTANCE)-> ?f@@YAXPEAUHINSTANCE__@@0@Z
  f(D3DXMATRIX*)        -> ?f@@YAXPEAUD3DXMATRIX@@@Z
  DWORD_PTR f()         -> ?f@@YA_KXZ
  LPSTR f()             -> ?f@@YAPEADXZ

Usage:
  python check_rc_sources.py [--layer Core|FTP|Input|Objects|ObjectsPlus|Setup|System]
                             [--mangle "DARKSDK void Foo(int, DWORD_PTR)"]
                             [--no-git] [--verbose]
"""

import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
SDK = REPO_ROOT / "Dark Basic Public Shared" / "Dark Basic Pro SDK"

# ---------------------------------------------------------------------------
# type -> MSVC x64 code
# ---------------------------------------------------------------------------

FUND_SINGLE_CHAR = {"X", "H", "I", "J", "K", "F", "G", "D", "E", "M", "N"}
# long/complex encodings that MSVC abbreviates with '0' when repeated
ABBREV_ELIGIBLE = True  # everything except FUND_SINGLE_CHAR

PRIMITIVE = {
    "void": "X",
    "int": "H",
    "unsigned int": "I",
    "UINT": "I",
    "long": "J",
    "unsigned long": "K",
    "DWORD": "K",
    "ULONG": "K",
    "dbReturnFloat_t": "K",
    "dbReturnInt_t": "H",
    "DWORD_PTR": "_K",
    "ULONG_PTR": "_K",
    "UINT_PTR": "_K",
    "size_t": "_K",
    "LONG_PTR": "_J",
    "LONGLONG": "_J",
    "ULONGLONG": "_K",
    "float": "M",
    "double": "N",
    "bool": "_N",
    "BOOL": "H",
    "char": "D",
    "signed char": "C",
    "unsigned char": "E",
    "BYTE": "E",
    "short": "F",
    "unsigned short": "G",
    "__int64": "_J",
    "long long": "_J",
    "unsigned __int64": "_K",
    "unsigned long long": "_K",
    "wchar_t": "_W",
    "SDK_LPSTR": "_K",
    "SDK_FLOAT": "K",
    "SDK_BOOL": "H",
    "LPSTR": "PEAD",
    "LPCSTR": "PEBD",
    "char*": "PEAD",
    "const char*": "PEBD",
    "void*": "PEAX",
    "LPVOID": "PEAX",
    "const void*": "PEBX",
    "HANDLE": "PEAX",
    "HMODULE": "PEAUHINSTANCE__@@",
}

# handle typedefs -> pointer to their underscore-suffixed struct tag
HANDLE_STRUCTS = {
    "HINSTANCE", "HWND", "HDC", "HGLRC", "HBITMAP", "HPEN", "HBRUSH",
    "HFONT", "HPALETTE", "HMENU", "HICON", "HCURSOR", "HKEY", "HGLOBAL",
    "HACCEL", "HDROP",
}

# pointer typedefs (LPDIRECT3DDEVICE9 etc.)
PTR_TYPEDEF = {
    "LPDIRECT3DDEVICE9": "PEAUIDirect3DDevice9@@",
    "LPDIRECT3D9": "PEAUIDirect3D9@@",
    "LPDIRECT3DTEXTURE9": "PEAUIDirect3DTexture9@@",
    "LPDIRECT3DSURFACE9": "PEAUIDirect3DSurface9@@",
    "LPDIRECT3DVERTEXBUFFER9": "PEAUIDirect3DVertexBuffer9@@",
    "LPDIRECT3DINDEXBUFFER9": "PEAUIDirect3DIndexBuffer9@@",
}


def mangle_type(t):
    """Mangle one type string (no defaults, no identifier) to MSVC x64 code."""
    t = t.strip().strip("()")
    if "<" in t or ">" in t:
        raise ValueError("template type not supported: %r" % t)
    if t in ("void", ""):
        return "X"
    if t in PRIMITIVE:
        return PRIMITIVE[t]
    if t in PTR_TYPEDEF:
        return PTR_TYPEDEF[t]
    if t in HANDLE_STRUCTS:
        return "PEAU%s__@@" % t

    is_ptr = t.endswith("*")
    is_const = False
    base = t
    if is_ptr:
        base = t[:-1].strip()
        if base.startswith("const"):
            is_const = True
            base = base[5:].strip()
        elif base.endswith("const"):
            is_const = True
            base = base[:-5].strip()
        inner = mangle_type(base)
        return ("PEB" if is_const else "PEA") + inner
    # by-value class / struct -> ?AU<name>@@
    if t.startswith("const "):
        t = t[6:].strip()
    # strip any calling-convention remnants
    for kw in ("CALLBACK", "WINAPI", "__cdecl", "__stdcall", "__fastcall", "__vectorcall"):
        t = t.replace(kw, "").strip()
    name = re.sub(r"\s+", "", t)
    if not name or not re.match(r"^[A-Za-z_]\w*$", name):
        raise ValueError("cannot mangle type: %r" % t)
    return "?AU%s@@" % name


def split_params(s):
    """Split a parameter list on top-level commas."""
    parts, depth, cur = [], 0, []
    for ch in s:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(ch)
    if "".join(cur).strip() or parts:
        parts.append("".join(cur))
    return parts


TYPE_WORDS = (
    set(PRIMITIVE) | set(HANDLE_STRUCTS) | set(PTR_TYPEDEF)
    | {"const", "signed", "unsigned", "short", "long", "wchar_t", "__int64",
       "__cdecl", "__stdcall", "__fastcall", "__vectorcall", "CALLBACK",
       "WINAPI", "APIENTRY"}
)


def normalize_param(p):
    """'DWORD_PTR lpStr' -> 'DWORD_PTR'; strip defaults, attributes, identifiers."""
    p = p.strip()
    p = re.sub(r"=\s*[^,]*$", "", p).strip()
    p = re.sub(r"\[\[[^\]]*\]\]", "", p).strip()
    # drop trailing parameter identifier if it is not a type word
    m = re.search(r"([A-Za-z_]\w*)$", p)
    if m and m.group(1) not in TYPE_WORDS:
        p = p[: m.start()].strip()
    # collapse 'char *' -> 'char*'
    p = re.sub(r"\s*\*\s*", "*", p).strip()
    p = re.sub(r"\s+", " ", p)
    return p


def mangle_params(param_types):
    """Apply the '0' abbreviation: repeated non-fundamental codes become '0'."""
    out, seen = [], set()
    for t in param_types:
        code = mangle_type(t)
        if code in seen and code not in FUND_SINGLE_CHAR:
            code = "0"
        seen.add(code)
        out.append(code)
    return "".join(out)


def mangle_decl(name, ret, params):
    # 'A' after '@@Y' = __cdecl calling convention (all DARKSDK exports)
    return "?%s@@YA%s%s@Z" % (name, mangle_type(ret), mangle_params(params))


# ---------------------------------------------------------------------------
# comment / literal aware source scanning
# ---------------------------------------------------------------------------

def strip_comments_and_strings(text):
    """Remove C/C++ comments but keep string-literal boundaries stable so
    things like 'http://' inside strings cannot confuse line handling."""
    out, i, n = [], 0, len(text)
    state = "code"  # code | line | block | str | char
    while i < n:
        c = text[i]
        if state == "code":
            if c == "/" and i + 1 < n and text[i + 1] == "/":
                state = "line"
                i += 2
            elif c == "/" and i + 1 < n and text[i + 1] == "*":
                state = "block"
                i += 2
            elif c == '"':
                state = "str"
                out.append(c)
                i += 1
            elif c == "'":
                state = "char"
                out.append(c)
                i += 1
            else:
                out.append(c)
                i += 1
        elif state == "line":
            if c == "\n":
                state = "code"
                out.append("\n")
            i += 1
        elif state == "block":
            if c == "*" and i + 1 < n and text[i + 1] == "/":
                state = "code"
                i += 2
            else:
                i += 1
        elif state == "str":
            if c == "\\":
                out.append(c)
                out.append(text[i + 1] if i + 1 < n else "")
                i += 2
            elif c == '"':
                state = "code"
                out.append(c)
                i += 1
            else:
                out.append(c)
                i += 1
        elif state == "char":
            if c == "\\":
                out.append(c)
                out.append(text[i + 1] if i + 1 < n else "")
                i += 2
            elif c == "'":
                state = "code"
                out.append(c)
                i += 1
            else:
                out.append(c)
                i += 1
    return "".join(out)


DARKSDK_RE = re.compile(
    r"\bDARKSDK(?:_DLL)?\s+((?:[^;()]|\([^)]*\))+?)\s*\(([^)]*)\)\s*(?:const)?\s*(?:;|\s*\{|\bif\b|$)",
    re.S,
)

RETURN_KW = ("__cdecl", "__stdcall", "__fastcall", "__vectorcall", "CALLBACK", "WINAPI", "APIENTRY")


def extract_exports(paths):
    """Return {name: set(mangled_names)} and conflicts across the given sources."""
    exports = {}
    for path in paths:
        try:
            raw = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        text = strip_comments_and_strings(raw)
        text = re.sub(r"\[\[[^\]]*\]\]", "", text)  # [[maybe_unused]] etc.
        # drop preprocessor lines (DARKSDK defines, #ifdef noise)
        text = "\n".join(
            ln for ln in text.splitlines() if not ln.lstrip().startswith("#")
        )
        for m in DARKSDK_RE.finditer(text):
            head, params = m.group(1), m.group(2)
            head = head.replace("SDK_RETSTR", "DWORD_PTR lpStr,").strip()
            tokens = re.findall(r"[A-Za-z_]\w*(?:\s*<[^>]*>)?\*?", head)
            if len(tokens) < 2:
                continue
            name = tokens[-1]
            ret = " ".join(tokens[:-1])
            # strip const at front of return
            ret = re.sub(r"^\s*const\s+", "", ret)
            # strip SDK_RETSTR remnants
            ret = ret.replace("SDK_RETSTR", "").replace("DWORD_PTR lpStr", "DWORD_PTR")
            for kw in RETURN_KW:
                ret = ret.replace(kw, "")
            ret = ret.strip()
            if not ret:
                continue
            # expand SDK_RETSTR already happened; now split params
            params = params.replace("SDK_RETSTR", "DWORD_PTR lpStr,")
            ptokens = [normalize_param(p) for p in split_params(params)]
            ptokens = [p for p in ptokens if p and p not in ("void", "SDK_RETSTR")]
            try:
                mangled = mangle_decl(name, ret, ptokens)
            except ValueError as e:
                sys.stderr.write("  [skip] %s: %s: %s\n" % (path.name, name, e))
                continue
            exports.setdefault(name, set()).add(mangled)
    return exports


# ---------------------------------------------------------------------------
# .rc STRINGTABLE parsing
# ---------------------------------------------------------------------------

ENTRY_RE = re.compile(r"\?[A-Za-z_][A-Za-z0-9_]*@@Y[A-Z0-9_]+@Z")


def parse_rc(path):
    names = []
    for m in ENTRY_RE.finditer(path.read_text(encoding="latin-1")):
        names.append(m.group(0))
    return names


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

LAYERS = {
    "Core": {
        "rc": SDK / "DarkSDK" / "Core" / "DBDLLCore.rc",
        "dirs": [SDK / "Shared" / "Core"],
    },
    "FTP": {
        "rc": SDK / "DarkSDK" / "FTP" / "FTP.rc",
        "dirs": [SDK / "Shared" / "FTP"],
    },
    "Input": {
        "rc": SDK / "DarkSDK" / "Input" / "Input.rc",
        "dirs": [SDK / "Shared" / "Input"],
    },
    "Objects": {
        "rc": SDK / "DarkSDK" / "Objects" / "Objects.rc",
        "dirs": [SDK / "Shared" / "Objects", SDK / "Shared" / "ConvX"],
    },
    "ObjectsPlus": {
        "rc": SDK / "DarkSDK" / "Objects" / "ObjectsPlus.rc",
        "dirs": [SDK / "Shared" / "Objects", SDK / "Shared" / "ConvX"],
    },
    "Setup": {
        "rc": SDK / "DarkSDK" / "Setup" / "Setup.rc",
        "dirs": [SDK / "Shared" / "Setup"],
    },
    "System": {
        "rc": SDK / "DarkSDK" / "System" / "System.rc",
        "dirs": [SDK / "Shared" / "System"],
    },
}


def git_changed_table_names(rc_path):
    """Names on added lines of the current git diff for this .rc file."""
    try:
        out = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "diff", "--unified=0", "--", str(rc_path)],
            capture_output=True, text=True, encoding="latin-1", errors="replace",
            timeout=30,
        ).stdout
    except (subprocess.SubprocessError, OSError):
        return set()
    changed = set()
    for line in out.splitlines():
        if line.startswith("+"):
            for m in ENTRY_RE.finditer(line):
                changed.add(m.group(0))
    return changed


def check_layer(layer, use_git=True, verbose=False):
    cfg = LAYERS[layer]
    if not cfg["rc"].exists():
        print("  [skip] %s: rc not found %s" % (layer, cfg["rc"]))
        return
    paths = sorted(
        p for d in cfg["dirs"] if d.exists()
        for p in d.rglob("*") if p.suffix in (".cpp", ".c", ".h", ".hpp", ".cxx")
    )
    exports = extract_exports(paths)
    table = parse_rc(cfg["rc"])
    changed = git_changed_table_names(cfg["rc"]) if use_git else set()

    matched = mismatched = table_only = 0
    mismatch_lines, table_only_lines = [], []
    seen = set()
    for name in table:
        if name in seen:
            continue
        seen.add(name)
        funcname = name[1:].split("@@")[0]
        if funcname not in exports:
            # not exported by this layer's sources at all
            table_only += 1
            flag = " <-- CHANGED IN THIS DIFF" if name in changed else ""
            table_only_lines.append("    %s%s" % (name, flag))
            continue
        src_sigs = exports[funcname]
        if name in src_sigs:
            matched += 1
            continue
        mismatched += 1
        mismatch_lines.append(
            "    table  %s%s\n    source %s"
            % (name, " <-- CHANGED IN THIS DIFF" if name in changed else "",
               " | ".join(sorted(src_sigs)))
        )

    source_only_names = [
        funcname for funcname in exports if funcname not in seen
    ]

    print("== %s ==" % layer)
    print("  table entries : %d" % len(table))
    print("  source exports: %d" % len(exports))
    print("  MATCH         : %d" % matched)
    print("  MISMATCH      : %d" % mismatched)
    print("  table-only    : %d" % table_only)
    print("  source-only   : %d" % len(source_only_names))
    for line in mismatch_lines:
        print("  MISMATCH\n" + line)
    for line in table_only_lines:
        print("  TABLE-ONLY\n" + line)
    if verbose:
        for head in source_only_names[:40]:
            print("  SOURCE-ONLY %s" % head)
        if len(source_only_names) > 40:
            print("  ... and %d more" % (len(source_only_names) - 40))
    return mismatched, table_only


def main():
    args = sys.argv[1:]
    only = None
    use_git = True
    verbose = False
    if "--no-git" in args:
        use_git = False
        args.remove("--no-git")
    if "--verbose" in args:
        verbose = True
        args.remove("--verbose")
    if "--mangle" in args:
        i = args.index("--mangle")
        decl = args[i + 1]
        m = DARKSDK_RE.search(strip_comments_and_strings(decl))
        if not m:
            sys.exit("cannot parse declaration: %s" % decl)
        head, params = m.group(1), m.group(2)
        tokens = re.findall(r"[A-Za-z_]\w*", head)
        name, ret = tokens[-1], " ".join(tokens[:-1])
        ptokens = [normalize_param(p) for p in split_params(params.replace("SDK_RETSTR", "DWORD_PTR lpStr,"))]
        ptokens = [p for p in ptokens if p and p != "void"]
        print(mangle_decl(name, ret, ptokens))
        return
    for layer in args:
        if layer in LAYERS:
            only = layer
            break
    layers = [only] if only else list(LAYERS)
    total_m, total_t = 0, 0
    for layer in layers:
        m, t = check_layer(layer, use_git, verbose)
        total_m += m
        total_t += t
    print("\nTOTAL: %d mismatches, %d table-only entries" % (total_m, total_t))


if __name__ == "__main__":
    main()
