# Engine mods

How a game on this engine loads a mod a player wrote in C++: the ABI between the two, the rules
that keep a modded match deterministic, what the loader checks, the form a mod takes on each
platform, the package, and the `ge mod` commands that make one. `engine/mod` is built to this
page, and Shooter's Edge is the first game that offers hooks (`docs/goals.md` M52). Where a part
is not built yet, this page is the plan; change it in the same PR that changes the plan.

This is the engine's OWN mod format, for any game on it. It is not Shooter's Edge's
compatibility with the classic game's mods (`plugins/progs`, M18p), which runs an existing
mod's bytecode unchanged: those mods keep working, and this format sits beside them.

## What a mod is

A mod is a package: a text manifest (`mod.manifest`) and one library a platform, built from C++
by `ge mod build`. A game that offers mods loads a package, checks it and calls it at the points
of its loop it names (its HOOKS). The mod reaches the game only through tables of C function
pointers, one each way, and keeps everything that changes between frames in a block of memory
the game owns. That is the whole contract:

```
   game                                     mod (a shared library)
   ────                                     ──────────────────────
   load_mod(dir) ── manifest, ABI, hash ──▶ ge_mod_entry(host_abi)  (the one exported symbol)
                 ◀──── ge_mod_v1 ─────────  name, game, state_bytes, init, hooks ─▶ the game's
                                                                                   hook table
   ge_mod_host_v1 ── libm, random, log ───▶ called with (state, host, ...)
     └ game ─▶ the game's services table    entities by number, fields by id
```

## The ABI

`engine/mod/abi.hpp` is the engine's half, plain C (C99 compiles it), and each game adds a
header of its own (Shooter's Edge's is `games/shooters_edge/sim/mod_api.hpp`). Four tables:

| Table | Who fills it | What it carries |
| --- | --- | --- |
| `ge_mod_host_v1` | the engine (`fill_host`) and the game | the engine's libm, the game's random stream, the log, and a pointer to the game's services |
| `ge_mod_v1` | the mod | its name, version, game, game API, the size of its state block, `init`, and a pointer to its hooks |
| the game's services (`se_mod_services_v1`) | the game | what a mod may read and change: entities, fields, spawning, sounds, functions |
| the game's hooks (`se_mod_hooks_v1`) | the mod | the points of the game's loop it runs at, and its own functions |

**Decisions:**

- **A versioned table of C function pointers each way, behind one exported symbol.** The mod
  exports `ge_mod_entry(uint32_t host_abi)` and returns a static `ge_mod_v1`; everything else is
  reached through the tables. One symbol keeps the loader's surface to a single lookup on every
  platform, and the tables can be passed to the same code whether it was loaded as a native
  library or run as a module (§ Platforms).
  *Not taken:* a C++ interface (virtual classes, `std::string`, `std::vector` across the line),
  because the C++ ABI differs between compilers and standard libraries, and a mod built with
  MSVC would not load into a game built with Clang. Exporting many symbols, one a hook, was not
  taken either: every new hook would be a symbol to look up and version, and the module form
  would need a second mechanism.
- **Every table starts with its `size` and its `abi`.** A table grows only by adding entries at
  its end. A reader looks at an entry only when the table's `size` covers it (`GE_MOD_HAS`), so
  a newer game reads an older mod's shorter tables, and a mod built against a newer header than
  the game it runs in tests an entry before it calls it. The game's own tables follow the same
  rule (`tests/shooters_edge_mod_test.cpp` feeds a shortened hook table).
  *Not taken:* a version number alone, with one struct per version (`v1`, `v2`, ...) that the
  reader switches on, because each version would be a copy the game keeps forever; and query
  interfaces by id (a COM-style `get_interface`), which pay for a lookup and a string or GUID
  table that a growing struct does not need.
- **ABI `major.minor` (`GE_MOD_ABI`, 1.0 now).** The minor moves when entries are added at a
  table's end, and an older mod still loads. The major moves when anything already there
  changes: a removed entry, a changed signature, a changed meaning. The loader refuses a mod
  whose major is not the engine's, and a manifest that asks for a newer minor than the engine
  has. Each refusal names both versions: "built against mod ABI 2.0, and this engine speaks ABI
  1.0" (`tests/mod_loader_test.cpp`).
  *Not taken:* semver for the whole engine as the gate, because an engine release that changes
  the renderer has not changed what a mod sees.
- **The game declares its hooks in its own header, versioned on its own (`game_api`).** The
  engine knows that a game has a hook table and a services table, never what is in them. A
  manifest names the game (`shooters_edge`) and the range of the game's API it runs on (`1`, or
  `1..3`). The loader refuses another game, or an API outside the range.
  *Not taken:* one engine-wide list of hooks, because a shooter's player think and a skier's
  step have nothing in common, and a list for every game would make each game's change the
  engine's.
- **No pointers into the game.** An entity is a number and a field is an id (`SE_FIELD_*`), and
  every service checks its arguments. A bad entity, a free one or a field of the wrong type is
  refused: the call is counted, the game's log gets a line, a getter answers 0, and the match
  goes on (`tests/shooters_edge_mod_test.cpp`, four refusals a frame). This costs a call per
  field, and it buys three things: a mod cannot corrupt the game's memory through the tables,
  the game may change its entity layout without breaking a mod, and the same source runs as a
  sandboxed module whose memory cannot address the game's (§ The store form).
  *Not taken:* handing the mod a pointer to the entity array and the field offsets, which is
  faster but makes the game's layout part of the ABI and cannot be sandboxed.
- **No exception, no C++ type and no allocation crosses a table.** Strings are `const char*`
  that the receiver copies; vectors are three floats.

**Shooter's Edge's hooks** are its game-program seam's (`plugins/shooter/program.hpp`): match
start, start frame, a player's pre- and post-think, connect, put in server, disconnect. The game
runs each event first and the mod's hook after it, so a mod sees what the game just did (the new
missile in `newmis` after a player's post-think) and may change it. A mod's own functions get
numbers from `function(k)`, to store in an entity's think, touch or blocked; the loop calls the
mod's `call(k, self, other)` when one comes due. `call_function` runs any function by its number,
so a mod that takes over a touch can still run the game's own. The seam numbers the mod's
functions from `kModFuncBase` (2^24), above any program's own, so the two never collide.

## Determinism

A replay, a ghost, a seek and network prediction all depend on a match playing to the same bits
from the same inputs. A mod is inside the match, so it keeps the engine's rules
(`docs/architecture.md` § Determinism), and the ABI is built so that following them is the easy
path:

- **No clock.** The host table offers no time but the game's (the services' `time` and
  `frametime`); a mod that reads the wall clock breaks its own replay, and the lab shows it.
- **Random numbers from the game's stream.** `host->random_u32` and `host->random` draw from a
  `core::Rng` the game seeds from the match's seed (`MatchOptions::mod_seed`), apart from the
  game's own stream. Adding a mod does not move a single draw of the game's. *Not taken:* sharing
  the game's stream, which would make every draw of the game's depend on whether a mod drew
  first.
- **All state in the block the game owns.** The mod declares `state_bytes` (at most 1 MB,
  `GE_MOD_STATE_MAX`); the game allocates it, zeroes it at a match's start, calls `init`, and
  hands it to every hook. Shooter's Edge keeps the block, the stream and the mod's counts in
  `ModRun`, a value that a snapshot copies like the server and the rules
  (`tests/shooters_edge_mod_test.cpp` seeks mid-match and plays 300 identical frames), and folds
  them into each frame's hash (`mod_hash`), so a mod that changes its block changes the digest.
  A match without a mod keeps every hash it had. *Not taken:* asking the mod to serialize itself
  (`save`/`load` hooks), because a mod that forgets a field breaks a seek silently, while a
  copied block cannot forget. The cost is that the block holds no pointers: a mod uses indices.
- **The float rules.** A mod is built by `cmake/Mods.cmake`, which links the engine's own
  `ge::fp` and `ge::isa` (now in `cmake/Fp.cmake`, shared by the engine's build and a mod's): no
  FMA contraction, no fast-math, the same instruction-set baseline.
- **Transcendentals through the table.** `host->sin`, `host->atan2`, `host->pow` and the rest are
  the engine's vendored libm (`engine/core/libm`), the same bits on every platform. A mod never
  calls `<cmath>`'s, whose bits differ by platform; `sqrt`, `fabs`, `floor`, `ceil`, `trunc` and
  `fmod` are exact in IEEE 754 and safe anywhere. *Not taken:* linking the libm into each mod,
  which would put a second copy of the engine's code into every package and make a fix there a
  rebuild of every mod.
- **The order is the game's.** Hooks run in the order the loop reaches them, on the game's
  thread.

`tests/shooters_edge_mod_test.cpp` holds the replay (two runs, frame by frame), the seek, and the
mod's stream following the seed. The `mod` lab fails when two modded runs part.

## The sandbox

A native library runs with the game's privileges: a desktop OS cannot stop a C++ mod from
calling `fopen`. So the sandbox is a contract the tables make easy to keep, and the store form
makes it a wall:

- **What the tables offer:** the game's entities, the game's random stream, the engine's libm,
  the log, sounds by name. **No files, no network, no threads, no clock, no allocator**: none of
  them is in a table, so a mod that keeps to its headers cannot reach them. A mod's data that
  would be a file is compiled into it.
- **What the loader checks** (§ The loader): the manifest, the ABI and the game, the SHA-256 of
  every library against the manifest before it is opened, the entry table's fields, the state
  block's size. It does not inspect the code.
- **What the game checks at run time:** every service argument (above), a hook table's `size`
  and `api`, and the mod's function numbers. A refusal never crashes the game.
- **The wall comes with the store form** (M52c): a module run by the engine's interpreter can
  import only the ABI's tables, has its own bounded memory, and runs on a fuel budget, so a mod
  that loops forever is stopped. A desktop game may then choose to run every mod in that form,
  for a server that loads mods it did not build.

*Not taken:* an OS sandbox around the native library (a separate process, seccomp), because a
hook is a call made thousands of times a second inside the frame, and a process boundary costs
microseconds a call. *Not taken either:* scanning the library for imported symbols (`fopen`,
`socket`), which is easy to get around and differs by platform; signing (§ The package) is how a
player trusts where a native mod came from.

## The loader

`load_mod(path, {game, api}, out)` in `engine/mod/loader.hpp`, in this order (the first failure
stops the load):

1. The manifest (`mod.manifest` in the directory, or the file given) parses and checks; every
   error is reported at once, and an unknown key is a warning, so a newer manifest still loads.
2. Its `abi` major is the engine's, and its minor is no newer than the engine's.
3. Its `game` is the loading game's, and its `game_api` range holds the game's version.
4. It lists a file for this platform (`this_platform()`: `linux-x64`, `macos-arm64`,
   `windows-x64`, ...), the file is there, and its SHA-256 is the one listed.
5. The library opens (`dlopen` on Linux and macOS, `LoadLibraryW` on Windows:
   `engine/mod/library.hpp`) and exports `ge_mod_entry`, which returns a table.
6. The table is long enough, speaks the engine's ABI major, names the manifest's name and game, a
   game API inside the manifest's range, a state block within the limit, and hooks.

Then the game checks its own hook table (`check_hooks`: its `size` and its `api`). Every message
is the engine's diagnostic shape, what failed and the next action: "the library was built against
mod ABI 2.0, and this engine speaks ABI 1.0; next: rebuild it against this engine (ge mod build
--engine=<tree>)".

## Platforms

| Platform | Form | Row |
| --- | --- | --- |
| Linux, macOS, Windows (x64 and arm64) | a shared library (`.so` on Linux and macOS, `.dll` on Windows), one per platform in the package | M52a (this), CI on all three in M52b |
| iOS, iPadOS | the portable module, run by the engine's interpreter | M52c |
| Android | the portable module, the same | M52c |
| A console, or a store build that ships curated mods inside the app | the mod's source linked into the game ahead of time | M52c |

The desktop form is the native library: the fastest, debuggable with the platform's own tools,
and built by the compiler the player already has. `cmake/Mods.cmake` builds it as a `MODULE`
library with hidden visibility (only the entry is exported), the engine's float flags, the name
the manifest lists, and the manifest written after each build. On macOS the suffix is `.so`,
CMake's own default for a module there, so the two POSIX platforms name their files alike.

### The store form

Apple's rules for the App Store do not let an app download and run native code, and iOS does
not let an app map memory executable, so no app on it can load a downloaded library or run a JIT.
Interpreted code that the app runs is allowed. Google Play's rules forbid an app from downloading
executable code (a library, a dex) from anywhere but Play, with the same exemption for code run
in a virtual machine or interpreter that has no access to Android's APIs.

**The answer: the same C++ source compiled to a WebAssembly module, run by an interpreter the
engine owns, in C++, with no JIT.**

- A WebAssembly module is a portable, sandboxed bytecode that clang compiles C and C++ to
  (`--target=wasm32`), so a player writes one source and `ge mod build` makes both forms. The
  manifest gets a `file.wasm32` row beside the native ones, and a store build loads only that
  row.
- The interpreter is ours (`engine/mod/wasm`, M52c), small (the MVP instruction set, no threads,
  no SIMD to start), and deterministic: WebAssembly's arithmetic is IEEE 754 with defined
  rounding, its one non-determinism (the bits of a NaN) is canonicalised by the interpreter,
  and the transcendentals are the host table's imports, as in the native form. It has no JIT, so
  it needs no executable memory and falls under both stores' exemptions.
- **The module's imports ARE the ABI's tables.** That is why the ABI hands out no pointer into
  the game: a module's memory is its own, and an entity number and a field id mean the same in
  both forms. A table entry becomes an import of the same name; a string crosses as an offset and
  a length in the module's memory, which the host copies. The state block lives in the module's
  memory, so a snapshot copies it the same way.
- Memory is bounded by the module's declared maximum, and each hook runs on a fuel budget (a
  count of instructions), so an endless loop is stopped and reported, never a hung game.
- An interpreter is slower than native code, often by an order of magnitude or more. A mod's
  hooks are a small part of a frame (a few thousand calls of a few hundred instructions), so this
  is affordable; `ge mod check --run` will report each hook's cost (M52b), and a mod whose cost
  breaks the frame's budget on a phone is refused by the lab.
- **The fallback, ahead-of-time linking**, is for a build that ships curated mods inside the app
  (a console, or a store build with a set of mods chosen by the developer): the mod's source is
  compiled into the game like any other code, and its entry is registered instead of looked up.
  It is fast and allowed everywhere, but a player cannot add a mod to it.

*Not taken:*

- **A scripting language** (Lua, JavaScript and the like): a mod would no longer be C++, the
  engine would carry a second language's runtime and its garbage collector (which breaks the
  snapshot-by-copy rule), and the two platforms would run different code.
- **A JIT for the module**: forbidden on iOS, and a source of bugs that differ by platform.
- **An existing WebAssembly runtime**: the general ones are much larger than the subset needed,
  are built around a JIT or an ahead-of-time compiler, or do not promise deterministic NaNs, and
  a third-party runtime's bits would be outside the engine's goldens. An
  interpreter of the MVP subset is a few thousand lines, held bit-exact by goldens like the rest
  of the engine.
- **Native libraries bundled in the app and chosen at run time**: the same as ahead-of-time
  linking with more steps, and still no player-made mods.
- **The classic game's bytecode VM for the engine's own mods**: its instruction set is a 32-bit
  float machine built for one game, with no C++ compiler targeting it.

## The package

A package is a directory: `mod.manifest` and the files it lists.

```
# A mod package (docs/mods.md § The package). Written by the build (cmake/Mods.cmake).
name = rocket-bounce
version = 1.0.0
game = shooters_edge
game_api = 1
abi = 1.0
file.linux-x64 = rocket_bounce.so c8c350ea...
file.windows-x64 = windows-x64/rocket_bounce.dll 5d0e17a1...
```

One `key = value` a line, `#` comments. `name` is kebab case, `version` is `major.minor.patch`,
`game_api` is `N` or `N..M`, `abi` is the lowest `major.minor` the mod needs, and each
`file.<platform>` is a path beside the manifest and its SHA-256. The manifest is text so a
player can read it and a diff can show it; it is the table of contents a signature will cover.

**Decisions:**

- **The hash of every file in the manifest**, checked before a library is opened. A package
  rebuilt but not repacked, or a file swapped, is refused with both hashes. This is what makes a
  signature over the manifest alone enough (M52d).
- **One package for every platform** (`ge mod pack` merges the builds), so a server names one
  package and each player loads the file for their platform.
- *Not taken:* a zip or another archive format, because a directory needs no reader in the
  engine and the stores repack whatever they ship anyway. *Not taken:* JSON or TOML for the
  manifest, because a line format this small needs no parser library, and its errors can name
  the line.

**Later rows:**

- **Signing (M52d).** An Ed25519 signature over the manifest by the author's key, in a
  `mod.sig` beside it; the game keeps a list of keys it trusts (its own, its community's) and
  says, for an unsigned or unknown package, what a player is about to run.
- **Distribution (M52d).** A server sends its mod list (name, version, manifest hash) to a
  joining client, which fetches each package from where the server says, checks it, and loads
  it. Two players in a match always run the same mods, or the client is refused with the list.

## The commands

`ge mod` (`engine/cli/mod.hpp`) takes a mod from nothing to a package, and each refusal comes
back as a finding with its next step:

| Command | What it does |
| --- | --- |
| `ge mod new <dir> --game=<id>` | A mod repo from `templates/mod-<game>/`: a `CMakeLists.txt` that includes the engine tree's `cmake/Mods.cmake`, a working mod in `src/mod.cpp`, a `.gitignore` and a README |
| `ge mod build [<dir>]` | CMake configure and build (Release) into `<dir>/build`; the package lands in `build/mods/<name>/` and is checked as the loader would; compiler errors come back at their file and line |
| `ge mod check <package>` | The loader on a package: every check above, with `--game=<id>` to check against a game |
| `ge mod pack <package>... [--out=<dir>]` | One package from several platforms' builds of the same mod: each checked, the manifests merged, the files copied |

The games that offer mods are a table (`kModGames`): the game id, the version of its API, and
where its header is. The consumer skill `mod` (`consumer-skills/mod/SKILL.md`) is what a mod's
author reads: the loop, the rules a mod keeps, and what each refusal means. The engine's own
`mod` skill (`.agents/skills/mod/SKILL.md`) is for changing the format.

Later (M52b): `ge mod check --run` plays the game's mod scenario with the package twice and
compares the digests; the SDK (`abi.hpp`, `Mods.cmake` and the games' headers) installs with the
engine's release package, so `ge mod build` needs no engine tree.

## The sample and the lab

`games/shooters_edge/mods/rocket_bounce/` is the sample mod, built by the tree's own CMake as a
package in `build/<preset>/mods/rocket-bounce/`. It scales a new rocket's velocity by 1.5 in the
player's post-think, and takes over a grenade's touch to count its bounces, running the game's
touch after its own; at the third bounce it lights the fuse. The `mod` lab
(`games/shooters_edge/labs/mod_lab.cpp`) loads it through the loader, plays the scripted match
without it and twice with it, and shows the loader's verdict, the two matches side by side, the
mod's log, both matches' missiles from above and from the side, the rocket's distance and the
grenade's height against the clock. It fails when the mod is refused, when the two modded runs
part, or when the mod changes nothing.

## Rows

| Row | What |
| --- | --- |
| M52a | Desktop mods on ABI 1.0, Shooter's Edge's seam, `ge mod new/build/check/pack`, the sample, the lab |
| M52b | The SDK in the release package; the sample built and loaded on three desktop OSes in CI; `ge mod check --run` |
| M52c | The store form: the WebAssembly module and the engine's interpreter; the store builds; ahead-of-time linking |
| M52d | Signing and distribution |
| M52e | A second game (Fall Line's hooks), the replay's keyframes and the network's prediction carrying the block, ABI 1.1 adding entries |
