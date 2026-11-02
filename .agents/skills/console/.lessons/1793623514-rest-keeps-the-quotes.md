---
title: Args::rest keeps the quotes the player typed; take a[1] for one quoted argument
date: 2026-11-02
scope: engine/console
concepts: [args, rest, quotes, path, command-argument]
---

A command taking a path read `a.rest(0)` and got `"~/Games/classic"` with its quotes, which no
folder matched. `a[i]` is the lexed word, unquoted; `rest` is the raw text from that word on, for
lines passed on whole (`sv <line>`). A command whose one argument may hold spaces takes
`a.size() == 2 ? a[1] : a.rest(0)` (client/main.cpp's `basedir`).
