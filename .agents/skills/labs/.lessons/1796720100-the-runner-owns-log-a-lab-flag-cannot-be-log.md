---
title: The lab runner owns --log (the log channels): a lab's own flag named log is never seen
date: 2026-11-10
scope: engine/lab, games/shooters_edge/labs/se_why_lab.cpp
concepts: [lab-flags, log-spec, runner, flag-name, se-why]
---

`labs <lab> --log=<path>` fails before the lab runs ("log spec item ... has no verbosity"):
the runner reads `--log` as the log channels' spec (`--log=why=debug`), whatever the lab's
LabSpec declares. Name a lab's file flag `--file` (se-why, se-people, se-rj) or after what it
holds, never `log`.
