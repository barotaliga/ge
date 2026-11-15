---
title: Try starting a server as a process on gesample, the sample game's own binary
date: 2026-11-15
scope: engine/server/spawn.cpp, engine/server/process.cpp, engine/server/bin/sample_main.cpp, engine/lab/labs/server_launch_lab.cpp, tests/server_spawn_test.cpp
concepts: [spawn, dedicated-server, gesample, ping, advert, state-folder, ge-sample-server, loopback]
---

Starting a dedicated server as its own process needs a real binary. A game's binary (`sesv`) drags
the whole game into an engine test, so the sample game has its own: `gesample`, built by
`ge_add_server` in `engine/server/module.cmake`. The test and the lab look for it beside their
own binaries (`self_dir`, `program_path`). Don't hand them its path by a define in the root or
the tests' CMakeLists: that edit makes every lab and test "reached", and precheck then runs all
of them (it ran out of memory here). To tell whether a started
server is up, ping its session port with the LAN probe (`server_answers`). The advert that comes
back carries the cfg's name and the seats, which proves the cfg was handed over, not just that a
process runs. Give every test and lab its own loopback port (27651, 27652): ctest and
labs-smoke run side by side. The discovery port is shared, but a second server only logs that
it could not open it and runs on.
