# logos-kv-module

A local key-value storage module for [Logos Core](https://logos.co), with swappable backends.

## The Problem

Apps built on Logos Core (Scala, Lope, LMAO, etc.) all need local persistence. There's currently no standard solution — every app embeds its own storage, leading to fragmentation and duplicated effort.

## The Solution

A reusable Logos Core module exposing a simple KV interface via QtRO, with pluggable backends:

| Backend | Use case |
|---|---|
| **Memory** | Testing, ephemeral state |
| **File** | Simple JSON, zero native deps |
| **RocksDB** | Production, high-performance |
| **SQLite** | Structured queries, familiar |

## Interface

```cpp
// QtRO interface — accessible from QML and other modules
void     set(QString ns, QString key, QString value);
QString  get(QString ns, QString key);
void     remove(QString ns, QString key);
QString  list(QString ns, QString prefix);    // returns JSON array
QString  listAll(QString ns);                 // list with empty prefix
void     clear(QString ns);
QString  version();

// Search
QString  scan(QString ns, QString pattern);           // returns JSON array of matching keys
QString  searchValues(QString ns, QString substring); // returns JSON array of {key, value}

// Optional per-namespace encryption (AES-256-GCM)
void     setEncryptionKey(QString ns, QString keyHex);
```

Namespacing ensures modules can't read each other's data.

### Search

`scan(ns, pattern)` finds keys containing `pattern` as a substring. Empty pattern returns all keys.

`searchValues(ns, substring)` finds entries where the value contains `substring` (case-insensitive). Returns a JSON array of `{"key": "...", "value": "..."}` objects.

```cpp
kv_module.scan("myapp", "user");
// ["user:1", "user:2", "admin_user"]

kv_module.searchValues("myapp", "meeting");
// [{"key":"event:1","value":"Team Meeting at 10am"}, ...]
```

### Encryption

Optional AES-256-GCM encryption can be enabled per namespace. When a key is set, `set()` encrypts values before storing and `get()` decrypts them transparently.

```cpp
// keyHex: 64 hex characters representing a 32-byte AES-256 key
kv_module.setEncryptionKey("myapp", "0123456789abcdef...");
kv_module.set("myapp", "secret", "sensitive data");  // stored encrypted
kv_module.get("myapp", "secret");                     // returns "sensitive data"
```

- Keys are held in memory only — never persisted by kv_module
- Caller is responsible for key derivation (e.g. PBKDF2 from user password)
- Namespaces without a key continue to work as plaintext (backward compatible)
- Stored format: `base64(nonce[12] + ciphertext + tag[16])`

## Installation

### From GitHub Releases

Download the pre-built `.so` from [Releases](https://github.com/jimmy-claw/logos-kv-module/releases):

```bash
curl -LO https://github.com/jimmy-claw/logos-kv-module/releases/latest/download/kv_module_plugin-linux-x86_64.so
cp kv_module_plugin-linux-x86_64.so /path/to/logoscore/modules/kv_module_plugin.so
```

### From Nix

Requires [Nix](https://nixos.org/) with flakes enabled.

```bash
nix build                  # build via logos-module-builder (plugin + headers)
nix build .#standalone     # build .so only (no logos-module-builder dependency)
nix build .#test           # build and run conformance tests
nix develop                # enter a dev shell with all dependencies
```

The `.so` is at `result/lib/kv_module_plugin.so`.

### From source

```bash
sudo apt-get install cmake qt6-base-dev qt6-declarative-dev libssl-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
# Output: build/kv_module_plugin.so
```

The default Nix build uses [logos-module-builder](https://github.com/logos-co/logos-module-builder) and provides Qt6, logos-cpp-sdk, and logos-liblogos automatically. The `standalone` output builds with cmake directly.

## Testing

### Conformance Tests (CI)

Unit-level backend conformance tests run on every push:

```bash
nix build .#test
```

### E2E Tests via logoscore

Full integration tests using the `logoscore` headless Logos Core harness — exercises the complete stack: Qt plugin loader → `logos_host` subprocess → QtRO → kv_module.

```bash
nix build && bash scripts/e2e-logoscore.sh
```

Tests: `version()`, `set`/`get`/`list`/`remove`/`clear`, and namespace isolation. Also runs in CI via `nix build .#test`.

### Example logoscore calls

```bash
LOGOSCORE=path/to/logoscore
MODULES=path/to/result/lib

QT_QPA_PLATFORM=offscreen    --modules-dir  --load-modules kv_module   --call 'kv_module.set(myapp, theme, dark)'

QT_QPA_PLATFORM=offscreen    --modules-dir  --load-modules kv_module   --call 'kv_module.get(myapp, theme)'
```

## Status

✅ v0.1 complete — production-ready backends, Logos Core integrated, CI passing.

## Who needs this

- [Scala](https://github.com/jimmy-claw/scala) — calendar + event storage
- [Lope](https://github.com/jimmy-claw/lope) — notes + attachment index
- [LMAO](https://github.com/jimmy-claw/lmao) — agent session state
- Any future Logos Core app

## See also

- [logos-co/ideas#20](https://github.com/logos-co/ideas/issues/20) — original proposal
- [logos-co/logos-app](https://github.com/logos-co/logos-app) — Logos Core
- [logos-module-builder](https://github.com/logos-co/logos-module-builder)
