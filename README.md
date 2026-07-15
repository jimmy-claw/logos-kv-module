# kv_module — Key-Value Storage for Logos Core

Local key-value storage module with swappable backends (memory, file).
Migrated to the **universal authoring model** (basecamp 0.2.0 + logos-module-builder 0.2.0).

## Architecture

Pure C++ implementation — no Qt, no Q_OBJECT, no Q_PLUGIN_METADATA.
All public methods are auto-exposed by `logos-cpp-generator`.

```
src/
├── kv_impl.h          # KvImpl : LogosModuleContext (universal pattern)
├── kv_impl.cpp        # Business logic (KV CRUD, encryption, backend mgmt)
└── backends/          # Swappable storage backends
    ├── KvBackend.h    # Abstract interface
    ├── MemoryBackend.h/cpp  # In-memory (default)
    └── FileBackend.h/cpp    # File-based (when dataDir set)
```

## Build

```bash
# Nix build (only supported method)
nix build --override-input logos-module-builder github:logos-co/logos-module-builder .#kv_module

# Flake check
nix flake --override-input logos-module-builder github:logos-co/logos-module-builder metadata --json
```

## API

| Method | Description |
|--------|-------------|
| `set(ns, key, value)` | Store a value in namespace |
| `get(ns, key)` | Retrieve a value (returns empty string if not found) |
| `remove(ns, key)` | Delete a key from namespace |
| `list(ns, prefix)` | List keys with prefix (returns JSON array) |
| `listAll(ns)` | List all keys in namespace (returns JSON array) |
| `clear(ns)` | Clear all keys in namespace |
| `setDataDir(path)` | Set data directory for file backend |
| `setEncryptionKey(ns, keyHex)` | Set AES-256-GCM encryption key for namespace |

## Encryption

Optional per-namespace AES-256-GCM encryption. Set a 32-byte key (64 hex chars) via `setEncryptionKey()`. All subsequent `set()`/`get()` calls for that namespace are transparently encrypted/decrypted.

## Backends

- **Memory** (default): In-memory storage, no persistence
- **File**: JSON file per namespace (activated by `setDataDir()`)

## Migration Notes (v0.1 → v0.2)

- Removed Qt dependency — pure C++ implementation
- Removed hand-written plugin glue (`kv_plugin.cpp/h`, `plugin.cpp`, `i_kv_module.h`)
- Replaced manual CMake with `LogosModule.cmake` macro
- Replaced `module.yaml` with `metadata.json` (universal interface)
- Base64 encoding/decoding now inline (was using Qt QByteArray)
