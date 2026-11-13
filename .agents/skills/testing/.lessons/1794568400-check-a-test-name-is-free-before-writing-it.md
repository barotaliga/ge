---
title: A new test's file name may already belong to another subject; look before writing it
date: 2026-11-13
scope: tests/shooters_edge_talk_test.cpp, tests/shooters_edge_classic_talk_test.cpp
concepts: [test-name, collision, overwrite, naming, make-find]
---

The one-name rule gave `games/shooters_edge/net/classic_talk.hpp` the test name
`tests/shooters_edge_talk_test.cpp`, and writing it replaced the bots' team-message test that
already had that name (it tests `bots/talk.hpp`). The build stayed green, so nothing flagged the
lost tests. Before you write a new test file, `ls tests/<name>_test.cpp` (or `make find Q=<word>`);
when the name is taken, name the test after the file's full stem (`shooters_edge_classic_talk`).
