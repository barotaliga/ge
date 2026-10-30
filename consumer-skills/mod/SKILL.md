---
name: mod
description: "Use when writing, building, checking or packing a C++ mod for a game on the engine (`ge mod new/build/check/pack`), or when a game loads one: the mod ABI (engine/mod/abi.hpp, a versioned C table each way), the game's own hook and services tables, the state block and the determinism rules a mod keeps, the package and its mod.manifest, and what each loader refusal means."
---

# Mods in C++

A mod is a shared library a player builds from C++ and the game loads at run time, like a plugin.
It links nothing of the engine: it compiles against `mod/abi.hpp` (the engine's half of the ABI)
and its game's header (Shooter's Edge: `shooters_edge/sim/mod_api.hpp`), and the two sides talk
through tables of C function pointers. The engine's `docs/mods.md` is the design and the reasons.

## The loop

```sh
ge mod new my-mod --game=shooters_edge   # CMakeLists.txt, src/mod.cpp (a working mod), README
ge mod build my-mod                      # build/mods/my-mod/: the library and its mod.manifest,
                                         # checked as the game's loader would
ge mod check my-mod/build/mods/my-mod    # the loader's verdict alone (--game=<id> to force one)
ge mod pack linux/my-mod windows/my-mod --out=dist/my-mod   # several platforms' builds, one package
```

`ge mod build --engine=<tree>` builds against another engine tree than the one the repo names.
Errors come back as findings with a next step, the compiler's at their file and line.

## Writing one

1. **One exported symbol**: `GE_MOD_EXPORT const ge_mod_v1* ge_mod_entry(uint32_t host_abi)`
   returns a static `ge_mod_v1`: `size = sizeof(ge_mod_v1)`, `abi = GE_MOD_ABI`, the name (the
   manifest's), the game id and the game API the header names, `state_bytes = sizeof(State)`,
   and `hooks` pointing at the game's hook table (`se_mod_hooks_v1`, its `size` and `api` set).
   Return null when `GE_MOD_ABI_MAJOR_OF(host_abi)` is not yours.
2. **Hooks run after the game's own rules** at the same points (start frame, a player's pre- and
   post-think, connect, put in server, disconnect), so a hook sees what the game just did: a
   missile it fired is `newmis` after the post-think.
3. **Your own functions**: `function(k)` (k from 1) is the number to store in an entity's think,
   touch or blocked; the game calls your `call(k, self, other)` when it comes due.
   `call_function(fn, self, other)` runs any function, the game's too, so a mod that takes over a
   touch can still run the game's after its own.
4. **Entities are numbers, fields are ids** (`SE_FIELD_*`): `get_float/set_float`, `get_int`,
   `get_vec`, `spawn`, `remove`, `set_size`. A bad entity or field is refused (a getter answers 0)
   and logged; it never crashes the game. Read the game's log when a change does not stick.

## The rules a mod keeps (or replays and multiplayer break)

- **All state in the block** the game hands every call (`void* state`, `state_bytes` long, zeroed
  at a match's start, then `init`). No globals, no `static` locals that change, no pointers inside
  the block: the game copies it into its snapshots and folds it into its digest.
- **No clock, no `rand`, no threads, no files, no network.** Random numbers come from
  `host->random` / `host->random_u32`, the game's seeded stream.
- **Transcendentals through the table** (`host->sin`, `host->atan2`, `host->pow`, ...): the
  engine's libm, the same bits on every platform. `sqrt`, `fabs`, `floor` and the like are exact
  everywhere. The build (`cmake/Mods.cmake`) sets the engine's float flags: no FMA, no fast-math.
- **No exception across the table**, and no C++ type: every entry is plain C.
- **Newer entries**: before calling a host or service entry added after the first version, test
  it: `GE_MOD_HAS(ge_mod_host_v1, host, entry)`.

## When the loader refuses

| Message says | Do |
| --- | --- |
| `needs mod ABI X.Y, and this engine speaks ABI A.B` / `built against mod ABI X.Y` | Rebuild against the engine the game ships with (`ge mod build --engine=<tree>`) |
| `a mod for <game>, not for <other>` / `game_api N ... offers M` | Build against that game's header and version |
| `has SHA-256 ..., the manifest lists ...` | The library is not the one packaged: rebuild, or re-pack |
| `lists no library for <platform>` | Build on that platform and `ge mod pack` the builds together |
| `does not open` / `exports no ge_mod_entry` | Build it for this platform; define the entry with `GE_MOD_EXPORT` |
