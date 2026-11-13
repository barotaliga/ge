---
title: A frag drop after a rebase may be the content, not the code; a crash only in a set is a worker thread's
date: 2026-11-13
scope: games/shooters_edge/bots/mind.cpp, games/shooters_edge/labs/se_team_lab.cpp, plugins/shooter/pilot.cpp, plugins/shooter/pilot_gaps.cpp
concepts: [bisect, loc-file, place-names, placeheard, crash, asan, stack-size, gap-trick, lava, gaplava]
---

- Bisect a behaviour change by picking main's commits onto the OLD head in a separate worktree
  (rerere replays the resolutions), halves at a time, and compare per-seed frags: a half that
  changes nothing is bit-identical seed by seed, so one 12-seed run settles it. Here the drop was
  main mounting the install's bundled client folder, whose `locs.pk3` gave se-team real loc files:
  the old binary given that file (`--locs=`) reproduced the drop exactly. Check the content a run
  reads (locs, packs, maps) before blaming code.
- A loc file names a room by many points, and names repeat across rooms: a heard place resolved
  as the middle of every point of its name lands in no room. Resolve it round where the speaker
  was last known (`placeheard`).
- A crash only in a multi-seed set (each seed alone passes) runs on a worker thread: reproduce it
  on one seed with `ulimit -s 512` (a worker's stack on macOS), then run that seed under the debug
  build's ASan for the line. Here it was a read past a vector's end whose garbage only crashed on
  some heaps; the macOS crash report (~/Library/Logs/DiagnosticReports) plus `objdump` at the
  offset named the access (a 40-byte NavNode by a bad index) before ASan confirmed it.
- dm2's gap tricks are ALL over lava, and `gapbuild`'s curve skipped every gap over lava: check a
  map's gaps' `harm` column in se-reach's table before reading why none is jumped.
- Count lava deaths by the map's contents at the feet (se-team trace `liquid` -5), not by height:
  dm2's YA room has floors as low as its lava.
