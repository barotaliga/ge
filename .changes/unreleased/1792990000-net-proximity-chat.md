---
type: Added
title: Proximity chat in a session
---

A session's chat has a fourth scope, `ChatScope::nearby`. Such a line reaches the players that the game's earshot callback (`SessionConfig::hears`) says can hear the sender, because the game knows where everyone is and how far a voice carries. A session whose game gives no earshot refuses the line with `no_nearby`.
