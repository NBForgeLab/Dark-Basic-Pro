// fuzz/dbp_codegen_fuzzer.cpp — libFuzzer entry point for the code generator.
//
// Mirrors the package-reader fuzzers but targets CompileSnippet(): every fuzz
// input is treated as DarkBASIC source and driven through the real emission
// pipeline. We abort (libFuzzer captures the crash) on any violation of the
// universal codegen contract:
//
//   * an unresolved leap placeholder survived relocation, or
//   * a forward-reference slot runs past the emitted program, or
//   * the machine-code buffer canary was overwritten (write past the cursor).
//
// Crash-inducing inputs are saved by libFuzzer into the corpus directory for
// triage. Built only under Clang with -DDBP_BUILD_FUZZERS=ON (see this
// directory's CMakeLists.txt).

#include <cstdint>
#include <string>
#include <string_view>

#include "codegen/CodegenHarness.h"

using namespace dbp::codegen;

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
    (void)argc;
    (void)argv;
    // Same shared bootstrap the corpus runner and dbp_tests use: headless mode
    // (a blocking dialog would hang the fuzzer), logging to a file rather than
    // the console, crash diagnostics and the compiler globals.
    InitializeForUnattendedUse("dbp_codegen_fuzzer.log");
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view src(reinterpret_cast<const char*>(data), size);
    const Snapshot snap = CompileSnippet(src);

    // Contract oracle: emitted code must be internally consistent. Any breach
    // is a genuine miscompile / memory-unsafety, so trap to let libFuzzer file
    // the input.
    for (const auto& ref : snap.refs) {
        if (ref.slotBytes == 4 && ref.label == "0") {
            __builtin_trap();  // unresolved branch target
        }
        if (static_cast<std::uint64_t>(ref.offset) + ref.slotBytes >
            snap.bytes.size()) {
            __builtin_trap();  // reference slot runs past the program
        }
    }
    for (const auto& b : snap.guardBytes) {
        if (b != 0xC3u) {
            __builtin_trap();  // buffer canary overwritten -> write overrun
        }
    }
    return 0;
}
