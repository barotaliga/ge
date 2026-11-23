---
title: A recipe's sleep held the menu's own console lines, so the page never asked its server
date: 2026-11-23
scope: engine/console/console.cpp, games/shooters_edge/menu/server_page.cpp
concepts: [recipe, sleep, console-buffer, execute, serversheet]
---

The first recipe opened SERVER SETTINGS and read "Not on a server" while joined. The page asks for
its sheet with `con.execute("serversheet open")`, which appends to the buffer, and the buffer was
held by the recipe's `sleep`, so the ask waited behind it. `Console::execute` now runs the host's
line past a sleep and leaves the held commands where they were. When a recipe shows a page empty
or stale, check first whether code it drives talks through the console.
