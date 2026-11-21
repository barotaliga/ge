---
title: The probes that find a cause fast, and the traps of the client-fed world, the lab binaries and a rebase
date: 2026-11-01
scope: games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/net/qw_team.cpp, games/shooters_edge/net/qw_mirror.cpp, plugins/shooterbots/features.cpp, games/shooters_edge/bots/fighter.hpp
concepts: [probe, trace, client-fed, mirror, bisect, loc-file, crash, stack-size, asan, digests, feature-names, labs-binary, stacked]
---

- Probes, reverted before the commit: a `GE_TMP_*` fprintf in `pilot_step` boxed to one place on a
  1-minute seed; `#define go_to(f, n) go_to_at(f, n, __LINE__)` to name each goal change's call
  site; `sort | uniq -c` over the lab's last notes for players stacked at one point. se-team's
  `keys` column (`keys_by`) names the writer of the keys.
- `--client-fed=1`: the hook's world is the client's mirror. Horizontal speed there is near true;
  vertical speed and the takeoff are not, and the server's world at the hook is a link's delay
  behind: measure flights off the recording, joined by the clock (`flights_by_mode`). Every other
  player is `kSolidNot` with health 0 or 100: filter by `free` and `health`.
- A frag drop after a rebase may be content: bisect by picking main's commits onto the old head in a
  worktree, halves at a time (a no-op half is bit-identical by seed). A bundled `locs.pk3` had given
  se-team real loc files. Check what a run reads (locs, packs, maps, the mod's rules) before blaming
  code.
- A crash only in a multi-seed set is a worker thread's stack: reproduce on one seed with `ulimit -s
  512`, then ASan.
- Lab binaries: copy one aside (`/tmp/labs-<tag>`) for a long set, as a rebuild mid-loop changes
  what is judged; copy each A/B binary anew (macOS kills one copied over a binary it ran, exit 137);
  time cost with one variant a call. `while pgrep -f x` from a shell whose command holds `x` never
  ends.
- `kNames` is sized by `kFeatures`: a missing name compiles and crashes (`features.cpp`
  static_asserts the last). A new lab table moves the lab's digest.
