---
title: A line a player reads is a game's words; the prompt shape is the log's
date: 2026-11-11
scope: games/, engine/console, engine/server, engine/hud, engine/cli/diagnostic.cpp
concepts: [player-text, diagnostics, console, refusal, next-hint, voice-agent]
---

The diagnostics convention (what, where, `next:`) leaked into console answers, chat replies and
server refusals, and a player read "addbot: coop is off; next: `coopgame`". Agents read the log,
labs and CLIs; people read the game. Write a player's line as a sentence with the command quoted
("Type \"coopgame\" to start one."), and say a developer-only bug as "the game is built wrong:
...". `tests/cli_diagnostics_test.cpp` fails `next:`, `skill:` or paired backticks in any literal
outside the log calls in the player directories; a lab's harness or a tool's main opts out with
`// voice: agent (why)`. Tests that matched the old wording (`said(l, "next: ...")`) match the new
sentence instead.
