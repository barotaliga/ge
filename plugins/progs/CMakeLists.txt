# The QC virtual machine: the progs loader and checker, the interpreter, entity and global memory,
# strings, the builtin table a game fills, the extension registry, the profiler, the assembler the
# tests and the lab build bytecode with, and the `qc` tool behind `ge qc` (skill: progs). Any game
# that runs QC bytecode links it. Its `cli/` runs that tool inside a game's player CLI for the
# toolkit's qc rows (engine/toolkit/qc.hpp).
ge_add_plugin(progs ENGINE core toolkit)
