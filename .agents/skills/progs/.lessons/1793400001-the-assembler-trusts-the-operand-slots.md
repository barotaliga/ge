---
title: The assembler writes whatever slots it is given; NOT_* reads A and writes C
date: 2026-10-29
scope: plugins/progs/assembler.cpp, plugins/progs/sample.cpp, plugins/progs/opcodes.cpp
concepts: [assembler, operand, not, opcode-shape, golden]
---

The sample emitted `NOT_F` and friends as `(a, b)`, as if the result went in B. The result goes
in C (`op_shape`: B unused). The loader accepted the file, because every operand was inside the
globals. The VM then wrote the result into global 0, and the goldens recorded the wrong answer
until a test read the value back. When you emit an opcode by hand, read its `op_shape` first.
After a change to the sample or to an opcode, read the golden's diff line by line rather than
rewriting it.
