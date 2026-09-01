#!/usr/bin/env python3
# run_codegen_tests.py — isolated, repeatable runner for the DBP codegen suite.
#
# Why isolation? The codegen suite intentionally exercises the real compiler
# emission path in one process (to surface global-state leaks). When a single
# construct crashes the compiler (segfault / stack overflow) the whole gtest
# process dies and the remaining cases never report. This runner launches ONE
# process per test case so a crash in one case cannot mask the others, then
# aggregates everything into a clear, machine-readable + human-readable report.
#
# Two modes:
#   tests   (default) gtest cases, one isolated process per case.
#   corpus  fuzz-corpus mode: runs the standalone dbp_codegen_corpus_runner
#           over every .dba file produced by dbp_codegen_seed_generator, one
#           isolated process per file.
#
# Usage:
#   python3 run_codegen_tests.py [--exe PATH] [--workers N] [--out DIR]
#   python3 run_codegen_tests.py --mode corpus [--corpus DIR] [--runner PATH]
#                                [--seed-gen PATH] [--regenerate]
#
# Output (both modes):
#   <out>/codegen_report.json   structured results (one entry per case)
#   <out>/codegen_report.md     human-readable summary
#   <out>/codegen_report.html   visual summary

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone

DEFAULT_EXE = r"D:\GitHub-repo\FPC Creator Repos\Dark-Basic-Pro\out\build\windows-x64-debug\bin\Debug\dbp_tests.exe"
CRASH_MARKERS = ("Unhandled crash detected", "Exception Code", "[CRITICAL]", "stack overflow",
                "ACCESS VIOLATION", "0xc0000005", "0xc00000fd", "0xC0000005", "0xC00000FD")
GTEST_TIMEOUT = 120  # seconds per case
CORPUS_TIMEOUT = 60  # seconds per file

STATUS_ORDER = ("PASS", "FAIL", "CRASH", "TIMEOUT", "ERROR")


def list_tests(exe):
    """Return a list of fully-qualified gtest case names for the codegen suites."""
    out = subprocess.run([exe, "--gtest_list_tests"], capture_output=True, text=True,
                         timeout=120, env={**os.environ, "HTTP_PROXY": "", "HTTPS_PROXY": "",
                                            "http_proxy": "", "https_proxy": ""})
    raw = out.stdout
    suites = []
    current_suite = None
    for line in raw.splitlines():
        # strip the trailing "  # GetParam() = ..." comment if present
        line = line.split("#")[0].rstrip()
        if not line.strip():
            continue
        indent = len(line) - len(line.lstrip())
        if indent == 0:
            # suite header ends with '.'
            current_suite = line.strip().rstrip(".")
        else:
            test_name = line.strip()
            if current_suite:
                suites.append(f"{current_suite}.{test_name}")
    # Keep only the suites we care about for the codegen report.
    keep = [s for s in suites if any(k in s for k in
            ("Codegen", "CodeGenerationSession", "ASTCodeGen"))]
    return keep


def run_one(exe, name):
    """Run a single test case in its own process; classify the outcome."""
    tmp_json = tempfile.mktemp(suffix=".json")
    env = {**os.environ, "HTTP_PROXY": "", "HTTPS_PROXY": "", "http_proxy": "", "https_proxy": ""}
    cmd = [exe, f"--gtest_filter={name}", f"--gtest_output=json:{tmp_json}"]
    started = time.time()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=GTEST_TIMEOUT, env=env)
        elapsed = time.time() - started
        rc = proc.returncode
        combined = proc.stdout + "\n" + proc.stderr
    except subprocess.TimeoutExpired:
        return {"name": name, "status": "TIMEOUT", "returncode": -1,
                "elapsed": GTEST_TIMEOUT, "summary": "timed out (possible hang)",
                "detail": f"> {GTEST_TIMEOUT}s"}
    except Exception as e:  # pragma: no cover
        return {"name": name, "status": "ERROR", "returncode": -2,
                "elapsed": time.time() - started, "summary": str(e), "detail": ""}

    finished = "[==========] Running" in combined and "[==========]" in combined
    crash = any(m in combined for m in CRASH_MARKERS)

    passed = failed = 0
    m = re.search(r"\[  PASSED  \]\s+(\d+)", combined)
    if m:
        passed = int(m.group(1))
    m = re.search(r"\[  FAILED  \]\s+(\d+)", combined)
    if m:
        failed = int(m.group(1))

    json_verdict = None
    if os.path.exists(tmp_json):
        try:
            with open(tmp_json, "r", encoding="utf-8", errors="replace") as fh:
                data = json.load(fh)
            for suite in data.get("testsuites", []):
                for tc in suite.get("testsuite", []):
                    if tc.get("status") == "RUN":
                        json_verdict = tc.get("result", "UNKNOWN")
                        break
        except Exception:
            json_verdict = None
        try:
            os.remove(tmp_json)
        except OSError:
            pass

    if crash:
        status = "CRASH"
        summary = "compiler crashed during emission"
        detail = ""
        for marker in CRASH_MARKERS:
            idx = combined.find(marker)
            if idx != -1:
                detail = combined[idx:idx + 240].replace("\n", " ")
                break
    elif rc == 0 and failed == 0 and (finished or json_verdict == "COMPLETED"):
        status = "PASS"
        summary = f"passed ({passed} assertion group)"
        detail = ""
    elif rc != 0 and failed > 0:
        status = "FAIL"
        summary = f"{failed} assertion(s) failed"
        for ln in combined.splitlines():
            if "[  FAILED  ]" in ln or "Failure" in ln or "Expected" in ln:
                detail = ln.strip()[:300]
                if detail:
                    break
    elif not finished and rc != 0:
        status = "CRASH"
        summary = "process aborted without gtest summary"
        detail = combined[-300:].replace("\n", " ")
    else:
        status = "FAIL" if failed else ("PASS" if rc == 0 else "ERROR")
        summary = f"returncode={rc}, passed={passed}, failed={failed}"
        detail = ""

    return {"name": name, "status": status, "returncode": rc,
            "elapsed": round(elapsed, 2), "summary": summary, "detail": detail}


def load_corpus_kinds(corpus_dir):
    """Map corpus stem -> manifest kind ('valid' / 'mutant').

    Read up-front so each case is judged against what it was meant to be.
    Without this the report cannot tell "a *valid* program was rejected" (a real
    defect) from "a malformed program was rejected" (the desired outcome) — both
    previously collapsed into a single, meaningless PASS/FAIL count.
    """
    kinds = {}
    manifest = os.path.join(corpus_dir, "manifest.json")
    if not os.path.exists(manifest):
        return kinds
    try:
        with open(manifest, "r", encoding="utf-8", errors="replace") as fh:
            data = json.load(fh)
        for entry in data.get("files", []):
            stem = os.path.splitext(os.path.basename(entry.get("file", "")))[0]
            if stem:
                kinds[stem] = entry.get("kind", "unknown")
    except Exception:
        pass
    return kinds


def classify_corpus(outcome, kind):
    """Verdict for one corpus input, given what happened and what it should be.

    `outcome` is what the runner observed (CLEAN / REJECTED / VIOLATION). The
    same outcome means opposite things depending on the input's kind:

      valid  + CLEAN     -> PASS  (a well-formed program compiled cleanly)
      valid  + REJECTED  -> FAIL  (a well-formed program was refused: a defect)
      valid  + VIOLATION -> FAIL  (broken output contract)
      mutant + REJECTED  -> PASS  (malformed input refused gracefully, no crash)
      mutant + CLEAN     -> PASS  (the mutation happened to be benign)
      mutant + VIOLATION -> FAIL  (broken output contract)
    """
    if outcome == "VIOLATION":
        return "FAIL"
    if kind == "valid":
        return "PASS" if outcome == "CLEAN" else "FAIL"
    return "PASS"


def run_corpus_one(runner_exe, path, kind="unknown"):
    """Run the standalone corpus runner on a single .dba file in isolation."""
    name = os.path.splitext(os.path.basename(path))[0]
    env = {**os.environ, "HTTP_PROXY": "", "HTTPS_PROXY": "", "http_proxy": "", "https_proxy": ""}
    started = time.time()
    try:
        proc = subprocess.run([runner_exe, path], capture_output=True, text=True,
                              timeout=CORPUS_TIMEOUT, env=env)
        elapsed = time.time() - started
        rc = proc.returncode
        out = (proc.stdout or "").strip()
    except subprocess.TimeoutExpired:
        return {"name": name, "status": "TIMEOUT", "returncode": -1, "kind": kind,
                "outcome": "UNKNOWN", "elapsed": CORPUS_TIMEOUT,
                "summary": "timed out (possible hang)",
                "detail": f"> {CORPUS_TIMEOUT}s", "file": path}
    except Exception as e:  # pragma: no cover
        return {"name": name, "status": "ERROR", "returncode": -2, "kind": kind,
                "outcome": "UNKNOWN", "elapsed": time.time() - started,
                "summary": str(e), "detail": "", "file": path}

    # The runner prints one JSON line on success; a crash yields no JSON. spdlog
    # trace lines may interleave, so locate the result line rather than assume it
    # is last.
    record = None
    if out:
        for line in out.splitlines():
            line = line.strip()
            if line.startswith("{") and "status" in line:
                try:
                    record = json.loads(line)
                    break
                except Exception:
                    # Fallback: the JSON may be malformed (e.g. an unescaped
                    # backslash in a Windows path). Recover the fields we need by
                    # regex so the case is still classified correctly.
                    m = re.search(r'"outcome"\s*:\s*"(CLEAN|REJECTED|VIOLATION)"', line)
                    s = re.search(r'"status"\s*:\s*"(PASS|FAIL|ERROR)"', line)
                    if m or s:
                        record = {"outcome": m.group(1) if m else "UNKNOWN",
                                  "status": s.group(1) if s else "",
                                  "error": "", "stage": "", "bytes": 0}
                        break

    if record and isinstance(record, dict):
        outcome = record.get("outcome", "UNKNOWN")
        status = classify_corpus(outcome, kind)
        summary = record.get("error") or {
            "CLEAN": "compiled cleanly",
            "REJECTED": "rejected with a diagnostic",
            "VIOLATION": "universal contract violated",
        }.get(outcome, outcome)
        if status == "FAIL" and outcome == "REJECTED" and kind == "valid":
            summary = ("valid program was rejected: "
                       + (record.get("error") or "no diagnostic captured"))
        detail = (f"outcome={outcome} stage={record.get('stage','')} "
                  f"bytes={record.get('bytes',0)}")
    else:
        # The runner's contract is to print exactly one result line whenever it
        # completes. Reaching here therefore means the process died before
        # finishing — i.e. it crashed.
        #
        # NOTE: the return code alone is NOT a reliable crash signal. Without a
        # crash handler Windows reports an exception code (0xC0000005 etc.,
        # surfacing as a POSITIVE int such as 3221225477, so `rc < 0` misses
        # them); WITH the handler installed the handler terminates the process
        # and the code is a plain 1. Absence of the result line is the invariant
        # that holds in both cases.
        status = "CRASH"
        outcome = "UNKNOWN"
        combined = out + "\n" + (proc.stderr or "")
        detail = ""
        for marker in ("Unhandled crash detected", "Exception Code",
                       "0xC0000005", "0xC00000FD"):
            idx = combined.find(marker)
            if idx != -1:
                detail = combined[idx:idx + 200].replace("\n", " ")
                break
        if not detail:
            detail = (out or proc.stderr or "")[-300:]
        summary = f"process died before reporting a result (rc={rc})"

    return {"name": name, "status": status, "returncode": rc, "kind": kind,
            "outcome": outcome, "elapsed": round(elapsed, 2),
            "summary": summary, "detail": detail, "file": path}


def run_tests_mode(args):
    print(f"[*] Listing codegen tests from {args.exe} ...")
    cases = list_tests(args.exe)
    if not cases:
        print("[!] No codegen tests discovered. Check the binary path.", file=sys.stderr)
        sys.exit(2)
    print(f"[*] {len(cases)} codegen test cases discovered.")

    results = []
    done = 0
    t0 = time.time()
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futs = {pool.submit(run_one, args.exe, c): c for c in cases}
        for fut in as_completed(futs):
            r = fut.result()
            results.append(r)
            done += 1
            if done % 25 == 0 or done == len(cases):
                print(f"    [{done}/{len(cases)}] {r['name']} -> {r['status']}")
    return results, round(time.time() - t0, 1)


def run_corpus_mode(args):
    bin_dir = os.path.dirname(os.path.abspath(args.exe))
    runner = args.runner or os.path.join(bin_dir, "dbp_codegen_corpus_runner.exe")
    seed_gen = args.seed_gen or os.path.join(bin_dir, "dbp_codegen_seed_generator.exe")
    corpus_dir = args.corpus or os.path.join(os.path.dirname(os.path.abspath(__file__)), "corpus")
    corpus_dir = os.path.abspath(corpus_dir)

    os.makedirs(corpus_dir, exist_ok=True)
    existing = [f for f in os.listdir(corpus_dir) if f.endswith(".dba")]
    if args.regenerate or not existing:
        if not os.path.exists(seed_gen):
            print(f"[!] Seed generator not found: {seed_gen}", file=sys.stderr)
            sys.exit(2)
        print(f"[*] Generating corpus with {seed_gen} ...")
        subprocess.run([seed_gen, corpus_dir], check=True,
                       env={**os.environ, "HTTP_PROXY": "", "HTTPS_PROXY": "",
                            "http_proxy": "", "https_proxy": ""})
    else:
        print(f"[*] Reusing {len(existing)} existing corpus files in {corpus_dir}.")

    files = sorted(os.path.join(corpus_dir, f)
                   for f in os.listdir(corpus_dir) if f.endswith(".dba"))
    if not files:
        print("[!] Corpus is empty and generation produced no files.", file=sys.stderr)
        sys.exit(2)
    if not os.path.exists(runner):
        print(f"[!] Corpus runner not found: {runner}", file=sys.stderr)
        sys.exit(2)
    print(f"[*] {len(files)} corpus inputs discovered.")

    # Resolve each input's intended kind BEFORE running, so every result carries
    # it and the verdict is meaningful.
    kinds = load_corpus_kinds(corpus_dir)
    if not kinds:
        print("[!] No manifest.json kind data found; verdicts will be "
              "'unknown' (valid/mutant cannot be distinguished).", file=sys.stderr)

    results = []
    done = 0
    t0 = time.time()
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futs = {pool.submit(run_corpus_one, runner, f,
                            kinds.get(os.path.splitext(os.path.basename(f))[0], "unknown")): f
                for f in files}
        for fut in as_completed(futs):
            r = fut.result()
            results.append(r)
            done += 1
            if done % 25 == 0 or done == len(files):
                print(f"    [{done}/{len(files)}] {r['name']} -> {r['status']}")

    # Cross-tabulate kind x status. This is the number that actually matters: a
    # suite can show a healthy overall pass rate while every single *valid*
    # program is being rejected, and only this breakdown reveals it.
    extra = {}
    matrix = {}
    outcomes = {}
    for r in results:
        matrix.setdefault(r.get("kind", "unknown"), {})[r["status"]] = \
            matrix.setdefault(r.get("kind", "unknown"), {}).get(r["status"], 0) + 1
        outcomes[r.get("outcome", "UNKNOWN")] = outcomes.get(r.get("outcome", "UNKNOWN"), 0) + 1
    if matrix:
        extra["corpus_matrix"] = matrix
        extra["corpus_outcomes"] = outcomes
    return results, round(time.time() - t0, 1), extra


def write_report(results, args, wall, extra=None, mode="tests"):
    extra = extra or {}
    counts = {}
    for r in results:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
    total = len(results)

    report = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "binary": args.exe,
        "mode": mode,
        "total_cases": total,
        "elapsed_seconds": wall,
        "counts": counts,
        "cases": sorted(results, key=lambda r: (r["status"] != "PASS", r["name"])),
    }
    if "corpus_matrix" in extra:
        report["corpus_matrix"] = extra["corpus_matrix"]
        report["corpus_outcomes"] = extra["corpus_outcomes"]

    os.makedirs(args.out, exist_ok=True)
    json_path = os.path.join(args.out, "codegen_report.json")
    md_path = os.path.join(args.out, "codegen_report.md")
    html_path = os.path.join(args.out, "codegen_report.html")

    with open(json_path, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2, ensure_ascii=False)

    # Markdown (English)
    with open(md_path, "w", encoding="utf-8") as fh:
        fh.write("# DBP Codegen Test Report\n\n")
        fh.write(f"- **Generated:** {report['generated_utc']}\n")
        fh.write(f"- **Binary:** `{args.exe}`\n")
        fh.write(f"- **Mode:** {mode}\n")
        fh.write(f"- **Total cases:** {total}\n")
        fh.write(f"- **Duration:** {wall}s\n\n")
        if "corpus_matrix" in extra:
            fh.write("## Corpus results by kind\n\n")
            fh.write("This is the breakdown that matters: an overall pass rate can look\n"
                     "healthy while every *valid* program is in fact being rejected.\n\n")
            fh.write("| Kind | " + " | ".join(STATUS_ORDER) + " | Total |\n")
            fh.write("|---|" + "---|" * (len(STATUS_ORDER) + 1) + "\n")
            for k in sorted(extra["corpus_matrix"]):
                row = extra["corpus_matrix"][k]
                cells = " | ".join(str(row.get(st, 0)) for st in STATUS_ORDER)
                fh.write(f"| {k} | {cells} | {sum(row.values())} |\n")
            fh.write("\n")
            fh.write("### Observed outcomes\n\n")
            fh.write("| Outcome | Count |\n|---|---|\n")
            for k, v in sorted(extra.get("corpus_outcomes", {}).items()):
                fh.write(f"| {k} | {v} |\n")
            fh.write("\n")
        fh.write("## Summary\n\n")
        fh.write("| Status | Count |\n|---|---|\n")
        for st in STATUS_ORDER:
            if counts.get(st):
                fh.write(f"| {st} | {counts[st]} |\n")
        fh.write("\n## Non-passing cases\n\n")
        for r in report["cases"]:
            if r["status"] != "PASS":
                fh.write(f"### `{r['name']}` — **{r['status']}**\n")
                fh.write(f"- {r['summary']}\n")
                if r.get("detail"):
                    fh.write(f"- Details: `{r['detail']}`\n")
                fh.write("\n")

    # HTML (English)
    html_rows = ""
    for r in report["cases"]:
        color = {"PASS": "#1a7f37", "FAIL": "#bc4c00", "CRASH": "#cf222e",
                 "TIMEOUT": "#9a6700", "ERROR": "#8250df"}.get(r["status"], "#666")
        detail = (r.get("detail") or "").replace("<", "&lt;").replace(">", "&gt;")
        kind = r.get("kind", "")
        html_rows += (f"<tr><td>{r['name']}</td><td>{kind}</td>"
                      f"<td style='color:{color};font-weight:700'>{r['status']}</td>"
                      f"<td>{r['summary']}</td><td>{detail}</td></tr>\n")

    # Cross-tab (kind x status) — the view that exposes "valid programs failing"
    # behind an otherwise plausible overall pass rate.
    matrix_html = ""
    if "corpus_matrix" in extra:
        matrix_html = ("<h2>Corpus results by kind</h2>"
                       "<table><tr><th>Kind</th>"
                       + "".join(f"<th>{st}</th>" for st in STATUS_ORDER)
                       + "<th>Total</th></tr>")
        for k in sorted(extra["corpus_matrix"]):
            row = extra["corpus_matrix"][k]
            matrix_html += (f"<tr><td>{k}</td>"
                            + "".join(f"<td>{row.get(st, 0)}</td>" for st in STATUS_ORDER)
                            + f"<td><b>{sum(row.values())}</b></td></tr>")
        matrix_html += "</table>"
    html = f"""<!doctype html><html lang="en" dir="ltr"><head><meta charset="utf-8">
<title>Codegen Test Report</title>
<style>body{{font-family:Segoe UI,Tahoma,sans-serif;margin:2rem;background:#0d1117;color:#e6edf3}}
h1{{color:#58a6ff}}table{{border-collapse:collapse;width:100%;font-size:13px}}
th,td{{border:1px solid #30363d;padding:6px 10px;text-align:left}}
th{{background:#161b22}}.sum{{display:flex;gap:1rem;margin:1rem 0}}
.card{{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:1rem 1.5rem}}
.big{{font-size:2rem;font-weight:800}}</style></head><body>
<h1>Codegen Test Report (DBP) — {mode}</h1>
<div class="sum">
<div class="card"><div class="big" style="color:#1a7f37">{counts.get('PASS',0)}</div>Passed</div>
<div class="card"><div class="big" style="color:#bc4c00">{counts.get('FAIL',0)}</div>Failed</div>
<div class="card"><div class="big" style="color:#cf222e">{counts.get('CRASH',0)}</div>Crashes</div>
<div class="card"><div class="big">{total}</div>Total</div>
</div>
<p>Generated: {report['generated_utc']} — Duration: {wall}s — Mode: {mode}</p>
{matrix_html}
<table><tr><th>Test Case</th><th>Kind</th><th>Status</th><th>Summary</th><th>Details</th></tr>
{html_rows}</table></body></html>"""
    with open(html_path, "w", encoding="utf-8") as fh:
        fh.write(html)

    print("\n================ CODEGEN TEST REPORT ================")
    print(f"  Mode        : {mode}")
    print(f"  Total cases : {total}")
    for st in STATUS_ORDER:
        if counts.get(st):
            print(f"  {st:8s} : {counts[st]}")
    if "corpus_matrix" in extra:
        print("  --- corpus results by kind ---")
        for k in sorted(extra["corpus_matrix"]):
            row = extra["corpus_matrix"][k]
            cells = "  ".join(f"{st}={row.get(st, 0)}" for st in STATUS_ORDER if row.get(st))
            print(f"  {k:10s}: {cells}  (total {sum(row.values())})")
        obs = extra.get("corpus_outcomes", {})
        if obs:
            print("  outcomes  : " + ", ".join(f"{k}={v}" for k, v in sorted(obs.items())))
    print(f"  Wall time   : {wall}s")
    print(f"  JSON report : {json_path}")
    print(f"  Markdown    : {md_path}")
    print(f"  HTML report : {html_path}")
    print("=====================================================")

    sys.exit(0 if counts.get("FAIL", 0) == 0 and counts.get("CRASH", 0) == 0
             and counts.get("ERROR", 0) == 0 else 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["tests", "corpus"], default="tests")
    ap.add_argument("--exe", default=DEFAULT_EXE)
    ap.add_argument("--workers", type=int, default=8)
    ap.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    # corpus-mode only
    ap.add_argument("--corpus", default=None)
    ap.add_argument("--runner", default=None)
    ap.add_argument("--seed-gen", default=None)
    ap.add_argument("--regenerate", action="store_true")
    args = ap.parse_args()

    if args.mode == "corpus":
        results, wall, extra = run_corpus_mode(args)
        write_report(results, args, wall, extra, mode="corpus")
    else:
        results, wall = run_tests_mode(args)
        write_report(results, args, wall, mode="tests")


if __name__ == "__main__":
    main()
