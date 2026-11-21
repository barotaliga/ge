---
title: A team call on an enemy waits on what the bots believe of him: measure the belief first
date: 2026-11-21
scope: games/shooters_edge/bots/mind.cpp, games/shooters_edge/bots/temper.cpp, games/shooters_edge/net/qw_team.cpp
concepts: [rlgang, rlhunt, rlknown, foe-has-key, belief, kill-feed, obituary, gate, push, lapsed, zsh]
---

- `rlgang` (the team's push on the launcher holder's area) ran 2.75 times a match. Its gates
  (`team.rlgang.<gate>`, counted with the switch off) put it on the belief: past strength, role
  and the push under way, nine thinks in ten found nobody believed to carry the launcher. The
  belief against the server's truth (`team.rl-known.*`) said why: a carrier was known 10 % of the
  time, because `foe_has_key` forgot him 10 s after the last sound of him.
- People know the carrier from the kill feed and remember him until he dies. `rlknown` reads the
  feed's rocket lines (the killer is the name that is not the victim; "rips" names him first) and
  keeps the belief until his death shows: 10 -> 55 %. Every check of "his launcher" must read
  `foe_has_key`: two inline copies of the old rule (item-first, chase) were left behind at first.
- A behaviour judged on a wrong belief is judged again once the belief is fixed (`rlhunt`: rare
  before, 258 thinks a match after), and on 24 seeds when 12 swing: +5.8 on 1-12, -3.2 on 13-24.
- Counting deaths "in a push" by the role misses the hurt ones (a bot under the join's strength
  drops the role and dies a moment later); by the push believed, the push's 30 s belief outlives
  the act. Split them (`gather`, `in`, `lapsed`) before reading either.
- zsh does not split an unquoted variable: `Q="--map=dm2 --size=4"; labs se-team $Q` passes ONE
  argument, and the matches play the default map (210 frags a side gives it away). Write the flags
  out, or `${=Q}`.
