---
title: A bit-exact picture check gates on the software adapter only, and keeps hardware out of the digest
date: 2026-10-29
scope: games/fall_line/labs/gpu_world_lab.cpp
concepts: [cull, cull-check, hi-z, flake, macos, paravirtual, metal, repeat, digest]
---

The `world` lab's cull check compares a culled still with an unculled one to the bit, which holds
on the software adapter (both draw the same surfaces at the same depths, the colour pass at
`equal`). On CI's macOS row, Apple's paravirtual Metal adapter, a difference showed on a process's
first run only, and the failure named no view, so it could not be told from a cull bug. A cull that
drops what shows drops it every time on the same frames: draw the culled still again, fail on a
difference that comes back (or on any difference on the software adapter), and warn with the view,
the trees' early, late and hidden counts and the differing pixels' box on one that does not. A repeat was
not enough: the paravirtual adapter dropped a pixel that came back within a run but not from run to
run, so a `--repeat=2` digest still moved. Gate on the software adapter alone. On hardware, measure
and warn, and attach nothing, because an attachment's NAME is in the digest even when its bytes are
`measured`. Only a note and a measured metric stay out of it.
