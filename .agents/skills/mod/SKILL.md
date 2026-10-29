---
name: mod
description: "Use for the engine's own C++ mods in gameengine2 (docs/goals.md M52): engine/mod (the C ABI header mod/abi.hpp and how its tables grow, the manifest, the SHA-256, the library file on Linux, macOS and Windows, the loader and its refusals), cmake/Mods.cmake (ge_add_mod, the package and its mod.manifest), a game's mod seam (Shooter's Edge's sim/mod_api.hpp and mod_host: hooks, services, the state block in snapshots and digests), the ge mod new/build/check/pack commands, templates/mod-<game>/, the mod consumer skill, the sample mod rocket-bounce and the mod lab; the store form (a WebAssembly module and the engine's interpreter, M52c). Not for the classic game's QC mods (the progs skill)."
---

# Mods in C++

A mod is a package (a `mod.manifest` and one library a platform) that a game loads at run time.
The design, every decision and what was not taken, is [`docs/mods.md`](../../../docs/mods.md);
the rows are `docs/goals.md` M52a–M52e. The QC mods of the classic game are the `progs` skill.

| File | What |
| --- | --- |
| `engine/mod/abi.hpp` | THE ABI, plain C: `ge_mod_host_v1`, `ge_mod_v1`, `GE_MOD_ABI`, `GE_MOD_HAS`, `GE_MOD_EXPORT` |
| `engine/mod/manifest.hpp` | `mod.manifest`: parse, check, write; `this_platform()` |
| `engine/mod/loader.hpp` | `load_mod`: manifest, ABI, game, hash, open, entry, table, in that order |
| `engine/mod/library.hpp` | `dlopen` / `LoadLibraryW`, the one platform file |
| `engine/mod/sha256.hpp`, `host.hpp` | the files' hashes; `fill_host` (the engine's libm in the host table) |
| `cmake/Mods.cmake` | `ge_add_mod`: a module library with the engine's float flags, its manifest after each build; also run with `-P` to write a manifest |
| `cmake/Fp.cmake` | `ge_fp`, `ge_isa`: shared by the engine's build and every mod's |
| `games/shooters_edge/sim/mod_api.hpp` | Shooter's Edge's hooks, services and field ids (`SE_MOD_API`) |
| `games/shooters_edge/sim/mod_host.hpp` | The seam: the composite program, the checked services, `ModRun`, `mod_hash` |
| `games/shooters_edge/mods/rocket_bounce/` | The sample mod, built by the tree into `build/<preset>/mods/rocket-bounce/` |
| `engine/cli/mod.hpp`, `templates/mod-<game>/` | `ge mod new/build/check/pack`; `kModGames` |

## Rules

- **The ABI only grows at a table's end.** Add an entry at the end of a table, raise
  `GE_MOD_ABI_MINOR`, and read the entry only behind `GE_MOD_HAS`. Anything else (a removed or
  reordered entry, a changed signature or meaning) raises `GE_MOD_ABI_MAJOR`, and every mod must
  be rebuilt: say so in the fragment (`breaking: true`). A game's own tables follow the same rule
  with its own `*_MOD_API`.
- **Nothing crosses a table but C**: no C++ type, no exception, no pointer into the game's memory.
  An entity is a number and a field an id, so the same source runs as a sandboxed module (M52c).
- **A service checks every argument** and refuses with a counted, logged refusal and a 0 answer;
  it never asserts on a mod's input.
- **Everything a mod can change is in the game's snapshot.** In Shooter's Edge that is `ModRun`
  (the block, the stream, the counts), folded into the frame hash by `mod_hash`. A new piece of
  mod-visible state goes there, or a seek breaks.
- **A match without a mod keeps every hash.** The `se-loop` and `se-*` digests must not move
  when the seam changes; `mod_hash` runs only when a mod is loaded.
- **Refusals in the diagnostic shape**, naming both versions: "built against mod ABI 2.0, and this
  engine speaks ABI 1.0; next: ...". `mod_finding` splits them into a `ge` finding.
- **A new game offering mods**: its header (`<game>/sim/mod_api.hpp`: hook and services tables,
  each `size` first), its seam over its own loop, a row in `kModGames`, a
  `templates/mod-<game>/`, a sample mod with `ge_add_mod`, and a test that replays a modded match.

## The loop

```sh
make test T="mod_|shooters_edge_mod|cli_mod"   # the format, the seam, the commands
build/debug/labs mod                            # the sample's match beside the plain one
build/debug/ge lab publish mod                  # the page for the report
build/debug/ge mod check build/debug/mods/rocket-bounce
```

A test that needs a built package depends on the `GE_MOD_TARGETS` property and reads the
packages from `GE_MOD_DIR` (`tests/CMakeLists.txt` adds both to every `mod_` test; the labs get
them too). The fixture `tests/mod_abi_two_fixture.cpp` is a mod built against ABI 2.0, for the
refusal.
