---
title: A share read off positions has a floor that chance scores; read the cause, and the bot's mind a beat late
date: 2026-11-20
scope: games/shooters_edge/labs/chase_read.hpp, games/shooters_edge/net/qw_team.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/footwork.cpp
concepts: [chase, follow, floor, base-rate, cause, beat, goalfeet, chasekill, breakoff, fight-end]
---

"Followed a vanished enemy" (came near where he was last seen within 4 s) read people 30 %, bots
37.5 %, and two switches built against it (`chasekill`, `goalfeet`) barely moved it. Split by the
bot's mind (se-team `team.chase.cause.*`), bots going ELSEWHERE score 29.5 %: the measure's floor,
so people's 30 is no following at all, and the bots' excess was renewed fights, not chases.

- Before building against a share, read what a player doing something else scores on it.
- Read a bot's mode a second past the beat that holds it (`beat`, 0.5 s): read at the moment the
  enemy went, 67 % was "fight" only because the bot had not thought again yet; at 1.5 s, 40 %.
- Out of sight the bot does not aim, so the route already has the feet: a fight's footwork branch
  for "unseen" never ran (`team.goalfeet.unseen` 0). Count a branch before judging it.
- The switch that moved no human measure still won the fights: `goalfeet` +4.0 / +3.45 head to
  head. Turning the back on a fight for a timed item (`breakoff`) lost 11.4 a match.
