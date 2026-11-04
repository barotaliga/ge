---
title: Put what every runner keeps in a base struct, so the QC code is untouched
date: 2026-11-04
scope: games/shooters_edge/server/rules_runner.hpp, games/shooters_edge/server/qc_server.hpp
concepts: [rules-runner, rules-common, qc-runner, qc-of, hosts]
---

The hosts read about forty members of the QC server (the loop, the lists, the sink, the map
change). Moving them into `RulesCommon` and making `QcServer` derive from it (plain data, no
virtuals) left every `s.sv`, `s.models` and `s.out` inside `server/` compiling as it was, while the
hosts hold a `RulesRunner` and read `*runner` (the common part). Bots then take
`const RulesCommon&`, which a `QcServer` still binds to, so the run harnesses that own a QC server
did not change. A test or lab that owns its QC server wraps it with `qc_runner_over`, which does
not delete it. What still needs the program behind a runner goes through `qc_of` (null for another
form), so a grep for `qc_of` lists what the world does not carry yet.

The endpoint keeps the runner's address (`ClassicServer::rules`), so a runner handed to
`classic_start` or `classic_change` must already sit where it lives: move a new map's runner into
the host's member first, then hand it on (the dedicated server's map change kept a pointer to a
local runner that ASan caught as a stack use after return).
