#!/usr/bin/env python3
"""Unit tests for tools/rc-audit/rc_audit.py."""
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import rc_audit


def _audit(text: str):
    with tempfile.TemporaryDirectory() as tmp:
        rc = Path(tmp) / "Test.rc"
        rc.write_text(text, encoding="utf-8")
        mismatches, warnings = [], []
        rc_audit.audit_rc(rc, mismatches, warnings)
        return mismatches, warnings


def test_tokenize():
    assert rc_audit.tokenize("XK") == ["X", "K"]
    assert rc_audit.tokenize("_KK") == ["_K", "K"]
    assert rc_audit.tokenize("AX_KH") == ["A", "X", "_K", "H"]
    assert rc_audit.tokenize("PEAD") == ["PEAD"]
    assert rc_audit.tokenize("PAD") == ["PAD"]
    assert rc_audit.tokenize("PEBD") == ["PEBD"]
    assert rc_audit.tokenize("_K_K0H") == ["_K", "_K", "0", "H"]


def test_callconv_not_consumed():
    # Regression: the 'A' of @@YA (__cdecl) must not leak into the signature,
    # otherwise the reconstructed names never match the file contents.
    mm, ww = _audit('STRINGTABLE\nBEGIN\n'
                    '    IDS_1 "FOO[%S%?Foo@@YAKK@Z%x"\n'
                    'END\n')
    assert len(mm) == 1, mm
    _, _, func, old, new = mm[0]
    assert func == "Foo"
    assert old == "?Foo@@YAKK@Z"
    assert new == "?Foo@@YA_K_K@Z"
    assert ww == []


def test_audit_detects_bare_k():
    mm, _ = _audit('STRINGTABLE\nBEGIN\n'
                   '    IDS_1 "SET WINDOW TITLE%S%?SetWindowTitle@@YAXK@Z%x"\n'
                   '    IDS_2 "BAR[%S%?Bar@@YA_K_K@Z%x"\n'
                   'END\n')
    assert len(mm) == 1, mm
    _, _, func, old, new = mm[0]
    assert func == "SetWindowTitle"
    assert old == "?SetWindowTitle@@YAXK@Z"
    assert new == "?SetWindowTitle@@YAX_K@Z"


def test_multi_string_params():
    mm, _ = _audit('STRINGTABLE\nBEGIN\n'
                   '    IDS_1 "FTP CONNECT%SSS%?Connect@@YAXKKK@Z%a, b, c"\n'
                   'END\n')
    assert len(mm) == 1, mm
    assert mm[0][3] == "?Connect@@YAXKKK@Z"
    assert mm[0][4] == "?Connect@@YAX_K_K_K@Z"


def test_single_string_among_ints():
    # Only the token aligned with %S% is promoted; int params stay H.
    mm, _ = _audit('STRINGTABLE\nBEGIN\n'
                   '    IDS_1 "SET EFFECT ON%LSL%?SetEffectOn@@YAXHKH@Z%a"\n'
                   'END\n')
    assert len(mm) == 1, mm
    assert mm[0][4] == "?SetEffectOn@@YAXH_KH@Z"


def test_string_return_with_hidden_dest():
    # $[ entries carry the destination-string handle as the first parameter.
    mm, _ = _audit('STRINGTABLE\nBEGIN\n'
                   '    IDS_1 "CHECKLIST STRING$[%SL%?ChecklistString@@YAKKH@Z%x"\n'
                   'END\n')
    assert len(mm) == 1, mm
    assert mm[0][4] == "?ChecklistString@@YA_K_KH@Z"


def test_value_return_untouched():
    # A value-returning command ([%L%) with no string params is left alone,
    # as is a %D% (32-bit colour value) parameter.
    mm, _ = _audit('STRINGTABLE\nBEGIN\n'
                   '    IDS_1 "GET FTP FILE SIZE[%L%?GetFileSize@@YAHXZ"\n'
                   '    IDS_2 "SET OBJECT DIFFUSE%LD%?SetDiffuseMaterial@@YAXHK@Z%a"\n'
                   'END\n')
    assert mm == []


def test_already_fixed_is_noop():
    mm, _ = _audit('STRINGTABLE\nBEGIN\n'
                   '    IDS_1 "GET CLIPBOARD$[%S%?GetClipboard@@YA_K_K@Z%x"\n'
                   '    IDS_2 "LOAD OBJECT%SL%?Load@@YA_KHH@Z%x"\n'
                   'END\n')
    assert mm == []


def test_pointer_and_abbrev_tokens_do_not_warn():
    # char* (PEAD), const char* (PEBD) and the '0' repeat-abbreviation are
    # all pointer-sized on x64 and must not be flagged.
    mm, ww = _audit('STRINGTABLE\nBEGIN\n'
                    '    IDS_1 "WRITE TO CLIPBOARD%S%?WriteToClipboard@@YAXPEAD@Z%x"\n'
                    '    IDS_2 "LEFT$[%SSL%?Left@@YA_K_K0H@Z%x"\n'
                    '    IDS_3 "ENCRYPT FILE%S%?EncryptDBPro@@YAXPEBD@Z%x"\n'
                    'END\n')
    assert mm == []
    assert ww == []


def test_backref_digit_resolves_to_referenced_param():
    # MSVC emits a digit backref equal to the 0-based position of the first
    # parameter with the same type. For (u64, char*, char*) x64 MSVC emits
    # ?GetRegistryS@@YA_K_KPEAD1@Z -- the trailing '1' refers to param #1
    # (PEAD), so the %S% pair is pointer-sized and must NOT warn.
    mm, ww = _audit('STRINGTABLE\nBEGIN\n'
                    '    IDS_1 "GET REGISTRY$[%SSS%?GetRegistryS@@YA_K_KPEAD1@Z%x"\n'
                    'END\n')
    assert mm == []
    assert ww == []


def test_non_string_token_at_s_position_warns():
    mm, ww = _audit('STRINGTABLE\nBEGIN\n'
                    '    IDS_1 "FOO%S%?Bar@@YAXHH@Z%x"\n'
                    'END\n')
    assert mm == []
    assert len(ww) == 1, ww
    assert "non-string token" in ww[0][3]


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    for fn in fns:
        fn()
        print(f"ok {fn.__name__}")
    print("All tests passed.")
