---
type: Changed
title: the campaign's rules are C++
---

Shooter's Edge's own single-player campaign (the level flow, secrets, runes and skill; every monster and boss; trains, secret doors, traps and the other set pieces; co-op) is now C++ (`games/shooters_edge/rules/campaign*.cpp`), on the rules it shares with the deathmatch, run as a native program where the compiled QC ran. It plays the QC's co-op runs to the bit. The last QC of our own (`games/shooters_edge/qc/`) is deleted; a lab's `--progs=campaign` names it.
