---
type: Added
title: packed demos say their map, mode, players and score
---

A classic recording packed into a replay file (`shooters_edge_cli demos pack`) now carries the words a hub's demo archive files it by, read off the recording itself: the map, the mode (coop, 2on2, 4on4, 1on1, ffa or single), the server's name, every player who played and their team, and in the trailer the length, each player's frags and each team's total. Spectators are left out. `DemoPackOptions::meta` lets a caller add a title, tags and the start time.
