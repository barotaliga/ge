---
type: Added
title: ge is the agent's whole inner loop
---

`ge` now builds, tests, checks and grows a game repo on its own: every command answers in one shape (text, or one JSON object with `--json`) whose findings say what failed, where and what to do next; `ge help --json` lists the whole catalog; `ge find` and `ge explain` search the repo's and the engine's tags; `ge build`, `ge test`, `ge check` and `ge lab` turn compiler, CMake, linker, format, test and lab output into findings and keep the full logs; `ge use <plugin>` links a plugin and syncs its skill; `ge add system|lab|test` writes a working, tagged piece to start from. New consumer skills: `ge`, `racing`, `mountain` and `skier` (a plugin's skill arrives with the plugin), and a scaffolded repo's CI runs `ge check`.
