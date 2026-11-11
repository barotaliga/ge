---
title: Measure people's rate before chasing a bot count; break slow time down by what the pilot was doing
date: 2026-11-12
scope: games/shooters_edge/labs/bumps_read.hpp, games/shooters_edge/labs/se_people_lab.cpp, games/shooters_edge/labs/se_team_lab.cpp, plugins/shooter/pilot.cpp
concepts: [bumps, wall-bumps, se-people, yardstick, speed, run-up, backstep, jump, water, replan, goal-node, trace, link2, next-wet]
---

- se-team's bump count (the server's speed lost a command) has no people's number beside it, so
  "8 walk bumps a bot-minute" judged nothing. se-people's `bumps.*` (bumps_read.hpp) reads a
  bump off the recording for people and bots alike: e1m2 people 1.9 a minute (2.5 at speed),
  bots 2.4-2.8 (4.4-5.2). Per minute AT SPEED is the fair rate: a slow bot meets fewer chances.
  Build the people's yardstick before tuning a bot count.
- Breaking the bots' time under 250 u/s down by pilot mode and `because` (one traced 3-minute
  match, a short python over the trace) found the costs at once: swimming 21 %, walking 19 %,
  a jump's run-up 10 %. A run-up walked back to its point facing it and turned round twice; a
  person steps back facing the jump (`backstep`: speed 193 -> 197, +1.6 h2h).
- Replans looked guilty (a goal node changes 26 times a bot-minute) but bumps after one were no
  more common than the time after one (19 % against 22 %): test a suspected cause against its
  base rate before building on it. Same for "the velocity points away from the next node": 21 %
  of all running, so 40 % of bumps is only twice the base.
- se-team's trace gained `link2` (the next link's kind, `end` at the route's last node) and
  `next_wet` (the next node in water): with them a water entry reads as planned or a fall.
- A temporary `#define go_to(f, n) go_to_at(f, n, __LINE__)` with an `extern "C"` array named the
  call site of every goal change in one run; it is a probe, reverted before the commit.
