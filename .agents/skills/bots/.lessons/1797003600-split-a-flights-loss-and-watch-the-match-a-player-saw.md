---
title: Split a flight's loss into the keys' and the walls' exactly, and take what a player saw in a match as the plan
date: 2026-11-12
scope: plugins/shooter/bot.cpp, plugins/shooter/pilot.cpp, games/shooters_edge/bots/watch.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [hop, landing, air-accelerate, walls, airwalk, hopjump, hopside, levelview, noback, mouse-hand, tolerance, paced-view, trace, way, flights]
---

- The air move is simple enough to recompute off an `se-team --trace --trace-every=1` row: the
  keys sent under the view sent, the wish's share along the velocity capped at 30, accelerate x
  wishspeed x 0.013 s. It matched to 0.01 units/s (p50), so a flight's speed change splits
  EXACTLY into what the keys gave and the rest (walls, bodies, hits). That split found that two
  thirds of a chained hop's key loss came after the hop had ended in the air (the walk's forward
  key braking): `airwalk`, +3.2 / +1.4 frags a match.
- A maintainer watching one match named three faults the numbers had circled for days (the wait
  before the next jump, forward held in the hop, jumps into walls). Each checked out in the trace
  at once (the first ground frame jumped 2 %, forward in 19 % of the hop's air). When the
  maintainer describes how a move is done, build that move; the measures then judge it.
- A jump held on the way down goes off on the landing frame, so the trace has NO ground row
  between the two flights: a split on `ground` merges a chain into one long flight, and a
  "landing" table then shows only the landings that did not chain. Split at the jump's rise.
- A view paced back at a hand's pace (`kBackTurn`) moved 1.2 degrees a command in pitch, inside
  the mouse hand's 3-degree tolerance: the hand never stroked, and a bot ran 35 s looking 79
  degrees down. When two layers each limit a motion (a pace and a dead band), check that the
  step one gives is larger than the band the other ignores.
- Hopping on from a slow landing cost 23 units/s a match: under ~240 the air's side key gains
  less than a run on the ground. Keep that rule when making the chain faster.
- A score that holds each option for the whole look (`airarc`) cannot value "nothing now, turn
  later": it halved the wall hits and braked as much. Short looks bias it least; it still lost
  head to head on dm2.
