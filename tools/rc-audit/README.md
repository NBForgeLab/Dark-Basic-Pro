# RC Audit Tool

Scans Dark Basic Pro SDK `.rc` string tables for `DWORD (K)` vs `DWORD_PTR (_K)` mangled-name mismatches on string-carrying parameters — a fatal x64 ABI break.

## Usage

```powershell
python tools/rc-audit/rc_audit.py --check   # audit, exit 1 on issues
python tools/rc-audit/rc_audit.py --list    # list mismatches
python tools/rc-audit/rc_audit.py --fix     # apply fixes in-place
```

## Scope

- Audits only `.rc` files used by the CMake build.
- Flags any command entry that carries `%S%` and uses bare `K` instead of `_K`.