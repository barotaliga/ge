---
name: progs
description: "Use for plugins/progs in gameengine2: the QC virtual machine Shooter's Edge runs mods' bytecode on. The progs loader and checker (every bound and index refused with the section, record, field and value), the classic multiplayer engine-side layout (globals 28..89, the 102 fields, crc 54730), the interpreter (every opcode on 32-bit floats, the call convention, the call depth, the local stack, the run-away guard), entity, field, pointer, function and string values, temporaries, the builtin table a game fills and the extension registry, faults with a QC stack trace and lines, the profile and trace hooks, the assembler and the sample program, the qc tool behind ge qc, the qc lab, and the bytecode program on the shooter seam (games/shooters_edge/sim/qc_program); the M18c row."
---

# The progs plugin

`plugins/progs/` (`ge_progs`, namespace `ge::progs`) runs compiled QC. It depends on the engine's
core only (`ge_add_plugin(progs ENGINE core)`); what a builtin DOES is a game's, never this
plugin's. `docs/shooters-edge.md` is the design, `docs/goals.md` M18c the row, M18p the
multiplayer builtins and extensions that come next.

| File | What |
| --- | --- |
| `progs/opcodes.hpp` | Every opcode's number, name and operand kinds (one table the checker, the VM and the disassembler read) |
| `progs/layout.hpp` | The classic multiplayer engine-side layout: the global and field offsets, counted from the tables at compile time; the optional fields and hooks found by name |
| `progs/file.hpp` | `load_progs`: the file read and checked; lookups by name (first match); the line table and its `.lines` sidecar |
| `progs/vm.hpp` | The VM: memory, values, `call_function`, guards, faults (`fault_text`), strings, profile, trace hooks |
| `progs/extensions.hpp` | The extension registry: named groups of builtins, fields and hooks; `install`, `offers` |
| `progs/assembler.hpp` | Bytecode built in memory for the tests, the lab and the sample (no compiler until M18d) |
| `progs/sample.hpp` | The sample program that runs every opcode, and its five builtins |
| `progs/disasm.hpp`, `progs/tool.hpp`, `bin/qc.cpp` | The disassembler and the `qc` tool `ge qc` runs (engine/cli/qc.cpp hands to it) |
| `labs/qc_lab.cpp` | The `qc` lab: opcode throughput, the sample's profile, a fault |
| `games/shooters_edge/sim/qc_program.hpp` | A loaded progs as a program on the shooter loop's seam |

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
- **Run the lab before and after** a change to the VM: `make lab LAB=qc ARGS="--tag=before"`, then
  `--compare=before`; its digest (`make ge ARGS="digest qc --write"`) holds the sample's profile.

Read this skill's lessons first: `make lessons ARGS=progs`.
