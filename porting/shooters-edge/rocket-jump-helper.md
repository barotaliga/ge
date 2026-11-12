# The rocket jump helpers in the classic match mod (a clean-room reader's answers)

**A WORKING NOTE** (deleted with `porting/`). A reader read KTX (QW-Group/ktx, master, 2026-11-08)
and, for the order of events only, MVDSV; this is its answer in its own words, no code. Built from
it: `plugins/shooter/rjump_assist.hpp`, `games/shooters_edge/server/rjump_rules.hpp` (M18e). Marks:
read, derived.

- **Names** (read): two plain aliases the server writes into each player's client, `krjump`
  (straight down) and `kfjump` (turned round); each sends an impulse and taps jump in the same
  client frame, releasing it the next. Impulses: 164 for `krjump`, 156 for `kfjump`. Not related to
  the server-info flag `pm_ktjump`. Ours: `+rjump` and `+rrjump` send the same impulses with a jump.
- **Down** (read): in the player's post-think (after the move, so after the jump), the server's
  copy of the view pitch is set to 80 down (the default `sv_maxpitch`), yaw kept; the rocket
  launcher selected and attack forced for that one weapon step; then pitch and attack put back.
  The client's screen never turns (derived).
- **Back** (read): the yaw turned +180 at the player's own pitch, fired, turned back; same order.
- **Nothing added** (read): the ordinary fire path, ammo, refire (0.8 s), self-damage, push.
- **Failures** (read): disabled: "krjump is disabled"; no launcher: "no weapon"; no rockets: "not
  enough ammo" (and the current weapon still fires in the turned direction: a quirk we do not
  keep); reloading: silently nothing (we wait up to a second instead); within 50 ms of a respawn:
  nothing.
- **Gating** (read): `k_disallow_krjump`, `k_disallow_kfjump` (1 off; empty default reads as on;
  the shipped example config sets both off); no per-mode values; admins toggle with `tkrjump`,
  `tkfjump` (announced "<name> enables krjump"); no vote; no per-player enable; use not disclosed.
  Ours adds the vote and the per-player grant the person asked for (as the easy jump's).
- **The sequel**: neither project holds anything for it (read).
