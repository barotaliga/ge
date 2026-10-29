---
type: Changed
title: One digest file a lab
---

The lab digest baseline is now a directory, `tests/digests/<lab>.txt`, holding one file per lab, in place of the single `tests/labs_digests.txt` of rows. Pull requests that move different labs no longer conflict in the merge queue, which ignores the union merge driver the single file depended on. `labs --check-digests` and `--write-digests` take the directory; a path ending in `.txt` is still read and written in the single-file form. `ge digest` reads and writes the directory, `ge digest --affected` reaches a lab through its moved file, and a new game repo's `labs_smoke` case checks `tests/digests/`.
