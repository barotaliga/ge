---
type: Added
title: ge digest, ge golden diff and ge golden write
---

`ge digest [<lab>] [--write]` runs a lab (or every lab) at its smoke flags twice and sets each digest beside `tests/labs_digests.txt`; `--write` rewrites that lab's row alone, or writes the file in a game repo that has none, and from then on the repo's `labs_smoke` test holds every lab to it. `ge golden diff [<golden|test>...] [--base=<ref>]` compares golden files with a git ref row by row, naming each column that moved and in how many rows, with a double's bits shown as the double. `ge golden write <test>` runs a test with `GE_GOLDEN_WRITE=1`, runs it again to hold what it wrote, and answers with that diff. The labs runner's `--write-digests` on one lab or a shard now keeps the other rows.
