---
title: A raw terminal is restored on every exit path, and a game's spelling of a core setting is a synonym
date: 2026-11-22
scope: engine/server/main.cpp
concepts: [terminal, raw-mode, line-editor, console-log, setting-name, synonym, dedicated-server]
---

The dedicated process puts its terminal in raw mode (no echo, no line buffering) for the line
editor. Restore it with `atexit` and in the fatal signals' handlers, and close the editor (history
saved, line hidden) before each `return` of `run`: a missed path leaves the operator's shell without
echo. Every printed line goes through `terminal_print`, which hides the line being typed, prints,
and draws it again; a `printf` that bypasses it lands in the middle of the prompt.

Build the editor as a model that writes what the terminal shows into a string (`term_feed`), so its
keys, history and Tab are tested without a terminal; drive the real thing once with `script -qfc`
and a timed `printf` of keys.

A setting the engine's server owns (the history, the Tab listing, the console log) takes an engine
name (`con_...`) bound to `ServerConfig`; a classic game's spelling of it is a `SettingName` row in
that game's table, so it exists only while that game is served and sets the one value.
