---
type: Added
title: Routes through forwarders, the browser's caches and remote screenshots
---

JOIN A GAME can find routes through forwarders (`sb_findroutes`, `sb_showproxies`, `connectbr`,
`observebr`), and `cl_proxyaddr` joins through a chain of them. The browser keeps each master's
last list and, if asked, the whole list between sessions (`sb_mastercache`, `sb_listcache`). A
dedicated server can ask a player for a picture of his screen (`snap`, `snapall`), and the
player's `scr_allowsnap` decides whether he sends one. The engine's screenshot writer can grab a
frame in memory (`Screenshots::grab`), and the server list reads and writes its cache as text.
