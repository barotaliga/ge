---
name: progs
description: "Use when the game links ge::progs (`ge use progs`): running compiled QC (a progs file) in the game, loading and checking it, filling the builtin table and registering extensions, calling its functions, reading a fault's QC stack trace, profiling it, and `ge qc` to disassemble, run, trace and profile a progs headless."
plugin: progs
---

# The progs plugin

`ge::progs` (headers `progs/*.hpp`, namespace `ge::progs`) runs compiled QC deterministically: the
same file and the same calls give the same bits on every platform. The plugin is the machine; what
each builtin does is the game's.

| Header | What | Start with |
| --- | --- | --- |
| `progs/file.hpp` | `load_progs`: a progs read and checked (every bound and index, refused with a message that names the record); lookups by name | `ge explain load_progs` |
| `progs/vm.hpp` | The VM: `create_vm`, `call_function`, the globals and entities, strings, faults (`fault_text`), the profile | `ge explain call_function` |
| `progs/layout.hpp` | The classic multiplayer engine-side globals and fields by offset | `ge explain layout_global` |
| `progs/extensions.hpp` | Named groups of builtins, fields and hooks the game offers; `install` | `ge explain register_extension` |
| `progs/assembler.hpp` | Bytecode built in memory for tests | `ge explain Assembler` |

## Use it

1. Load: `load_progs(bytes, size, progs, error)`; a refusal is the player's file, so show `error`.
2. `create_vm(vm, progs)`, then the game's builtins: `set_builtin(vm, number, fn, "name")` for
   each, or `install(registry, vm)`. A builtin reads `vm.argc` arguments from `parm(i)` and writes
   the return slot (`kGlobalReturn`); `vm_error` faults the call.
3. Set the globals a hook expects (`self`, `other`, `time`) and `call_function(vm, fn)`. False is
   a fault: `fault_text(vm)` is the message with the QC stack; the game decides what it ends, then
   `clear_fault`.
4. Keep `vm.num_entities` at the entities in use: a value past it is a fault, never a stray read.

## See it

```sh
ge qc disasm <progs> [--function=<name>]               # functions as statements
ge qc run <progs> --function=<name> --args=10          # the return value or the fault and stack
ge qc trace <progs> --function=<name> [--statements]   # every call with arguments and return
ge qc profile <progs> --function=<name> --repeat=20    # statements and calls per function
```

A line table beside the file (`<progs>.lines`, one line number a statement) puts lines in faults
and listings.
