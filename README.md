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
void set(QString ns, QString key, QByteArray value);
QByteArray get(QString ns, QString key);
void remove(QString ns, QString key);
QStringList list(QString ns, QString prefix);
void clear(QString ns);
```

Namespacing ensures modules can't read each other's data.

## Status

🚧 Early design phase — see [issues](https://github.com/jimmy-claw/logos-kv-module/issues) for roadmap.

## Who needs this

- [Scala](https://github.com/jimmy-claw/scala) — calendar + event storage
- [Lope](https://github.com/jimmy-claw/lope) — notes + attachment index  
- [LMAO](https://github.com/jimmy-claw/lmao) — agent session state
- Any future Logos Core app

## See also

- [logos-co/ideas#20](https://github.com/logos-co/ideas/issues/20) — original proposal
- [logos-co/logos-app](https://github.com/logos-co/logos-app) — Logos Core
- [logos-module-builder](https://github.com/logos-co/logos-module-builder)
