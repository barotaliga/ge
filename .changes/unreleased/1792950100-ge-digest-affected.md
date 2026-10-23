---
type: Added
title: ge digest --affected runs the digests of the labs a change reaches
---

`ge digest --affected [--base=<ref>] [--list]` checks the digests of only the labs that the change since the base reaches. It uses the same include-graph reach as `ge test --affected`, but looks at each lab's own source object instead of the whole labs executable. A plugin change therefore reaches every game lab that includes the game library's headers. A lab reads a committed asset (a model under `<root>/assets/<name>/`) by path, so a changed asset reaches the labs under that plugin or game and the labs whose source names it. A baseline row that the change rewrote reaches its lab, and a changed build file reaches every lab. `--list` names the labs and runs nothing. The engine's `make precheck` takes its labs from it, and `tools/labs-reached.sh` is retired. Every registered lab now records its source file (`ge::lab::lab_source`), and `labs --json` lists it.
