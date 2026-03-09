#!/usr/bin/env bash
# =============================================================================
#  e2e-logoscore.sh — E2E tests: logoscore --call with kv_module
# =============================================================================
#
#  Validates the KV module plugin stack headlessly:
#    logoscore CLI → Qt plugin loader → kv_module
#
#  Tests:
#    1. version()           — basic plugin load
#    2. set()             — store a value
#    3. get()             — retrieve and assert value
#    4. set() second key  — store second value
#    5. list()            — list keys, assert both present
#    6. remove()          — remove first key
#    7. get() after remove — assert empty
#    8. clear()           — clear namespace
#    9. list() after clear — assert empty
#   10. Namespace isolation — key in ns "a" not visible in ns "b"
#
#  Prerequisites:
#    - Nix build:  cd <repo> && nix build
#      Produces:   <repo>/result/{lib/logos/modules/,include/}
#    - logoscore: auto-detected from nix store (lez-multisig build)
#
#  Usage:
#    bash scripts/e2e-logoscore.sh
#
# =============================================================================
set -euo pipefail

export PATH="$HOME/.nix-profile/bin:$PATH"

# ── Paths ────────────────────────────────────────────────────────────────────

# Resolve repo root relative to this script (works regardless of where you clone)
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# logoscore: env var > nix store (any logoscore binary) > repo result/bin
if [[ -z "${LOGOSCORE:-}" ]]; then
    LOGOSCORE="$(find /nix/store -name logoscore -type f 2>/dev/null | head -1 || true)"
fi
LOGOSCORE="${LOGOSCORE:-$REPO_DIR/result/bin/logoscore}"

# modules dir: build a properly structured module dir in /tmp
# logoscore requires: <modules_dir>/<module_name>/kv_module_plugin.so + manifest.json
# where manifest.json has "main" as a platform-keyed object
if [[ -z "${MODULES_DIR:-}" ]]; then
    PLUGIN_SRC="$REPO_DIR/result/lib/kv_module_plugin.so"
    if [[ ! -f "$PLUGIN_SRC" ]]; then
        # flat lib output from logos-module-builder
        PLUGIN_SRC="$(find "$REPO_DIR/result" -name "kv_module_plugin.so" 2>/dev/null | head -1 || true)"
    fi

    TMP_MODULES="/tmp/kv-modules-$$"
    mkdir -p "$TMP_MODULES/kv_module"

    if [[ -n "$PLUGIN_SRC" && -f "$PLUGIN_SRC" ]]; then
        cp "$PLUGIN_SRC" "$TMP_MODULES/kv_module/"
    fi

    cat > "$TMP_MODULES/kv_module/manifest.json" << 'MANIFEST_EOF'
{
  "name": "kv_module",
  "version": "0.1.0",
  "description": "Local key-value storage module for Logos Core with swappable backends",
  "author": "Jimmy Claw",
  "type": "core",
  "category": "storage",
  "main": {
    "linux-x86_64": "kv_module_plugin.so",
    "linux-amd64": "kv_module_plugin.so",
    "linux-arm64": "kv_module_plugin.so",
    "linux-aarch64": "kv_module_plugin.so",
    "darwin-x86_64": "kv_module_plugin.dylib",
    "darwin-arm64": "kv_module_plugin.dylib",
    "windows-x86_64": "kv_module_plugin.dll"
  },
  "dependencies": [],
  "capabilities": []
}
MANIFEST_EOF

    MODULES_DIR="$TMP_MODULES"
    trap 'rm -rf "$TMP_MODULES"' EXIT
fi

# ── Colours ──────────────────────────────────────────────────────────────────

BOLD='\033[1m'; DIM='\033[2m'; RESET='\033[0m'
GREEN='\033[0;32m'; CYAN='\033[0;36m'; YELLOW='\033[1;33m'; RED='\033[0;31m'

# ── Helpers ──────────────────────────────────────────────────────────────────

PASS=0; FAIL=0; SKIP=0

pass() { echo -e "  ${GREEN}PASS${RESET}: $1"; PASS=$((PASS + 1)); }
fail() { echo -e "  ${RED}FAIL${RESET}: $1 — $2"; FAIL=$((FAIL + 1)); }
skip() { echo -e "  ${YELLOW}SKIP${RESET}: $1 — $2"; SKIP=$((SKIP + 1)); }
banner() { echo -e "\n${CYAN}${BOLD}━━━ $1 ━━━${RESET}\n"; }
info()   { echo -e "  ${DIM}$1${RESET}"; }

cleanup_logoscore() {
    pkill -f "logoscore.*modules-dir" 2>/dev/null || true
    pkill -f "logos_host" 2>/dev/null || true
    sleep 0.5
}

# Run a logoscore --call and capture output.
# Usage: logoscore_call <load_modules> <call_expr>
logoscore_call() {
    local load_modules="$1"
    local call_expr="$2"

    cleanup_logoscore

    local output
    output=$(QT_QPA_PLATFORM=offscreen timeout 30 "$LOGOSCORE" \
        --modules-dir "$MODULES_DIR" \
        --load-modules "$load_modules" \
        --call "$call_expr" 2>&1 || true)

    cleanup_logoscore
    echo "$output"
}

# Extract the result value after "Method call successful. Result: "
extract_result() {
    echo "$1" | grep "Method call successful" | sed 's/.*Result: //'
}

# ── Pre-flight ───────────────────────────────────────────────────────────────

banner "Pre-flight Checks"

if [[ ! -x "$LOGOSCORE" ]]; then
    echo -e "${RED}logoscore not found at $LOGOSCORE${RESET}"
    echo "Build with: cd ~/logos-kv-module && nix build"
    exit 1
fi
pass "logoscore binary: $LOGOSCORE"

if [[ ! -d "$MODULES_DIR" ]]; then
    echo -e "${RED}modules directory not found at $MODULES_DIR${RESET}"
    echo "Build with: cd ~/logos-kv-module && nix build"
    exit 1
fi
pass "modules directory: $MODULES_DIR"

# ── Test 1: version() ───────────────────────────────────────────────────────

banner "Test 1: kv_module.version()"

OUTPUT=$(logoscore_call "kv_module" "kv_module.version()")

if echo "$OUTPUT" | grep -q "Method call successful"; then
    RESULT=$(extract_result "$OUTPUT")
    pass "version() = $RESULT"
else
    fail "version()" "unexpected output"
    echo "$OUTPUT" | tail -5 | sed 's/^/      /'
fi

# ── Test 2: set() — store a value ─────────────────────────────────────────

banner "Test 2: kv_module.set(\"test_ns\", \"key1\", \"hello\")"

OUTPUT=$(logoscore_call "kv_module" 'kv_module.set("test_ns", "key1", "hello")')

if echo "$OUTPUT" | grep -q "Method call successful"; then
    pass "set(test_ns, key1, hello)"
else
    fail "set(test_ns, key1, hello)" "unexpected output"
    echo "$OUTPUT" | tail -5 | sed 's/^/      /'
fi

# ── Test 3: get() — retrieve and assert ───────────────────────────────────

banner "Test 3: kv_module.get(\"test_ns\", \"key1\") — expect \"hello\""

OUTPUT=$(logoscore_call "kv_module" 'kv_module.get("test_ns", "key1")')

if echo "$OUTPUT" | grep -q "Method call successful"; then
    RESULT=$(extract_result "$OUTPUT")
    if echo "$RESULT" | grep -q "hello"; then
        pass "get(test_ns, key1) = hello"
    else
        fail "get(test_ns, key1)" "expected 'hello', got: $RESULT"
    fi
else
    fail "get(test_ns, key1)" "unexpected output"
    echo "$OUTPUT" | tail -5 | sed 's/^/      /'
fi

# ── Test 4: set() — store second value ────────────────────────────────────

banner "Test 4: kv_module.set(\"test_ns\", \"key2\", \"world\")"

OUTPUT=$(logoscore_call "kv_module" 'kv_module.set("test_ns", "key2", "world")')

if echo "$OUTPUT" | grep -q "Method call successful"; then
    pass "set(test_ns, key2, world)"
else
    fail "set(test_ns, key2, world)" "unexpected output"
    echo "$OUTPUT" | tail -5 | sed 's/^/      /'
fi

# ── Test 5: list() — list all keys ────────────────────────────────────────

banner "Test 5: kv_module.list(\"test_ns\", \"\") — expect key1 and key2"

OUTPUT=$(logoscore_call "kv_module" 'kv_module.list("test_ns", "")')

if echo "$OUTPUT" | grep -q "Method call successful"; then
    RESULT=$(extract_result "$OUTPUT")
    HAS_KEY1=false; HAS_KEY2=false
    echo "$RESULT" | grep -q "key1" && HAS_KEY1=true
    echo "$RESULT" | grep -q "key2" && HAS_KEY2=true
    if $HAS_KEY1 && $HAS_KEY2; then
        pass "list(test_ns) contains key1 and key2"
    else
        fail "list(test_ns)" "expected key1 and key2, got: $RESULT"
    fi
    info "Result: $RESULT"
else
    fail "list(test_ns)" "unexpected output"
    echo "$OUTPUT" | tail -5 | sed 's/^/      /'
fi

# ── Test 6: remove() — remove first key ───────────────────────────────────

banner "Test 6: kv_module.remove(\"test_ns\", \"key1\")"

OUTPUT=$(logoscore_call "kv_module" 'kv_module.remove("test_ns", "key1")')

if echo "$OUTPUT" | grep -q "Method call successful"; then
    pass "remove(test_ns, key1)"
else
    fail "remove(test_ns, key1)" "unexpected output"
    echo "$OUTPUT" | tail -5 | sed 's/^/      /'
fi

# ── Test 7: get() after remove — assert empty ─────────────────────────────

banner "Test 7: kv_module.get(\"test_ns\", \"key1\") — expect empty after remove"

OUTPUT=$(logoscore_call "kv_module" 'kv_module.get("test_ns", "key1")')

if echo "$OUTPUT" | grep -q "Method call successful"; then
    RESULT=$(extract_result "$OUTPUT")
    # After remove, result should be empty (empty QByteArray)
    if [[ -z "$RESULT" ]] || echo "$RESULT" | grep -qE '^(\s*|""|null)$'; then
        pass "get(test_ns, key1) = empty after remove"
    else
        fail "get(test_ns, key1)" "expected empty, got: $RESULT"
    fi
else
    fail "get(test_ns, key1)" "unexpected output"
    echo "$OUTPUT" | tail -5 | sed 's/^/      /'
fi

# ── Test 8: clear() — clear namespace ──────────────────────────────────────

banner "Test 8: kv_module.clear(\"test_ns\")"

OUTPUT=$(logoscore_call "kv_module" 'kv_module.clear("test_ns")')

if echo "$OUTPUT" | grep -q "Method call successful"; then
    pass "clear(test_ns)"
else
    fail "clear(test_ns)" "unexpected output"
    echo "$OUTPUT" | tail -5 | sed 's/^/      /'
fi

# ── Test 9: list() after clear — assert empty ─────────────────────────────

banner "Test 9: kv_module.list(\"test_ns\", \"\") — expect empty after clear"

OUTPUT=$(logoscore_call "kv_module" 'kv_module.list("test_ns", "")')

if echo "$OUTPUT" | grep -q "Method call successful"; then
    RESULT=$(extract_result "$OUTPUT")
    # After clear, list should return empty (no keys)
    if [[ -z "$RESULT" ]] || echo "$RESULT" | grep -qE '^(\s*|\[\s*\]|\(\s*\))$'; then
        pass "list(test_ns) = empty after clear"
    else
        fail "list(test_ns)" "expected empty, got: $RESULT"
    fi
else
    fail "list(test_ns)" "unexpected output"
    echo "$OUTPUT" | tail -5 | sed 's/^/      /'
fi

# ── Test 10: Namespace isolation ─────────────────────────────────────────────

banner "Test 10: Namespace isolation — key in ns \"a\" not visible in ns \"b\""

# Set a key in namespace "a"
OUTPUT=$(logoscore_call "kv_module" 'kv_module.set("ns_a", "secret", "hidden")')

if echo "$OUTPUT" | grep -q "Method call successful"; then
    pass "set(ns_a, secret, hidden)"

    # Try to read it from namespace "b" — should be empty
    OUTPUT=$(logoscore_call "kv_module" 'kv_module.get("ns_b", "secret")')

    if echo "$OUTPUT" | grep -q "Method call successful"; then
        RESULT=$(extract_result "$OUTPUT")
        if [[ -z "$RESULT" ]] || echo "$RESULT" | grep -qE '^(\s*|""|null)$'; then
            pass "get(ns_b, secret) = empty (namespace isolation confirmed)"
        else
            fail "namespace isolation" "key from ns_a visible in ns_b: $RESULT"
        fi
    else
        fail "get(ns_b, secret)" "unexpected output"
        echo "$OUTPUT" | tail -5 | sed 's/^/      /'
    fi

    # Cleanup: clear namespace "a"
    logoscore_call "kv_module" 'kv_module.clear("ns_a")' >/dev/null 2>&1
else
    fail "namespace isolation setup" "could not set key in ns_a"
    echo "$OUTPUT" | tail -5 | sed 's/^/      /'
fi

# ── Summary ──────────────────────────────────────────────────────────────────

banner "Results"

echo -e "  ${GREEN}Passed${RESET}: $PASS"
echo -e "  ${RED}Failed${RESET}: $FAIL"
echo -e "  ${YELLOW}Skipped${RESET}: $SKIP"
echo ""

if [[ $FAIL -gt 0 ]]; then
    echo -e "  ${RED}${BOLD}Some tests failed!${RESET}"
    exit 1
fi

echo -e "  ${GREEN}${BOLD}All tests passed!${RESET}"
