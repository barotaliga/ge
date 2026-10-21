# Porting: the working area

**A WORKING AREA, deleted when porting ends.** Everything in `porting/` (this guide, the
porting map, the research, the subsystem notes, the golden generators and their make
targets) exists only while modules are carried over from their reference sources, and it is
the only place those sources may be named (AGENTS.md § A clean slate). The engine, its
plugins, its games, its tests and its docs never depend on anything here except `make
golden`, which regenerates the committed tables in `tests/golden/`.

| File | What |
| --- | --- |
| `README.md` | This guide: the references, the layout and type hints, the JavaScript traps, bisecting |
| `map.md` | THE PORTING MAP: one row per engine file, the reference module it reproduces and the reference identifiers it covers. `make find` searches it; `make agents-check` holds it |
| `research.md` | What game5 needed in engine terms, the libm measurement, how established engines solve each part |
| `subsystems.md` | Per skill: which reference modules a subsystem reproduces and how to compare against them |
| `sibling-games.md` | What game2–game4 imply for the plugin boundaries |
| `milestones.md` | The porting notes behind docs/goals.md's rows (what was skipped, the speed against the reference) |
| `parity/` | The golden generators: Node scripts that run the reference and write `tests/golden/` |
| `porting.mk` | `make golden` and `make golden-libm` (included by the root Makefile while this directory exists) |

**When porting ends**: confirm no file outside `porting/` and the `port` skill names
anything in here but `make golden`, delete `porting/` and `.agents/skills/port/`, drop the
`port` rows from AGENTS.md and `.agents/skills/README.md`, and remove `porting/` and the
`port` skill from `WORKING` in `tools/agents-index.mjs`. The golden tables stay: they are
the engine's own record of its bits.

## Carrying a module over

How a TypeScript module from game5 or oss-game-framework becomes C++ here. The `port`
skill runs this procedure, and the `determinism` skill owns the numeric rules. The C++
that comes out reads as engine code written here first: no reference names in it, its
tags, its test or its commit, and the link goes in `map.md`. And no reference
architecture is assumed to carry over (AGENTS.md § C++ only, and no inherited
architecture): the layout table below is a hint for where a module MIGHT go, never a rule.

**Bit parity is optional.** Matching the oracle's bits is a tool, not a requirement: it is
the quickest proof that a carried-over module is correct, and the golden generators in
`parity/` stay for that. Where the engine does better by diverging (a cleaner structure, a
faster algorithm, a fixed bug), it diverges, rewrites the affected goldens from its own
reviewed output, and says so in the PR.

## The references

The sources being ported are **read-only**. Clone them beside this repo:

```sh
git clone --depth 1 https://github.com/niclaslindstedt/game5 ../game5
git clone --depth 1 https://github.com/niclaslindstedt/oss-game-framework ../oss-game-framework
```

A port is correct when the TypeScript and the C++ produce the same bits on the same
inputs. The TypeScript is run by `porting/parity/*.mjs` (Node with
`--experimental-strip-types` imports the `.ts` files directly), which writes golden
files to `tests/golden/`. The C++ test reads those files and compares bits. Golden
files are committed. They are regenerated only when the oracle moves, and the diff is
reviewed as a change.

**Which Node.** Transcendentals in V8 depend on its version (`porting/research.md` §2).
Golden files for anything that reaches `Math.sin`, `exp`, `pow` or similar are written
under **game5's pinned Node** (`../game5/.nvmrc`) and record the Node version in their
first line. Bit-exact functions (`+ − × ÷ √`, hypot, the PRNG) are the same on any Node.
`Math.pow` in Node 24 is the host C library's `pow`, so its tables must also come from
game5's reference platform, Linux x86-64 with glibc (`make golden-libm` refuses anything
else).

## Layout mapping

| Oracle | Port |
| --- | --- |
| `oss-game-framework/src/core/<x>.ts` | `engine/core/<x>.hpp` (+ `.cpp` when it is not small) |
| `oss-game-framework/src/racing/<x>.ts` | `plugins/racing/<x>.hpp/.cpp` |
| `oss-game-framework/src/loop/run-clock.ts` | `engine/loop/run_clock.hpp` |
| `oss-game-framework/src/audio/*` | `engine/audio/*` |
| `game5/engine/mapgen/<x>.ts`, `engine/analysis/<x>.ts` | `plugins/mountain/<dir>/<x>.hpp/.cpp` |
| `game5/engine/<dir>/<x>.ts` (any other) | its home by the `plugins` skill: `plugins/<plugin>/<dir>/<x>.hpp/.cpp`, or `games/fall_line/sim/<dir>/<x>.hpp/.cpp` when it is Fall Line's alone |
| `game5/pwa/src/game/<x>.ts` | `games/fall_line/app/<x>.hpp/.cpp` (or `engine/render/` when generic) |
| `game5/tests/<topic>_test.ts` | `tests/<topic>_test.cpp` (a parity test plus the behavioural asserts that matter) |
| `game5/scripts/<lab>.mjs` | `tools/<lab>/` (C++ CLI) when the lab is a measuring tool |

File names use `snake_case`, and kebab-case TS names map directly: `run-start.ts`
becomes `run_start.hpp`. The port file's first comment says what it owns in the engine's
terms; its oracle file goes in its `map.md` row.

## Type mapping

| TypeScript | C++ |
| --- | --- |
| `number` (a measurement) | `double` (never `float` unless the TS stores into a `Float32Array`) |
| `number` used as an int with `\| 0` / `>>> 0` / `Math.imul` | `int32_t` / `uint32_t`, through `core/num` helpers when the value can leave the int range |
| `Float32Array` element | `float` storage; read back as `double` (a JS read widens exactly) |
| `Math.fround(x)` | `static_cast<double>(static_cast<float>(x))` |
| `type X = {…}` (data) | `struct X {…};` POD, field order as in TS |
| `readonly T[]` built once | `std::vector<T>` reserved, or `std::span<const T>` to read |
| union of string literals | `enum class` (`uint8_t`), with names kept exactly |
| `T \| undefined` / optional field | `std::optional<T>` or a `has_x` flag, whichever is hot-path-free |
| `Map` / `Set` | an insertion-ordered container (`std::vector` of pairs plus an index) whenever anything iterates it |
| closure returned as an object (`createRng`) | a struct of state plus free functions (`core::Rng`, `next(r)`) |

## JavaScript semantics that bite (each one has a `core/num` helper)

1. **`Math.round(x)`** rounds half toward +∞ (`round(-2.5) == -2`), and `x + 0.5` then
   `floor` is wrong at `0.49999999999999994`. Neither `std::round` nor `floor(x + 0.5)`
   matches it. Use `num::round`.
2. **`x | 0`** is ToInt32: NaN and ±∞ become 0, and anything else wraps modulo 2³². A C++
   cast of an out-of-range double is undefined behaviour. Use `num::to_int32` and
   `num::to_uint32`.
3. **`Math.max` / `Math.min`** propagate NaN and order −0 below +0. `std::max` does
   neither. Use `num::max` and `num::min` whenever an input can be NaN or a signed zero.
   When it cannot, a plain compare is fine, and the code should say why.
4. **`Math.sign`** returns ±0 and NaN as they are. Use `num::sign`.
5. **`%`** on doubles is `std::fmod` (exact, with the sign of the dividend), not
   `std::remainder`.
6. **Transcendentals** (`Math.sin`, `cos`, `tan`, `atan`, `atan2`, `asin`, `acos`, `exp`,
   `expm1`, `log`, `log2`, `pow`, `cbrt`, `tanh`): only `core::libm`, never `<cmath>`
   (`tools/check-libm.sh` fails the build's CI otherwise). Those fourteen are every one
   game5's engine calls. A port that meets another (`Math.log10`, `sinh` …) ports it into
   `core/libm` from the same upstream first (`determinism` skill).
   `Math.sqrt`, `abs`, `floor`, `ceil` and `trunc` are exact and may use `<cmath>`.
7. **`Math.hypot`**: `core::hypot` / `hypot3` / `hypot4` (done, bit for bit).
8. **`**`** is `Math.pow`, so it goes through `core::libm::pow`, even for `x ** 2`.
   (Node 24 answers `x ** 2` with `x * x` and `x ** 0.5` with `sqrt(x + 0)`; `libm::pow`
   does the same, so writing `x * x` by hand is also exact for a constant 2.)
9. **`Array.prototype.sort`** is stable, so use `std::stable_sort` with the same
   comparator. With no comparator, JS sorts **as strings**, so port that faithfully
   if it occurs.
10. **Object and `Map` iteration order** is insertion order (integer-like object keys
    first, in ascending order). Never iterate an `unordered_map` to produce a result.
11. **`toFixed`, `String(n)` and template literals** used inside a hash or a key: port
    the exact formatting. (game5's engine uses `toFixed` only in diagnostic messages and
    `toString(16)` only on the uint32 digests, so M1 ships no helper for it.) `std::to_chars` gives the shortest round-trip digits, but JS
    switches to exponent form at 1e21 and below 1e-6.
12. **Integer division**: `Math.floor(a / b)` is a double divide followed by a floor.
    Keep it in double; do not turn it into C++ `int / int` (which truncates).
13. **`Number.EPSILON`, `MAX_SAFE_INTEGER`, `Infinity`** map to `std::numeric_limits`
    equivalents. Check that every use means the same thing.
14. **Evaluation order**: JS evaluates operands left to right, and C++ does not
    guarantee that for function arguments. If two arguments each draw from the same
    `Rng` (`f(next(r), next(r))`), hoist the draws into named locals in TS order.

## Parity levels

1. **Unit golden**: a function's outputs on a table of inputs (`tests/golden/<x>.txt`).
2. **Structure golden**: a built `Level` dumped field by field as hex (M4).
3. **Digest**: game5's own `levelDigest` and sim digests. They are coarse, rounded to
   centimetres, so they are a gate and not a proof.
4. **Trajectory**: a run's state hashed at every step (M5). The first diverging step
   names the system to look at.

Port **bottom-up** in the order the oracle's dependencies run (core before mapgen,
mapgen before game), and land each module with its golden test. Do not port a module
whose dependencies have no parity yet.

## Bisecting

When a golden row fails, dump intermediate values as hex from both sides: add a temporary
print to a copy of the oracle module under `porting/parity/` (never to the oracle repo),
run the same inputs through the C++, and find the first value that differs. A one-ULP
difference is a contraction or a libm call; a large one is logic or ordering.

## When the oracle is wrong

Do not fix it here. The port reproduces it, including its bugs. The engine-side comment
states the behaviour as a fact about the engine; that it is the oracle's bug is recorded
here or in the file's `map.md` row. Tell the user so it can be fixed upstream by whoever owns
game5. Once game5 changes, the golden files are regenerated and the port follows.
