---
title: A per-player grant is voted on as a setting keyed by the player's user id
date: 2026-11-10
scope: games/shooters_edge/net/classic_game.cpp, engine/server/vote.hpp
concepts: [vote, speedjump, easy-jump, per-player, user-id, vote-check-key, revert]
---

`VoteCheck::key` is what makes a vote on a player work without touching the engine's vote: the
game's check names the change `speedjump:<user id>`, so the book keeps each player's grant apart,
keeps the FIRST `before` when the same player is voted on twice, and `vote_map_over` hands back
one change per player. Put the player's name in `value` and `before` (`Ann on`, `Ann off`) so the
broadcast lines say who, and read the share off the value's last word and the player off the key
when applying: names may hold spaces, the user id is stable across maps and a slot is not. Apply
through the administrator's call (`classic_speedjump`), which already discloses and prints; a
player gone by the next map is passed over by its error.
