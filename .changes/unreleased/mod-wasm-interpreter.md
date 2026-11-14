---
type: Added
title: The engine's WebAssembly interpreter for mods
---

`engine/mod/wasm` reads a WebAssembly module (the binary format clang writes for `--target=wasm32`), validates every function and runs it in an interpreter with no JIT. The interpreter runs on an instruction budget, has a bounded stack and bounded memory, names every trap, and gives every NaN an operation makes the same bits on every platform. A host offers its functions as imports or puts them in the module's function table. This is the first part of the store form for mods (M52c), for platforms that may not load native code.
