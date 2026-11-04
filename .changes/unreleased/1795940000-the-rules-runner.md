---
type: Changed
title: The rules runner
---

Shooter's Edge's listen server, lobby, campaign, network endpoint and dedicated server now hold a game's rules as a runner (a table of calls over the rules' state) instead of the QC server itself, so a second program form can plug in beside it; behaviour and every digest are unchanged.
