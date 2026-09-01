// tests/codegen/CodegenCorpusRunner.cpp — standalone per-input codegen driver.
//
// Driven by run_codegen_tests.py --mode corpus, which launches ONE process per
// corpus file so a compiler crash in one input cannot mask the others. For each
// file it compiles via the in-process harness and enforces the universal codegen
// contract, printing a single JSON line:
//
//   {"file":"...","status":"PASS|FAIL","stage":"...","bytes":N,
//    "hasError":true|false,"error":"..."}
//
// A hard crash (segfault / stack overflow) kills the process before the JSON is
// printed; the Python runner then classifies the case as CRASH from the missing
// or empty output.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "codegen/CodegenHarness.h"
#include "DBPLogger.h"

using namespace dbp::codegen;

// Escape a string for safe embedding inside a JSON string literal. Windows file
// paths contain backslashes ('\') which are invalid JSON escapes; without this,
// json.loads() of the result line fails and the harness misclassifies the case.
static std::string json_string(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// g_hTempWindow / g_igLoader_HWND are defined by dbp_compiler_lib
// (DBPCompiler.cpp). This process never opens a window, and headless mode
// suppresses any dialog the compiler might otherwise raise.

int main(int argc, char** argv) {
    // Single shared bootstrap: headless mode + logger + crash diagnostics +
    // compiler globals. Kept in the harness so every unattended entry point
    // (this runner, dbp_tests, the fuzz target) cannot drift apart.
    InitializeForUnattendedUse("codegen_corpus_runner.log");

    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <file.dba>\n",
                     argc > 0 ? argv[0] : "dbp_codegen_corpus_runner");
        spdlog::shutdown();
        return 2;
    }

    const std::string path = argv[1];
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::printf("{\"file\":\"%s\",\"status\":\"ERROR\",\"error\":\"cannot open input\"}\n",
                    json_string(path).c_str());
        spdlog::shutdown();
        return 2;
    }
    const std::string src((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());

    const Snapshot snap = CompileSnippet(src);

    // The runner reports FACTS, not a verdict. Whether a given outcome is good
    // depends on what the input was supposed to be, which only the harness
    // knows (it holds the corpus manifest):
    //
    //   CLEAN     — compiled and relocated with no diagnostics at all.
    //   REJECTED  — the compiler stopped and *reported* why (the correct,
    //               graceful way to handle a malformed program).
    //   VIOLATION — the universal contract was broken (unresolved branch, slot
    //               past the program, canary overwritten, or a silent failure
    //               where nothing was reported at all).
    //
    // Collapsing REJECTED into "PASS" (as this did before) hid the fact that
    // *valid* programs were being rejected; collapsing it into "FAIL" would
    // equally mislabel malformed inputs that were correctly rejected. The
    // harness decides, per kind.
    const char* outcome = "CLEAN";
    bool ok = true;
    std::string reason;

    const bool reported = snap.hasError || snap.hasParserError;
    const bool stopped = !snap.parsed || !snap.emitted || !snap.relocated;

    if (stopped && !reported) {
        ok = false;
        outcome = "VIOLATION";
        reason = "silent failure at stage " + snap.stage;
    } else if (stopped) {
        outcome = "REJECTED";
    }

    if (ok && !snap.bytes.empty()) {
        for (const auto& ref : snap.refs) {
            if (ref.slotBytes == 4 && ref.label == "0") {
                ok = false;
                outcome = "VIOLATION";
                reason = "unresolved leap placeholder at MCB+" + std::to_string(ref.offset);
                break;
            }
            if (static_cast<std::uint64_t>(ref.offset) + ref.slotBytes >
                snap.bytes.size()) {
                ok = false;
                outcome = "VIOLATION";
                reason = "reference slot past program end at MCB+" + std::to_string(ref.offset);
                break;
            }
        }
    }
    if (ok) {
        for (size_t i = 0; i < snap.guardBytes.size(); ++i) {
            if (snap.guardBytes[i] != 0xC3u) {
                ok = false;
                outcome = "VIOLATION";
                reason = "machine-code buffer canary overwritten at guard+" + std::to_string(i);
                break;
            }
        }
    }
    if (ok && snap.emitted && snap.relocated && !snap.hasError && snap.bytes.empty()) {
        ok = false;
        outcome = "VIOLATION";
        reason = "reported success but emitted no machine code";
    }
    if (ok && reported) {
        outcome = "REJECTED";
    }

    // Flush the logger BEFORE emitting the result line so that, under piped
    // capture, no trailing spdlog trace can land after the JSON and confuse the
    // harness that parses the output.
    spdlog::shutdown();
    std::printf("{\"file\":\"%s\",\"outcome\":\"%s\",\"status\":\"%s\",\"stage\":\"%s\","
                "\"bytes\":%zu,\"hasError\":%s,\"hasParserError\":%s,\"error\":\"%s\"}\n",
                json_string(path).c_str(), outcome, ok ? "PASS" : "FAIL",
                snap.stage.c_str(), snap.bytes.size(),
                snap.hasError ? "true" : "false",
                snap.hasParserError ? "true" : "false",
                json_string(reason).c_str());
    std::fflush(stdout);
    return ok ? 0 : 1;
}
