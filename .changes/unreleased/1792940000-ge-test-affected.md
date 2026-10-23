---
type: Added
title: ge test --affected runs only the tests a change reaches
---

`ge test --affected [--base=<ref>]` runs only the tests that the change since the base (default origin/main) reaches, and builds only their targets. A target is reached when one of its objects includes a changed file. It is also reached when it includes a header from a directory that a reached library compiles in, so a change in a plugin reaches the game library using its headers and the game's tests. The labs' smoke slices run when the labs are reached or the digests change, and a changed golden runs its test. A changed build file runs everything. `--list` names the tests and runs nothing. `ge lint` reads the same reach (`cli/reach.hpp`). `ge test` now runs CTest in parallel.
