---
title: A function record with no code may be a builtin bound by its name, not an empty function
date: 2026-11-09
scope: plugins/progs/vm.cpp, games/shooters_edge/server/catalogue.cpp, games/shooters_edge/server/bi_message.cpp
concepts: [bind-by-name, named-builtin, first-statement-zero, rerelease, localized, dollar-key, print-key]
---

A later program declares some builtins with number 0: its function record has first statement 0,
no parameter start and no locals, and only its name (`ex_sprint`) says which builtin it is. Before
`bind_by_name`, the VM entered statement 0 (a DONE) and the call silently did nothing, so a whole
family of prints vanished without a fault. Now a no-code function runs the builtin a row binds to
its name, or faults naming the function. The same program prints text KEYS (`$qc_got_item`) with
arguments: resolve them on the server, in every print builtin AND in WriteString (the finale's
text) AND in the level's name, through `progs::localize_join_args` / `resolve_key`, never in one
client path. When a release's messages show as `$keys`, check that `QcServerOptions::localized`
was set by the host that made the server (listen, co-op, dedicated) before the parser.
