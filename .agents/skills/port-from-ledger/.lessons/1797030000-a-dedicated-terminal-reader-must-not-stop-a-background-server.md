---
title: A server that reads its terminal must not read one it does not own
date: 2026-11-22
scope: engine/server/main.cpp
concepts: [terminal, stdin, dedicated-server, sigttin, background-process]
---

The dedicated server's terminal console reads standard input on a thread of its own. A process
started in the background of an interactive shell (`sesv &`) that reads its terminal is stopped by
the shell (SIGTTIN), and a server started by a test or a lab with a pipe for its input would eat
the parent's input. So it reads only when its input is a terminal it holds in the foreground
(`isatty` and `tcgetpgrp(0) == getpgrp()`; `_isatty` on Windows), and never in an offline server.
To check it black-box without a terminal of your own, run it under `script -qfc "<server>"
/dev/null` and pipe the lines into `script`.
