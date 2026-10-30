---
title: Count a mod's deaths from its frag log, not from the hooks the server calls
date: 2026-10-29
scope: games/shooters_edge/server/mod_match.cpp, games/shooters_edge/labs/se_mod_lab.cpp
concepts: [hooks, respawn, frag-log, logfrag, mod-match, report]
---

The first report counted a death each time the server called PutClientInServer. The test mod
respawns a killed player by calling PutClientInServer itself, from T_Damage. That is a QC call,
not a hook, so the hook log never saw those respawns and showed no deaths. A mod's own rules are
only visible in what the mod writes: the report now reads deaths and frags from the `logfrag`
lines (the killer and the victim on each line). The lab's timeline caption says why no respawn
dots appear. Before you measure a mod by the hooks, ask whether the mod can do the same thing
from QC.
