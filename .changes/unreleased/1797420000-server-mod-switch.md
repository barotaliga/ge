---
type: Added
title: A dedicated server switches its mods while it runs
---

A dedicated server started with `--commands=PATH` runs each console line added to that file, and `engine/server/spawn.hpp` hands every server it starts such a file (`hosted_command` writes to it). `mod arena,ctf` on that console resets every server value, remounts the search path, reads the `server.cfg` layers again with the command line still winning (`server_switch_mods`), and restarts the match under the new settings on the same port. A mod whose cfg refuses leaves the mods as they were. The `server-launch` lab now switches the sample server's mods before it stops it.
