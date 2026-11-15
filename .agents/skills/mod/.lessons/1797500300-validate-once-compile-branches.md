---
title: Validate a WebAssembly body once and compile its branches, so the run loop checks nothing
date: 2026-11-14
scope: engine/mod/wasm/validate.cpp, engine/mod/wasm/interp.cpp
concepts: [wasm, validate, branch, br-table, interpreter, fuzz]
---

The validator's operand-type stack knows each block's static height, so a branch compiles to
(target instruction, values kept, height to cut back to) and the interpreter needs no control
stack or type tags: it trusts what validation proved. Hold that trust with a sweep that changes
every byte of a real clang module three ways and runs whatever still reads, under ASan; it costs
seconds. A clang C `(int)d` conversion is UB when out of range and clang emits the saturating
instruction, so a test of the trapping conversions calls `__builtin_wasm_trunc_s_i32_f64`. The
fixture is clang's output kept as bytes in a header, so no CI row needs a wasm32 compiler.
