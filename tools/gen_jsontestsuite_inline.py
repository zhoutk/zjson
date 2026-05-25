#!/usr/bin/env python3
"""
Generate tests/test_jsontestsuite_inline.cpp with all JSONTestSuite test_parsing
files embedded as inline C++ string literals.

Usage:
    python3 tools/gen_jsontestsuite_inline.py [test_parsing_dir] [output_cpp]

Defaults:
    test_parsing_dir = refer/JSONTestSuite/test_parsing
    output_cpp       = tests/test_jsontestsuite_inline.cpp
"""

import os
import sys
import subprocess
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def get_default_args():
    test_dir = os.path.join(REPO_ROOT, "refer", "JSONTestSuite", "test_parsing")
    out_cpp  = os.path.join(REPO_ROOT, "tests", "test_jsontestsuite.cpp")
    return test_dir, out_cpp


# Extra hand-written behavioral tests to append after the auto-generated corpus
EXTRA_TESTS = r"""
// ============================================================
// Extension mode: comments must be accepted (not strict RFC)
// ============================================================
TEST(JSONTestSuite, extension_mode_comments_accepted) {
    std::string err;

    // ParseJson (extension mode) must accept comments
    auto ext_accept = [&](const std::string& s) {
        Json j = Json::ParseJson(s, err);
        return !j.isError();
    };

    EXPECT_TRUE(ext_accept("{\"a\":/*comment*/\"b\"}"));
    EXPECT_TRUE(ext_accept("{\"a\":\"b\"}/**/"));
    EXPECT_TRUE(ext_accept("[1]//tail\n"));
    EXPECT_TRUE(ext_accept("/**/42"));

    // Strict mode must reject comments
    EXPECT_FALSE(zjson_accepts("{\"a\":/*comment*/\"b\"}"));
    EXPECT_FALSE(zjson_accepts("{\"a\":\"b\"}/**/"));
    EXPECT_FALSE(zjson_accepts("{\"a\":\"b\"}//"));
}

// ============================================================
// UTF-8 validation mode (ParseJsonStrictUtf8)
// ============================================================
TEST(JSONTestSuite, utf8_validation_rejects_invalid_bytes) {
    std::string err;

    // Valid UTF-8 should pass
    Json j1 = Json::ParseJsonStrictUtf8("[\"abc\"]", err);
    EXPECT_FALSE(j1.isError()) << err;

    Json j2 = Json::ParseJsonStrictUtf8("[\"\xc3\xa9\"]", err);  // U+00E9
    EXPECT_FALSE(j2.isError()) << err;

    Json j3 = Json::ParseJsonStrictUtf8("[\"\xe4\xbd\xa0\xe5\xa5\xbd\"]", err);  // U+4F60 U+597D
    EXPECT_FALSE(j3.isError()) << err;

    Json j4 = Json::ParseJsonStrictUtf8("[\"\xf0\x9d\x84\x9e\"]", err);  // U+1D11E
    EXPECT_FALSE(j4.isError()) << err;

    // Invalid: lone continuation byte
    Json j5 = Json::ParseJsonStrictUtf8(std::string("[\"a\x80\"]"), err);
    EXPECT_TRUE(j5.isError());
    EXPECT_NE(err.find("UTF-8"), std::string::npos);

    // Invalid: overlong 2-byte encoding of ASCII
    Json j6 = Json::ParseJsonStrictUtf8(std::string("[\"a\xc0\xaf\"]"), err);
    EXPECT_TRUE(j6.isError());

    // Invalid: truncated 3-byte sequence
    Json j7 = Json::ParseJsonStrictUtf8(std::string("[\"a\xe0\x80\"]"), err);
    EXPECT_TRUE(j7.isError());

    // Invalid: surrogate half U+D800
    Json j8 = Json::ParseJsonStrictUtf8(std::string("[\"a\xed\xa0\x80\"]"), err);
    EXPECT_TRUE(j8.isError());

    // Invalid: above U+10FFFF  (F4 90 80 80)
    Json j9 = Json::ParseJsonStrictUtf8(std::string("[\"a\xf4\x90\x80\x80\"]"), err);
    EXPECT_TRUE(j9.isError());

    // Invalid: 5-byte sequence (never valid UTF-8)
    Json j10 = Json::ParseJsonStrictUtf8(std::string("[\"a\xf8\x80\x80\x80\x80\"]"), err);
    EXPECT_TRUE(j10.isError());
}

TEST(JSONTestSuite, utf8_validation_accepts_valid_in_keys) {
    std::string err;
    // UTF-8 in key names
    Json j = Json::ParseJsonStrictUtf8("{\"cl\xc3\xa9\":\"val\"}", err);
    EXPECT_FALSE(j.isError()) << err;
}

// ============================================================
// Deep nesting (PDA must handle without stack overflow)
// ============================================================
TEST(JSONTestSuite, deep_nesting_pda_safety) {
    // 100 levels should succeed (max_depth)
    {
        std::string deep;
        for (int i = 0; i < 100; ++i) deep.push_back('[');
        deep += "null";
        for (int i = 0; i < 100; ++i) deep.push_back(']');

        std::string err;
        Json j = Json::ParseJsonStrict(deep, err);
        EXPECT_FALSE(j.isError()) << err;
    }

    // 102 levels should fail (> max_depth)
    {
        std::string deep;
        for (int i = 0; i < 102; ++i) deep.push_back('[');
        deep += "null";
        for (int i = 0; i < 102; ++i) deep.push_back(']');

        std::string err;
        Json j = Json::ParseJsonStrict(deep, err);
        EXPECT_TRUE(j.isError());
    }

    // Even very deep failing nesting must not crash (PDA uses heap stack)
    {
        std::string deep;
        for (int i = 0; i < 10000; ++i) deep.push_back('[');
        std::string err;
        Json j = Json::ParseJsonStrict(deep, err);
        EXPECT_TRUE(j.isError());
    }
}

// ============================================================
// Stress: parse -> copy -> move -> toString -> re-parse in tight loop
// ============================================================
TEST(JSONTestSuite, stress_round_trip) {
    const std::string sample =
        "{\"arr\":[1,2,3],\"obj\":{\"key\":\"value\",\"n\":-0.5e+3},\"t\":true,\"f\":false,\"nil\":null}";

    for (int i = 0; i < 3000; ++i) {
        std::string err;
        Json parsed = Json::ParseJsonStrict(sample, err);
        ASSERT_FALSE(parsed.isError()) << err;

        Json copied = parsed;
        Json moved = std::move(copied);

        std::string out = moved.toString();
        Json reparsed = Json::ParseJsonStrict(out, err);
        ASSERT_FALSE(reparsed.isError()) << err;
        ASSERT_EQ(reparsed["arr"][1].toInt(), 2);
    }
}
"""

# ---------- compile probe ----------

PROBE_SRC = r"""
#include "src/zjson.hpp"
#include <iostream>
#include <string>
#include <fstream>
#include <iterator>
int main(int argc, char* argv[]) {
    if (argc < 2) { std::cerr << "need filename\n"; return 1; }
    std::ifstream f(argv[1], std::ios::binary);
    if (!f) { std::cerr << "open failed\n"; return 2; }
    std::string input((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
    std::string err;
    ZJSON::Json j = ZJSON::Json::ParseJsonStrict(input, err);
    std::cout << (j.isError() ? "rejected" : "accepted") << "\n";
    return 0;
}
"""

def compile_probe():
    src_path = os.path.join(REPO_ROOT, "tools", "_probe_tmp.cpp")
    exe_name = "_probe_tmp.exe" if sys.platform == "win32" else "_probe_tmp"
    exe_path = os.path.join(REPO_ROOT, "tools", exe_name)
    with open(src_path, "w") as f:
        f.write(PROBE_SRC)
    r = subprocess.run(
        ["clang++", "-std=c++17", f"-I{REPO_ROOT}", src_path, "-o", exe_path],
        capture_output=True, text=True, cwd=REPO_ROOT
    )
    if r.returncode != 0:
        sys.exit(f"Probe compile failed:\n{r.stderr}")
    os.unlink(src_path)
    return exe_path

def probe_file(exe, path):
    r = subprocess.run([exe, path], capture_output=True, text=True)
    return r.stdout.strip()  # "accepted" or "rejected"

# ---------- encoding ----------

def bytes_to_cpp_string(data: bytes) -> tuple:
    """Return (cpp_literal_body, byte_count) for arbitrary bytes.
    The body is safe to use as: std::string(body, byte_count)
    """
    parts = []
    i = 0
    while i < len(data):
        b = data[i]
        if b == ord('"'):
            parts.append('\\"')
        elif b == ord('\\'):
            parts.append('\\\\')
        elif b == ord('\n'):
            parts.append('\\n')
        elif b == ord('\r'):
            parts.append('\\r')
        elif b == ord('\t'):
            parts.append('\\t')
        elif 0x20 <= b < 0x7F:
            parts.append(chr(b))
        else:
            parts.append(f'\\x{b:02x}')
            # After a hex escape, if the next char is a valid hex digit,
            # close+reopen the string literal to avoid ambiguity.
            if i + 1 < len(data):
                nxt = data[i + 1]
                if (0x30 <= nxt <= 0x39) or (0x41 <= nxt <= 0x46) or (0x61 <= nxt <= 0x66):
                    parts.append('" "')
        i += 1
    return ''.join(parts), len(data)

# MSVC C2026: single string literal token must be < 16380 chars.
# We split the encoded body into 16000-char chunks and concatenate std::string pieces.
# MSVC C1060 (heap exhaustion): keep TEST() bodies small and compress repeated patterns.
MSVC_LITERAL_MAX = 16000
CASES_PER_TEST   = 25


def bytes_to_small_cpp_expr(raw: bytes) -> str:
    """Return a compact std::string(...) expression for a small byte sequence."""
    if len(raw) == 0:
        return 'std::string("", 0)'
    if len(set(raw)) == 1:
        c = raw[0]
        char_lit = f"'\\x{c:02x}'" if c < 0x20 or c >= 0x7F else f"'{chr(c)}'"
        return f'std::string({len(raw)}, {char_lit})'
    body, size = bytes_to_cpp_string(raw)
    return f'std::string("{body}", {size})'


def detect_repeated_pattern(raw: bytes):
    """Return (pattern, count, tail) for long repeated content, or None.

    We only compress when a short leading pattern repeats enough times to cover
    almost the whole input. This keeps the generated AST small for MSVC.
    """
    if len(raw) < 4096:
        return None

    best = None
    for unit in range(1, 33):
        pattern = raw[:unit]
        count = 0
        pos = 0
        while pos + unit <= len(raw) and raw[pos:pos + unit] == pattern:
            pos += unit
            count += 1

        tail = raw[pos:]
        if count < 16 or len(tail) > 32:
            continue

        score = count * unit
        if best is None or score > best[3]:
            best = (pattern, count, tail, score)

    if best is None:
        return None
    return best[0], best[1], best[2]

def bytes_to_cpp_expr(raw: bytes) -> str:
    """Return a C++ expression of type std::string that equals `raw`."""
    repeated = detect_repeated_pattern(raw)
    if repeated is not None:
        pattern, count, tail = repeated
        pattern_expr = bytes_to_small_cpp_expr(pattern)
        tail_expr = bytes_to_small_cpp_expr(tail)
        return f'repeat_concat({pattern_expr}, {count}, {tail_expr})'

    if len(raw) == 0:
        return 'std::string("", 0)'

    # Fast path: all same byte — use fill constructor
    if len(set(raw)) == 1:
        c = raw[0]
        char_lit = f"'\\x{c:02x}'" if c < 0x20 or c >= 0x7F else f"'{chr(c)}'"
        return f'std::string({len(raw)}, {char_lit})'

    # Encode the raw bytes into an escaped literal body
    body, size = bytes_to_cpp_string(raw)

    if len(body) <= MSVC_LITERAL_MAX:
        return f'std::string("{body}", {size})'

    # Body too long for a single MSVC token: split into chunks and concatenate.
    # Uses std::string(...) + std::string(...) — simpler AST than lambdas.
    pieces = []
    i = 0
    while i < len(raw):
        chunk = raw[i:i + MSVC_LITERAL_MAX]
        cb, cs = bytes_to_cpp_string(chunk)
        pieces.append(f'std::string("{cb}", {cs})')
        i += MSVC_LITERAL_MAX
    return ' +\n        '.join(pieces)


def emit_test_group(lines, suite, test_name, entries_group, expect_true, label, msg_prefix):
    """Emit one TEST() function for `entries_group` (list of (name, raw))."""
    lines.append(f"TEST({suite}, {test_name}) {{")
    for name, raw in entries_group:
        expr = bytes_to_cpp_expr(raw)
        lines.append(f'    // {name}')
        if expect_true:
            lines.append(f'    EXPECT_TRUE(zjson_accepts({expr})) << "{msg_prefix}: {name}";')
        else:
            lines.append(f'    EXPECT_FALSE(zjson_accepts({expr})) << "{msg_prefix}: {name}";')
    lines.append("}")
    lines.append("")


def emit_category(lines, suite, base_name, pairs, expect_true, msg_prefix):
    """Emit all entries split into groups of CASES_PER_TEST, numbered if >1 group."""
    groups = [pairs[i:i+CASES_PER_TEST] for i in range(0, len(pairs), CASES_PER_TEST)]
    for idx, group in enumerate(groups):
        suffix = f"_{idx}" if len(groups) > 1 else ""
        emit_test_group(lines, suite, base_name + suffix, group, expect_true, base_name, msg_prefix)

# ---------- main ----------

def main():
    test_dir = sys.argv[1] if len(sys.argv) > 1 else get_default_args()[0]
    out_cpp  = sys.argv[2] if len(sys.argv) > 2 else get_default_args()[1]

    if not os.path.isdir(test_dir):
        sys.exit(f"test_parsing dir not found: {test_dir}")

    print(f"Compiling probe...", flush=True)
    probe_exe = compile_probe()

    # Collect and probe all files
    files = sorted(
        fn for fn in os.listdir(test_dir)
        if fn.endswith(".json") and len(fn) >= 3 and fn[1] == '_' and fn[0] in ('y','n','i')
    )
    print(f"Found {len(files)} JSON files. Probing...", flush=True)

    entries = []  # (category, filename, raw_bytes, outcome)
    for fn in files:
        path = os.path.join(test_dir, fn)
        with open(path, "rb") as f:
            raw = f.read()
        outcome = probe_file(probe_exe, path)
        cat = fn[0]
        entries.append((cat, fn, raw, outcome))

    y_ok   = sum(1 for c,_,_,o in entries if c=='y' and o=='accepted')
    n_ok   = sum(1 for c,_,_,o in entries if c=='n' and o=='rejected')
    i_acc  = [(fn,raw) for c,fn,raw,o in entries if c=='i' and o=='accepted']
    i_rej  = [(fn,raw) for c,fn,raw,o in entries if c=='i' and o=='rejected']

    print(f"  y_ accept: {y_ok}/{sum(1 for c,*_ in entries if c=='y')}")
    print(f"  n_ reject: {n_ok}/{sum(1 for c,*_ in entries if c=='n')}")
    print(f"  i_ accept: {len(i_acc)}, i_ reject: {len(i_rej)}")

    os.unlink(probe_exe)

    # ---- generate C++ ----
    lines = []
    lines.append("// AUTO-GENERATED (partial) - do not edit y_/n_/i_ sections by hand.")
    lines.append("// Corpus generated by tools/gen_jsontestsuite_inline.py from JSONTestSuite/test_parsing")
    lines.append("// Hand-written behavioral tests are appended below the corpus.")
    lines.append("//")
    lines.append("// JSONTestSuite conformance results baked in:")
    lines.append(f"//   y_ (MUST accept) : {y_ok}/{sum(1 for c,*_ in entries if c=='y')}")
    lines.append(f"//   n_ (MUST reject) : {n_ok}/{sum(1 for c,*_ in entries if c=='n')}")
    lines.append(f"//   i_ accepted      : {len(i_acc)}")
    lines.append(f"//   i_ rejected      : {len(i_rej)}")
    lines.append("")
    lines.append("#include <gtest/gtest.h>")
    lines.append('#include "../src/zjson.hpp"')
    lines.append("#include <string>")
    lines.append("")
    lines.append("using ZJSON::Json;")
    lines.append("")
    lines.append("// Helper: parse with strict RFC mode")
    lines.append("static bool zjson_accepts(const std::string& input) {")
    lines.append("    std::string err;")
    lines.append("    Json j = Json::ParseJsonStrict(input, err);")
    lines.append("    return !j.isError();")
    lines.append("}")
    lines.append("")
    lines.append("static std::string repeat_concat(const std::string& pattern, size_t count,")
    lines.append("                                 const std::string& tail = std::string()) {")
    lines.append("    std::string out;")
    lines.append("    out.reserve(pattern.size() * count + tail.size());")
    lines.append("    for (size_t index = 0; index < count; ++index) out += pattern;")
    lines.append("    out += tail;")
    lines.append("    return out;")
    lines.append("}")
    lines.append("")

    # ---- y_ tests ----
    lines.append("// ============================================================")
    lines.append("// y_: MUST accept - RFC 8259 valid documents")
    lines.append("// ============================================================")
    y_pairs = [(fn[:-5], raw) for c, fn, raw, o in entries if c == 'y']
    emit_category(lines, "JSONTestSuite_y", "MustAccept", y_pairs, True, "Should accept")

    # ---- n_ tests ----
    lines.append("// ============================================================")
    lines.append("// n_: MUST reject - RFC 8259 invalid documents")
    lines.append("// ============================================================")
    n_pairs = [(fn[:-5], raw) for c, fn, raw, o in entries if c == 'n']
    emit_category(lines, "JSONTestSuite_n", "MustReject", n_pairs, False, "Should reject")

    # ---- i_ tests (accepted by ZJSON) ----
    if i_acc:
        lines.append("// ============================================================")
        lines.append("// i_: Implementation-defined - ZJSON currently ACCEPTS these")
        lines.append("// ============================================================")
        emit_category(lines, "JSONTestSuite_i", "CurrentlyAccepted",
                      [(fn[:-5], raw) for fn, raw in i_acc], True, "Regression: used to accept")

    # ---- i_ tests (rejected by ZJSON) ----
    if i_rej:
        lines.append("// ============================================================")
        lines.append("// i_: Implementation-defined - ZJSON currently REJECTS these")
        lines.append("// ============================================================")
        emit_category(lines, "JSONTestSuite_i", "CurrentlyRejected",
                      [(fn[:-5], raw) for fn, raw in i_rej], False, "Regression: used to reject")

    cpp = "\n".join(lines) + "\n"
    cpp += EXTRA_TESTS
    with open(out_cpp, "w", encoding="utf-8") as f:
        f.write(cpp)
    print(f"Written: {out_cpp}  ({os.path.getsize(out_cpp)//1024} KB)")

if __name__ == "__main__":
    main()
