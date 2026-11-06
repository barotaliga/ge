---
type: Added
title: A VM for compiled game modules (.qvm)
---

`plugins/progs` runs a game module compiled to the `.qvm` stack machine, the form a match mod ships as: `load_qvm` checks the header, every opcode and the load-time rules (each refusal names the rule and the instruction), and `qvm_call` runs an export with every load, store and block copy checked, system calls that may call the module again, and faults that name the instruction and the frames above it. A real match mod's module loads and its start-up export runs. The `qvm` lab charts the sample module's instructions by size, times the interpreter, and reads a player's own module with `--file=`.
