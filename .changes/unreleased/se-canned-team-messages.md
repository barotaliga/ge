---
type: Added
title: Canned team messages and more config events
---

In Shooter's Edge, the `tp_msg*` commands a teamplay config binds (`tp_msgreport`, `tp_msglost`, `tp_msgneed`, `tp_msgsafe`, `tp_msghelp`, `tp_msgcoming` and the rest) tell your team your armour, health, weapons, powerups and place behind your short name. A config's `f_demostart`, `f_demoend`, `f_exit` and `f_cfgload` aliases run on those events, and `echo` fills in the percent codes and fun characters.
