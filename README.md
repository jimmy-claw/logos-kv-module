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
```

Namespacing ensures modules can't read each other's data.

## Building with Nix

Requires [Nix](https://nixos.org/) with flakes enabled.

```bash
nix build       # build the module (plugin + headers)
nix build .#test  # build and run conformance tests
nix develop     # enter a dev shell with all dependencies
```

The Nix build uses [logos-module-builder](https://github.com/logos-co/logos-module-builder) and provides Qt6, logos-cpp-sdk, and logos-liblogos automatically.

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
