---
type: Added
title: A game starts, finds and stops its dedicated server
---

`engine/server/spawn.hpp` starts a game's dedicated server as its own process with a cfg handed over, keeps a record of it in the game's state folder, finds it again later from that record and a ping of its port, and stops it. The process helpers (`spawn_detached`, `process_alive`, `process_stop`, `program_path`, `self_dir`) moved from `engine/gamecli/process.hpp` to `engine/server/process.hpp`. The sample game now has a dedicated binary, `gesample`, and the new `server-launch` lab starts it, joins it, leaves it running, finds it from a second program and stops it.
