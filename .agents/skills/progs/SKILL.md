---
name: progs
description: "Use for plugins/progs in gameengine2: the QC virtual machine Shooter's Edge runs mods' bytecode on. The progs loader and checker (every bound and index refused with the section, record, field and value), the classic multiplayer engine-side layout (globals 28..89, the 102 fields, crc 54730), the interpreter (every opcode on 32-bit floats, the call convention, the call depth, the local stack, the run-away guard), entity, field, pointer, function and string values, temporaries, the builtin table a game fills and the extension registry, faults with a QC stack trace and lines, the profile and trace hooks, the assembler and the sample program, the QC compiler (compile_qc, ge qc build) and the language it reads, the qc tool behind ge qc, the qc lab, and the bytecode program on the shooter seam (games/shooters_edge/sim/qc_program); the standard builtins (stdlib: math, vectors, ftos and vtos text, tokenize, the string extensions, strzone), the map's entity text, and the mod server (games/shooters_edge/server: the builtin catalogue by number, checkextension, the hooks, the extension fields, loading a progs from --progs or sv_progs, the assembled test mod, a match with bots, the se-mod lab); the baseline deathmatch mod in QC (games/shooters_edge/qc, its arena, the se-baseline lab); the M18c, M18d and M18p rows."
---

# The progs plugin

`plugins/progs/` (`ge_progs`, namespace `ge::progs`) runs compiled QC. It depends on the engine's
core only (`ge_add_plugin(progs ENGINE core)`); what a builtin DOES is a game's, never this
plugin's, except the standard builtins that touch no world (`stdlib.hpp`). `docs/shooters-edge.md`
is the design, `docs/goals.md` M18c, M18d and M18p the rows.

| File | What |
| --- | --- |
| `progs/opcodes.hpp` | Every opcode's number, name and operand kinds (one table the checker, the VM and the disassembler read) |
| `progs/layout.hpp` | The engine-side layouts by program form (`Layout`: multiplayer 54730, single-player 5927): each a table, its offsets resolved by name (`g`, `f`, -1 where absent); the multiplayer constants `kG*`/`kF*`; the optional fields and hooks found by name; `layout_declarations` (a form's declarations as QC source, so a test program compiles to that form) |
| `progs/file.hpp` | `load_progs`: the file read and checked; lookups by name (first match); the line table and its `.lines` sidecar |
| `progs/vm.hpp` | The VM: memory, values, `call_function`, guards, faults (`fault_text`), strings, profile, trace hooks |
| `progs/extensions.hpp` | The extension registry: named groups of builtins, fields and hooks; `install`, `offers` |
| `progs/assembler.hpp` | Bytecode built in memory for the tests, the lab and the sample: each opcode pinned with no compiler between |
| `progs/qc_compile.hpp` | The QC compiler: `compile_qc` (sources in order), `compile_project` (a `progs.src`); `qc_lexer`, `qc_compiler.hpp` (its state), `qc_compile.cpp` (declarations, the layout), `qc_statements.cpp`, `qc_expressions.cpp` |
| `progs/sample.hpp` | The sample program that runs every opcode, and its five builtins |
| `progs/disasm.hpp`, `progs/tool.hpp`, `tool/main.cpp` | The disassembler and the plugin's tool, the `progs` executable `ge qc` runs (engine/cli/qc.cpp hands to it) |
| `labs/qc_lab.cpp` | The `qc` lab: opcode throughput, the sample's profile, a fault |
| `progs/text.hpp` | Numbers as QC text: `ftos` and `vtos` (the shortest form that reads back; at or past 1e21 the exponent form) |
| `progs/stdlib.hpp` | The standard builtins that touch no world: math, vectors, random, text, `tokenize`/`argc`/`argv`, the string extensions, `strzone` |
| `progs/entity_text.hpp` | The map's entity text parsed and spawned into a progs' fields (typed values, `angle`, `light`, `_` keys, the inhibit flags) |
| `games/shooters_edge/sim/qc_program.hpp` | A loaded progs as a program on the shooter loop's seam; `before_call` / `after_call` for the server |
| `games/shooters_edge/server/` | The mod server: `catalogue.hpp` (every builtin by number, the extensions), `bi_*.cpp` (the game's builtins), `messages.hpp` (the Write family and multicast), `commands.hpp` (client commands, chat, info, the console, pausing), `qc_server.hpp` (the hooks, clients, maps), `campaign.hpp` (the campaign host: a single-player program's maps in a chain, the parms and `serverflags` carried, `changelevel` / `restart` / `map`), `test_mod.hpp` (with the campaign stub, compiled from `layout_declarations`), `baseline_mod.hpp` (the baseline mod compiled from source, its arena's entity text), `mod_match.hpp` (a progs played by bots), `labs/se_mod_lab.cpp`, `labs/se_baseline_lab.cpp` |
| `games/shooters_edge/qc/` | The baseline deathmatch mod in QC, its README the modder's guide |

## Rules

- **The specification is `porting/shooters-edge/progs.md`** while the port lasts. Read it, never
  another implementation. Where it marks **Ours**, this VM checks what the classic server did not;
  keep those checks.
- **Floats are 32-bit and in the stated order.** Every arithmetic opcode works in `float`, a
  vector's components one at a time, the dot product `(x + y) + z`, and a NaN result stored as the one quiet NaN `0x7fc00000`
  (which NaN the hardware hands back differs by CPU and operand order); only STATE widens (`time`
  to double, plus 0.1, rounded back). Truth is `!= 0` except IF and IFNOT, which test the RAW
  word (so -0 is true there). BITAND and BITOR truncate toward zero, and out of range or NaN is
  -2147483648. Math builtins call `core/libm`. `tests/golden/progs_ops.txt` holds every result's
  bits over awkward inputs: a deliberate change rewrites it (`GE_GOLDEN_WRITE=1`) and says why.
- **Every value is checked where it is used** (an entity value on the grid and in use, a field
  inside the entity, a pointer inside the entities in use, a function index, a string value), and
  a bad one is a fault with the stack, never a read of the wrong memory. The loader refuses what
  can be refused before running (opcodes, global operands, jumps, functions' ranges, names).
- **No allocation in a call.** `create_vm` sizes the globals, the entities, the stacks, the
  engine's string store and the profile; a builtin that makes text uses `temp_string` (the ring
  of 8 is the contract) or `engine_string` (held until `clear_engine_strings`).
- **The run-away guard is per engine call**: 99999 statements run, the 100000th faults; a builtin
  calling back into QC starts its own count on the same stacks (32 functions deep, 2048 local
  words).
- **A fault ends the engine call, not the VM's memory.** The stacks unwind to the call's entry and
  the VM refuses calls until `clear_fault`; what a fault ends (the map, the match) is the game's.
- **Builtins by number are the game's table** (`set_builtin`, or `install` from the extension
  registry). The plugin installs none; the sample's five exist only for the sample.
- **On the shooter seam** a bytecode program is mirrored: the loop's fields of every entity in use
  are copied into the VM's entities before a call and back after, and a builtin that reaches the
  server copies them out and in around it (`qc_program.hpp`). Entity numbers on the loop's side,
  entity values (bytes) on the VM's. The program's own fields live only in the VM, so a removed
  entity keeps them under the progs' free rule (the listed fields cleared, nextthink -1) while the
  loop resets its record whole; `live` says which records the VM still mirrors.
- **Three string rings, one zone.** A hook's text is the ring of 8 (`temp_string`); a builtin's
  text result is the ring of 64 (`builtin_temp_string`), so a mod may hold several `ftos` results
  in one expression; `strzone` keeps a copy in one of 1024 zone slots until `strunzone` or the
  map's end. Each kind has its own value range (`kTempBase`, `kBuiltinTempBase`, `kZoneBase`).
- **The builtin table is 1024 wide for the game** (`sim::kQcBuiltins`): the extensions sit past
  512 (`setpause` 531, `precache_vwep_model` 532). `install_catalogue` refuses a row past the
  table, because an install that drops a number silently is a mod that faults at its first call.
- **Field arguments are raw words.** A builtin given a field (`find`'s second argument) reads the
  word as an int offset; a test passes it with `qcfix::raw()`, never as a float.
- **Mirrored fields are set inside a call.** The adapter copies the loop's fields into the VM at
  every call, so a test that writes `health` or `maxspeed` between calls is overwritten: use
  `Fixture::in_call`.
- **The language is `porting/shooters-edge/qc-language.md`**, never another compiler. The
  compiler is one pass: a name is declared before it is used (a prototype for mutual calls).
  Each function's parameters, locals and temporaries are one contiguous block of globals at
  operands past `kLocalBase`, relocated when the function ends (the VM saves the block on a
  call, so recursion keeps them); temporaries free at the start of each statement. A field after
  `.` is looked up at file scope, so a parameter named like a field does not hide it. `&&` and
  `||` evaluate both sides; a call evaluates every argument, then fills the PARMs.
- **Game code reads the program's form, never the constants.** Every engine global and field the
  adapter, the server's builtins, the network and the bots touch is `q.layout->g` / `->f` (or
  `vm_layout(vm)` in a builtin): the single-player form shifts most offsets. The `kG*`/`kF*`
  constants are the multiplayer form's, for the test mod and the tests that build multiplayer
  programs. A third form is a third table in `layout.cpp` (`kLayouts`), and nothing else moves.
- **The layout decides the crc.** The engine-side globals and fields are checked by name, type
  and offset; crc 54730 is written only when they all match (a program that declares
  `end_sys_fields` and does not match gets a warning). The globals end with 2 spare words,
  because RETURN reads 3. `ge qc run` takes a crc 0 program as it stands.
- **The output is the text's alone.** No time, path or hash order reaches the bytes:
  `tests/progs_qc_compile_test.cpp` builds twice and compares, and `tests/golden/progs_qc_compile.txt`
  holds the listing of every construct. A deliberate change rewrites it and says why.
- **The baseline mod is game logic, not a test fixture.** Change it as a modder would (its
  README), keep its arena's entity text (`baseline_mod.cpp`) placing every entity it spawns, and
  run `make lab LAB=se-baseline` before and after; `tests/shooters_edge_baseline_mod_test.cpp`
  holds it to a clean spawn, a deathmatch, the door, the plat and a replay.
- **Run the lab before and after** a change to the VM: `make lab LAB=qc ARGS="--tag=before"`, then
  `--compare=before`; its digest (`make ge ARGS="digest qc --write"`) holds the sample's profile.
  A change to the server or a builtin runs `make lab LAB=se-mod` the same way.

Read this skill's lessons first: `make lessons ARGS=progs`.
