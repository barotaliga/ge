---
type: Fixed
title: The server browser shows a bot's ping as 0 ms, not 9999
---

A seat the server plays itself, a bot or a listen server's own player, has no link to measure. In a server's status it now reports 0 ms; before, it reported 9999 ms, so the browser and `secli players` showed every bot as lagging badly.
