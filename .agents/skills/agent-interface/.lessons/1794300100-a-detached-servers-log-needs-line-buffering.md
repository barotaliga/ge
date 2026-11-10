---
title: A program started into a log file buffers its output by the block; the server now writes a line at a time
date: 2026-11-10
scope: engine/server/main.cpp
concepts: [gamecli, dedicated-server, log, stdout-buffering, spawn-detached, asan]
---

`shooters_edge_cli host --dedicated` starts the server with its output in a log file. Into a file
(or a pipe) the C library buffers stdout by the 4 KiB block, so the log stayed empty for minutes
and an agent told to read it saw nothing. `stdbuf -oL` does not help a debug build: ASan refuses to
start when a library is preloaded before it. The fix is in the server (`setvbuf(stdout, nullptr,
_IOLBF, 0)` at the top of `server_main`). Any other program a CLI starts into a log needs the same,
or its log is useless until it exits. Also: a debug (ASan) server with three bots takes about a
minute to answer its first status query on e1m2, so try the hosting loop on a release build, or
without bots, before deciding the CLI's wait is wrong.
