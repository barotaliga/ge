---
title: A page's own letter keys arrive as MenuInput::letter; set a filter through its cvar
date: 2026-11-15
scope: engine/ui/menu.hpp, engine/platform/window.hpp, games/shooters_edge/menu/servers_page.cpp, games/shooters_edge/client/main.cpp
concepts: [menu-input, letter-key, filter, cvar, server-browser, archive]
---

A menu page that wants its own keys (the server browser's F, E, M and P filters) reads
`MenuInput::letter`, which the window sets only for the letters some page claims; W, A, S and D
stay navigation. The window fills it with `InputSnapshot::tapped_letter("efmp")`, which reads the
letters by their HID usage, so no new `Key` entry is needed; add a page's letter to that string. When the key changes a setting, set the cvar through the console
(`Cvars::set(..., Source::local)`) rather than the bound field, so the change reaches the config
and any watcher exactly as typing it would; fall back to the field only where no console is
bound (a test).
