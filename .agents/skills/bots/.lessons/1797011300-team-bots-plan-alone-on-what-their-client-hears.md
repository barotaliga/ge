---
title: Team bots plan alone on what their client hears, and a fighter change reaches every bot built on the fighter
date: 2026-11-01
scope: plugins/shooterbots/mind.cpp, games/shooters_edge/bots/talk.cpp, plugins/shooterbots/team.cpp, plugins/shooterbots/companion.cpp, plugins/shooterbots/way_through.cpp, games/shooters_edge/net/qw_team.cpp
concepts: [team, roles, claims, separate-brains, say-team, loc, placeheard, companion, give-way, coop, campaign, real-map, seeds]
---

- The team plan names roles and claims only; the mode, retreats, weapon and trigger stay each
  fighter's. A planner reading every member's state plays as one mind: each bot believes its own
  senses and the team lines its CLIENT receives (`split_team_line`), a person's as a bot's.
- Planning alone: stagger each bot's first plan off its own stream (two claimed one item in one
  frame); the rusher SAYS it, or all four rush the quad; hold the opening claim until the reach is
  worked out. A call that must follow another at once passes the rate limit (`kSayGap_s`) on
  purpose.
- With no `.loc` file name places off the items. A loc name repeats across rooms: resolve a heard
  place round where the speaker was last known (`placeheard`).
- A companion at the leader's shoulder blocks him: read the leader's stillness off his speed, and
  give way off his line in every mode. A test's scripted leader must reach each leg in about the
  walk's time, or a blocked leader reads as a bored companion.
- The companion and the campaign leader are fighters: a deathmatch behaviour reaches them (the dodge
  put a companion out of position); gate it off `escort.leader >= 0`.
- The campaign leader passed the coop hall and stalled on every real map for a new reason.
  `--plan=1`'s log of plan changes plus the map's entities near where he stood found each. Judge
  over several seeds (one explores into water), and tell the campaign's builder of a program gap.
