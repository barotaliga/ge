---
title: A test that runs a game's CLI points HOME at its own folder, or the machine's settings leak in
date: 2026-11-22
scope: tests/gamecli_cli_test.cpp, engine/gamecli/gamecli.cpp, games/shooters_edge/cli/spec.cpp
concepts: [gamecli, prefs-dir, home, hermetic, sb-masters, masters, test]
---

`gc::prefs_dir` reads HOME (APPDATA on Windows, XDG_DATA_HOME first on Linux), and Shooter's
Edge's `locate` reads the series' shared settings from there even when every call passes its own
`--basedir`, `--state` and `--logs`. On a developer's Mac whose settings named `sb_masters`,
`servers --has=ctf` asked the real internet masters and listed 14 live CTF servers, so the test
failed there and passed on CI. Point HOME, XDG_DATA_HOME and APPDATA at the test's temporary
root before each call (`own_home` in gamecli_cli_test.cpp). Do the same in any new test that
reaches a per-user folder.
