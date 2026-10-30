# The QC virtual machine: the progs loader and checker, the interpreter, entity and global memory,
# strings, the builtin table a game fills, the extension registry, the profiler, the assembler the
# tests and the lab build bytecode with, and the `qc` tool behind `ge qc` (skill: progs). Any game
# that runs QC bytecode links it.
ge_add_plugin(progs ENGINE core)
