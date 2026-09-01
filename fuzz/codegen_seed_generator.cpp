// fuzz/codegen_seed_generator.cpp — grammar-driven DarkBASIC corpus generator.
//
// Produces a reproducible corpus of .dba source files (well-formed programs
// plus deliberately malformed mutants) that exercise the compiler and the x64
// code generator. The corpus feeds two consumers:
//
//   * the MSVC standalone runner (tests/codegen/CodegenCorpusRunner.cpp) via
//     run_codegen_tests.py --mode corpus, and
//   * the libFuzzer target dbp_codegen_fuzzer (this directory, Clang build).
//
// Reproducibility: a fixed PRNG seed means re-running yields byte-identical
// files, so CI diffs stay stable and a failing input can be replayed exactly.
//
// Usage: dbp_codegen_seed_generator <corpus-directory>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t kSeed = 0xDBA0C0DEu;
constexpr int kValidCount = 90;
constexpr int kMutantCount = 90;

std::mt19937& Rng() {
    static std::mt19937 rng(kSeed);
    return rng;
}

int RandInt(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(Rng());
}

const std::vector<std::string>& Vars() {
    static const std::vector<std::string> v = {
        "a", "b", "c", "i", "j", "k", "n", "x", "y", "z"};
    return v;
}

std::string Pick(const std::vector<std::string>& v) {
    return v[RandInt(0, static_cast<int>(v.size()) - 1)];
}

// A numeric expression only (integer or float). String variables ($) must never
// appear in arithmetic, so we never append a '$' suffix here.
std::string Expr(int depth) {
    if (depth <= 0) {
        std::string v = Pick(Vars());
        if (RandInt(0, 1)) v += "#";   // optional float suffix, never a string
        return v;
    }
    switch (RandInt(0, 5)) {
        case 0: return Expr(depth - 1) + " + " + Expr(depth - 1);
        case 1: return Expr(depth - 1) + " - " + Expr(depth - 1);
        case 2: return Expr(depth - 1) + " * " + Expr(depth - 1);
        case 3: return Expr(depth - 1) + " / " + Expr(depth - 1);
        case 4: return "(" + Expr(depth - 1) + " > " + Expr(depth - 1) + ")";
        default: return std::to_string(RandInt(0, 100));
    }
}

// A boolean condition for if/while. Built from numeric comparisons joined by the
// logical operators and/or so generated programs stay grammatically valid.
std::string BoolExpr(int depth) {
    if (depth <= 0) {
        const char* ops[] = {"==", "<>", ">", "<", ">=", "<="};
        return Expr(0) + " " + std::string(ops[RandInt(0, 5)]) + " " + Expr(0);
    }
    switch (RandInt(0, 2)) {
        case 0: {
            const char* op = RandInt(0, 2) == 0 ? "==" : (RandInt(0, 1) ? ">" : "<");
            return Expr(depth - 1) + " " + std::string(op) + " " + Expr(depth - 1);
        }
        case 1: return "(" + BoolExpr(depth - 1) + ") and (" + BoolExpr(depth - 1) + ")";
        default: return "(" + BoolExpr(depth - 1) + ") or (" + BoolExpr(depth - 1) + ")";
    }
}

// A single well-formed statement. `indent` is the nesting level; `inLoop` is true
// when this statement sits inside a for/while body, which is the only place an
// `exit` is meaningful. The caller wraps the result in a full program with a
// trailing terminator.
std::string Statement(int indent, int depth, bool inLoop) {
    std::string pad(static_cast<size_t>(indent) * 2, ' ');
    // Outside a loop, never emit a bare `exit` (it would terminate the program
    // prematurely / leave dead code). Restrict the choice to non-exit cases.
    int choice = inLoop ? RandInt(0, 9) : RandInt(0, 8);
    switch (choice) {
        case 0: return pad + Pick(Vars()) + " = " + Expr(depth) + "\r\n";
        case 1: return pad + Pick(Vars()) + "$ = \"" + std::to_string(RandInt(0, 999)) + "\"\r\n";
        case 2: {
            std::string s = pad + "if " + BoolExpr(depth) + "\r\n";
            s += Statement(indent + 1, depth - 1, false);
            s += pad + "else\r\n";
            s += Statement(indent + 1, depth - 1, false);
            s += pad + "endif\r\n";
            return s;
        }
        case 3: {
            std::string v = Pick(Vars());
            std::string s = pad + "for " + v + " = 1 to " + std::to_string(RandInt(2, 20)) + "\r\n";
            s += Statement(indent + 1, depth - 1, true);
            s += pad + "next " + v + "\r\n";
            return s;
        }
        case 4: {
            std::string s = pad + "while " + BoolExpr(depth) + "\r\n";
            s += Statement(indent + 1, depth - 1, true);
            s += pad + "endwhile\r\n";
            return s;
        }
        case 5: {
            std::string v = Pick(Vars());
            std::string s = pad + "select " + v + "\r\n";
            int cases = RandInt(1, 3);
            for (int c = 0; c < cases; ++c) {
                s += pad + "  case " + std::to_string(c + 1) + "\r\n";
                s += Statement(indent + 2, depth - 1, false);
            }
            s += pad + "endselect\r\n";
            return s;
        }
        case 6: return pad + "gosub S" + std::to_string(RandInt(0, 3)) + "\r\n";
        case 7: return pad + "r = F(" + Pick(Vars()) + ")\r\n";
        case 8: return pad + "dim " + Pick(Vars()) + "(" + std::to_string(RandInt(2, 20)) + ")\r\n";
        default: return pad + "exit\r\n";  // only reachable when inLoop == true
    }
}

std::string ValidProgram() {
    std::string src;
    int n = RandInt(2, 6);
    for (int i = 0; i < n; ++i)
        src += Statement(0, RandInt(1, 3), false);

    // Always define function F so that any `r = F(...)` reference resolves.
    std::string p = Pick(Vars());
    src += "\r\nfunction F(" + p + ")\r\n  local t\r\n  t = " + p + " * 2\r\nendfunction t\r\n";

    // Always define gosub labels S0..S3 so that any `gosub S0..S3` target exists.
    for (int g = 0; g < 4; ++g) {
        std::string lbl = "S" + std::to_string(g);
        src += "\r\n" + lbl + ":\r\n  " + Pick(Vars()) + " = 1\r\nreturn\r\n";
    }

    // A single array declaration with an element assignment, exercising arrays
    // without risking a scalar/array name clash.
    src += "\r\ndim arr(" + std::to_string(RandInt(2, 20)) + ")\r\narr(0) = 1\r\n";

    src += "end\r\n";
    return src;
}

enum class MutKind {
    Truncate,
    DropTerminator,
    InjectGarbage,
    DuplicateLine,
    HugeToken,
    NulByte,
    Unicode,
    DeepIfNesting,
    MismatchNext,
    UnterminatedString,
    NumericOverflow,
    EmptyProgram,
    StrayEnd,
};

std::string Mutate(const std::string& prog, MutKind kind) {
    switch (kind) {
        case MutKind::Truncate:
            return prog.size() > 8 ? prog.substr(0, prog.size() / 2) : prog;
        case MutKind::DropTerminator: {
            std::string s = prog;
            for (const char* kw : {"endif", "next", "endwhile", "endselect",
                                   "endfunction", "endtype"}) {
                auto pos = s.find(kw);
                if (pos != std::string::npos) {
                    s.erase(pos, std::strlen(kw));
                    break;
                }
            }
            return s;
        }
        case MutKind::InjectGarbage: {
            std::string s = prog;
            auto pos = s.find("\r\n");
            if (pos != std::string::npos)
                s.insert(pos + 2, "$$$ broken @@@ not_a_command\r\n");
            return s;
        }
        case MutKind::DuplicateLine: {
            std::string s = prog;
            auto pos = s.find("\r\n");
            if (pos != std::string::npos)
                s.insert(pos + 2, s.substr(0, pos + 2));
            return s;
        }
        case MutKind::HugeToken: {
            std::string s = prog;
            auto pos = s.find(" = ");
            if (pos != std::string::npos)
                s.insert(pos + 3, std::string(2000, 'Z'));
            return s;
        }
        case MutKind::NulByte: {
            std::string s = prog;
            if (!s.empty()) s.insert(s.size() / 2, 1, '\0');
            return s;
        }
        case MutKind::Unicode: {
            std::string s = prog;
            auto pos = s.find("\r\nend\r\n");
            if (pos != std::string::npos)
                s.insert(pos, "\xE2\x9C\x93 \xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7 ");
            return s;
        }
        case MutKind::DeepIfNesting: {
            std::string src = "a = 1\r\n";
            for (int i = 0; i < 20; ++i)
                src += std::string(static_cast<size_t>((i + 1) * 2), ' ') +
                       "if a > " + std::to_string(i) + "\r\n";
            src += std::string(42, ' ') + "deep = 1\r\n";
            for (int i = 0; i < 20; ++i)
                src += std::string(static_cast<size_t>((i + 1) * 2), ' ') + "endif\r\n";
            return src + "end\r\n";
        }
        case MutKind::MismatchNext:
            return "for i = 1 to 10\r\n  a = i\r\nnext j\r\nend\r\n";
        case MutKind::UnterminatedString:
            return "a$ = \"never closes\r\nend\r\n";
        case MutKind::NumericOverflow:
            return "a = 999999999999999999999999\r\nend\r\n";
        case MutKind::EmptyProgram:
            return "";
        case MutKind::StrayEnd:
            return "end\r\nend\r\nend\r\n";
    }
    return prog;
}

bool WriteFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(out);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << (argc > 0 ? argv[0] : "dbp_codegen_seed_generator")
                  << " <corpus-directory>\n";
        return 2;
    }
    const auto corpus = std::filesystem::path(argv[1]);
    std::error_code ec;
    std::filesystem::create_directories(corpus, ec);
    if (ec) {
        std::cerr << "cannot create corpus directory: " << ec.message() << "\n";
        return 3;
    }

    int written = 0;
    std::vector<std::pair<std::string, std::string>> manifest;

    for (int i = 0; i < kValidCount; ++i) {
        const std::string src = ValidProgram();
        const auto path = corpus / ("case_" + std::to_string(i) + "_valid.dba");
        if (WriteFile(path, src)) {
            ++written;
            manifest.emplace_back(path.filename().string(), "valid");
        }
    }

    const MutKind kinds[] = {
        MutKind::Truncate,      MutKind::DropTerminator, MutKind::InjectGarbage,
        MutKind::DuplicateLine, MutKind::HugeToken,     MutKind::NulByte,
        MutKind::Unicode,       MutKind::DeepIfNesting, MutKind::MismatchNext,
        MutKind::UnterminatedString, MutKind::NumericOverflow,
        MutKind::EmptyProgram,  MutKind::StrayEnd,
    };
    for (int i = 0; i < kMutantCount; ++i) {
        const std::string base = ValidProgram();
        const MutKind kind = kinds[RandInt(0, static_cast<int>(std::size(kinds)) - 1)];
        const std::string src = Mutate(base, kind);
        const auto path = corpus / ("case_" + std::to_string(i) + "_mutant.dba");
        if (WriteFile(path, src)) {
            ++written;
            manifest.emplace_back(path.filename().string(), "mutant");
        }
    }

    {
        std::ofstream mf(corpus / "manifest.json");
        mf << "{\n  \"seed\": " << kSeed << ",\n  \"files\": [\n";
        for (size_t i = 0; i < manifest.size(); ++i) {
            mf << "    {\"file\": \"" << manifest[i].first << "\", \"kind\": \""
               << manifest[i].second << "\"}";
            if (i + 1 < manifest.size()) mf << ",";
            mf << "\n";
        }
        mf << "  ]\n}\n";
    }

    std::cout << "wrote " << written << " corpus files to " << corpus.string() << "\n";
    return 0;
}
