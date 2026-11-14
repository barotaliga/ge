---
title: Sign the manifest alone and let its hashes name a tampered file; refuse a broken signature even where unsigned loads
date: 2026-11-14
scope: engine/mod/signature.hpp, engine/mod/loader.cpp, engine/cli/mod.cpp
concepts: [signature, mod-sig, trusted-keys, tamper, loader]
---

The manifest already holds every file's SHA-256, so `mod.sig` signs only the manifest's bytes and
the loader's existing hash step names a changed library with both hashes; the signature check
(step 1, right after the manifest parses) only has to name the manifest. Check the signature
against the bytes read from disk, never a re-written manifest: `write_manifest` of the parsed
struct is not byte-identical (comments, order). `ge mod pack` writes a new manifest, so sign after
packing. The engine already had one signature scheme (`net/crypto.hpp`, the master's tickets), so
`ge_mod` links `ge_net` rather than carrying a second copy of Monocypher's objects; the web and
store builds build `ge_net` anyway.
