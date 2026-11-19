---
type: Added
title: A limited remote console for helpers
---

`lrcon_password` gives a trusted helper a second remote-console password that runs only the commands the administrator allowed with `lrcon_add <start of a command>` (for example `lrcon_add map` or `lrcon_add kick`); anything else answers "This command is not permitted.". `lrcon_del` and `lrcon_list` manage the list. Both games' servers have it; on Quake II's, `addlrconcmd`, `dellrconcmd` and `listlrconcmds` work too.
