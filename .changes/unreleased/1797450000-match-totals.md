---
type: Added
title: Players' totals at the account master
---

A server now sends each finished match's report to its account master (`sv_stats`, on by default), signed with a key of its own and sent again until the master answers; a server that is its own master counts its own matches. The master keeps the reports (`reports.log`) and adds each logged-in player's part to their totals per game, mode, map and ping band, but only for servers its owner trusts (the hub's `--trusted-servers=<file>`). Totals can be read whole, for one map or mode, without some modes, or at a good ping. Each player's report also carries their lowest, average and highest ping.
