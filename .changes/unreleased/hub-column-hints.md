---
type: Added
title: The hub reads a game's own columns as the game says, and names a player put out for cheating
---

`HubSpec::columns` gives a report column its words, scale and digits and a sentence on what it means, so Shooter's Edge's aim spread reads as 0.64 (or "-" with too few shots) instead of a raw number. A match page names, under the scoreboard, a player the server's cheat checks put out. A player's totals keep a `_min` column's least known value, so the tightest aim spread across their matches is the one shown.
