---
title: The baseline mod's QC is compiled into the build; a QC edit moves goldens and many digests
date: 2026-11-09
scope: games/shooters_edge/rules, games/shooters_edge/client/held.cpp, games/shooters_edge/view/hand.cpp
concepts: [baseline-mod, jump-sound, qc, digests, golden, death, held-weapon, before-after]
---

The arena variant a window or lab runs without a pack `progs.dat` is built from
`games/shooters_edge/qc` at build time, not read at run time: stashing the QC and rerunning a lab
shows the NEW behaviour. A true "before" for a mod change is `ge lab before <lab> --ref=origin/main` (the labs built at that ref in a scratch worktree, tag `before`), then `--compare=before`. Any sound or
flag the mod adds (the jump's sound, `FL_JUMPRELEASED` on the player) moves
`tests/golden/shooters_edge_team.txt` and the digests of every lab that plays the arena (about 30:
se-audio, se-baseline, se-demo*, se-team, se-hop ...): run `ge digest --affected --write` once and
say why in the PR. `se-client --death=1` films it on a real
map from the shareware pack (`classicenhance run <the shareware release zip> --only=pack`).
