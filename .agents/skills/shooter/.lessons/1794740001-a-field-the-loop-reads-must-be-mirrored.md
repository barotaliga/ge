---
title: A field the loop reads must be in the program adapter's mirror, or it is silently zero
date: 2026-10-31
scope: games/shooters_edge/sim/qc_program.cpp, plugins/shooter/framemove.cpp
concepts: [mirror, qc-program, view-ofs, punchangle, original-move, water-level, adapter]
---

The loop's `Edict` carries fields the original move reads (`view_ofs` for the eye's water level,
`punchangle` it decays), but the program adapter copies only the fields in `mirror_in` and
`mirror_out`. A field missing there keeps the loop's default (zero) while the program's own copy
in the VM holds the real value: the eye sat at the feet and the punch never decayed, and no test
failed because each side looked right on its own. When the loop starts reading a field, add it
to the mirror in the same change and decide its direction: the program's alone (read back,
never written: `view_ofs`), the loop's alone (`movement`), or both (`punchangle`, `fixangle`).
Then test it from the far side: set it on one side, run a frame, read it on the other.
