# Changelog

All notable changes to logos-kv-module are documented here.

## [v0.0.2] - 2026-03-09

- b62d26e fix: use listAll() instead of list(ns,) — empty arg dropped by logoscore
- c85f10f refactor: single logoscore session for all tests (much faster)
- c65d25a fix: fail loudly if plugin .so not found during setup
- 0d7e370 fix: use -m and -c flags (correct logoscore CLI flags per tutorial)
- b886233 debug: add pre-flight debug output
- 5fb2935 fix: use kv_module (not kv_module_plugin) as --load-modules arg
- 937a596 fix: auto-setup correct module dir structure with platform-keyed manifest
- 33073fe fix: correct manifest format - main must be platform-keyed object
- 40eb897 fix: install plugin into lib/kv_module/ subdir with manifest.json
- e9866d0 fix: add main field and list all backends in metadata.json
- 41da639 fix: use kv_module_plugin as load-modules arg (matches .so filename)
- e404564 docs: fix stale path comments in e2e script
- fe34000 fix: auto-detect repo root and logoscore path in e2e script
## [v0.0.1] - 2026-03-09


## [v0.1.0] - 2026-03-09

- feat: initial release — Memory, File, RocksDB, SQLite backends
- feat: Qt plugin interface (IKvModule) with Logos Core integration
- feat: set/get/list/listAll/remove/clear methods via QtRO
- ci: conformance tests, Qt plugin build, RocksDB, SQLite, E2E Nix jobs
- docs: README, logoscore testing gotchas PR to logos-module-builder
