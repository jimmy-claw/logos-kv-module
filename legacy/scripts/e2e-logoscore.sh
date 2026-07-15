#!/usr/bin/env bash
# =============================================================================
#  e2e-logoscore.sh — E2E tests: logoscore --call with kv_module
# =============================================================================
#
#  Validates the KV module plugin stack headlessly using a single logoscore
#  invocation (one logos_host process, all calls sequential = fast).
#
#  Tests:
#    1. version()           — basic plugin load
#    2. set()               — store a value
#    3. get()               — retrieve and assert value
#    4. set() second key    — store second value
#    5. list()              — list keys, assert both present
#    6. remove()            — remove first key
#    7. get() after remove  — assert empty
#    8. clear()             — clear namespace
#    9. list() after clear  — assert empty
#   10. Namespace isolation — key in ns "a" not visible in ns "b"
#
#  Prerequisites:
#    - Nix build:  cd <repo> && nix build
#      Produces:   <repo>/result/lib/kv_module_plugin.so
#    - logoscore: auto-detected from nix store
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
        PLUGIN_SRC="$(find "$REPO_DIR/result" -name "kv_module_plugin.so" 2>/dev/null | head -1 || true)"
    fi

    TMP_MODULES="/tmp/kv-modules-$$"
    mkdir -p "$TMP_MODULES/kv_module"

    if [[ -n "$PLUGIN_SRC" && -f "$PLUGIN_SRC" ]]; then
        cp "$PLUGIN_SRC" "$TMP_MODULES/kv_module/"
    else
        echo "ERROR: kv_module_plugin.so not found! Searched: $REPO_DIR/result"
        find "$REPO_DIR/result" -name "*.so" 2>/dev/null | head -5 || echo "No .so files in result/"
        exit 1
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

BOLD='\033[1m'; RESET='\033[0m'
GREEN='\033[0;32m'; CYAN='\033[0;36m'; RED='\033[0;31m'; YELLOW='\033[1;33m'

PASS=0; FAIL=0

pass() { echo -e "  ${GREEN}PASS${RESET}: $1"; PASS=$((PASS + 1)); }
fail() { echo -e "  ${RED}FAIL${RESET}: $1 — $2"; FAIL=$((FAIL + 1)); }
banner() { echo -e "\n${CYAN}${BOLD}━━━ $1 ━━━${RESET}\n"; }

# ── Pre-flight ───────────────────────────────────────────────────────────────

banner "Pre-flight Checks"

if [[ ! -x "$LOGOSCORE" ]]; then
    echo -e "${RED}logoscore not found at $LOGOSCORE${RESET}"
    exit 1
fi
pass "logoscore binary: $LOGOSCORE"

if [[ ! -d "$MODULES_DIR" ]]; then
    echo -e "${RED}modules directory not found at $MODULES_DIR${RESET}"
    exit 1
fi
pass "modules directory: $MODULES_DIR"

# ── Run all calls in a single logoscore invocation ───────────────────────────

banner "Running Tests (single logoscore session)"

OUTPUT=$(QT_QPA_PLATFORM=offscreen timeout 120 "$LOGOSCORE" \
    --modules-dir "$MODULES_DIR" \
    --load-modules kv_module \
    --call "kv_module.version()" \
    --call 'kv_module.set(test_ns, key1, hello)' \
    --call 'kv_module.get(test_ns, key1)' \
    --call 'kv_module.set(test_ns, key2, world)' \
    --call 'kv_module.listAll(test_ns)' \
    --call 'kv_module.remove(test_ns, key1)' \
    --call 'kv_module.get(test_ns, key1)' \
    --call 'kv_module.clear(test_ns)' \
    --call 'kv_module.listAll(test_ns)' \
    --call 'kv_module.set(ns_a, secret, hidden)' \
    --call 'kv_module.get(ns_b, secret)' \
    --call 'kv_module.clear(ns_a)' \
    2>&1 || true)

# Extract "Method call successful. Result: <value>" lines in order
RESULTS=()
while IFS= read -r line; do
    if echo "$line" | grep -q "Method call successful"; then
        RESULTS+=("$(echo "$line" | sed 's/.*Result: //')")
    elif echo "$line" | grep -q "^Error:"; then
        RESULTS+=("ERROR: $line")
    fi
done <<< "$OUTPUT"

# ── Assert results ────────────────────────────────────────────────────────────

banner "Test 1: version()"
R="${RESULTS[0]:-}"
if [[ -n "$R" && "$R" != ERROR* ]]; then pass "version() = $R"; else fail "version()" "${R:-no result}"; fi

banner "Test 2: set(test_ns, key1, hello)"
R="${RESULTS[1]:-}"
if [[ "$R" != ERROR* ]]; then pass "set(test_ns, key1, hello)"; else fail "set" "$R"; fi

banner "Test 3: get(test_ns, key1) — expect hello"
R="${RESULTS[2]:-}"
if echo "$R" | grep -q "hello"; then pass "get(test_ns, key1) = hello"; else fail "get(test_ns, key1)" "expected hello, got: $R"; fi

banner "Test 4: set(test_ns, key2, world)"
R="${RESULTS[3]:-}"
if [[ "$R" != ERROR* ]]; then pass "set(test_ns, key2, world)"; else fail "set" "$R"; fi

banner "Test 5: list(test_ns) — expect key1 and key2"
R="${RESULTS[4]:-}"
if echo "$R" | grep -q "key1" && echo "$R" | grep -q "key2"; then
    pass "list(test_ns) contains key1 and key2"
else
    fail "list(test_ns)" "expected key1+key2, got: $R"
fi

banner "Test 6: remove(test_ns, key1)"
R="${RESULTS[5]:-}"
if [[ "$R" != ERROR* ]]; then pass "remove(test_ns, key1)"; else fail "remove" "$R"; fi

banner "Test 7: get(test_ns, key1) — expect empty after remove"
R="${RESULTS[6]:-}"
if [[ -z "$R" ]] || echo "$R" | grep -qE '^(\s*|""|null)$'; then
    pass "get(test_ns, key1) = empty after remove"
else
    fail "get after remove" "expected empty, got: $R"
fi

banner "Test 8: clear(test_ns)"
R="${RESULTS[7]:-}"
if [[ "$R" != ERROR* ]]; then pass "clear(test_ns)"; else fail "clear" "$R"; fi

banner "Test 9: list(test_ns) — expect empty after clear"
R="${RESULTS[8]:-}"
if [[ -z "$R" ]] || echo "$R" | grep -qE '^(\s*|\[\s*\]|\(\s*\))$'; then
    pass "list(test_ns) = empty after clear"
else
    fail "list after clear" "expected empty, got: $R"
fi

banner "Test 10: Namespace isolation"
R="${RESULTS[10]:-}"
if [[ -z "$R" ]] || echo "$R" | grep -qE '^(\s*|""|null)$'; then
    pass "get(ns_b, secret) = empty (namespace isolation confirmed)"
else
    fail "namespace isolation" "key from ns_a visible in ns_b: $R"
fi

# ── Summary ──────────────────────────────────────────────────────────────────

banner "Results"
echo -e "  ${GREEN}Passed${RESET}: $PASS"
echo -e "  ${RED}Failed${RESET}: $FAIL"
echo ""

if [[ $FAIL -gt 0 ]]; then
    echo -e "  ${RED}${BOLD}Some tests failed!${RESET}"
    exit 1
fi
echo -e "  ${GREEN}${BOLD}All tests passed!${RESET}"
