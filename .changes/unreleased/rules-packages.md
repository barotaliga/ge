---
type: Added
title: Shooter's Edge's rules ship as mod packages
---

A whole program of rules can now be a mod package: `games/shooters_edge/sim/rules_api.hpp` declares the program (its form, globals, fields, strings and functions) and hands each function checked services for every word of the game's state and the classic server's calls by number. The arena and the campaign are written against it, built into the game as before and built as the `arena` and `campaign` packages, which play the same matches to the bit.
