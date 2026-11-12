---
type: Added
title: The remote console's hashed password and the ban list
---

A Shooter's Edge server's remote console takes a hashed password: the SHA-1 of the password, the time and the line, so the password never travels. `sv_rconcrypt` (classic `sv_crypt_rcon`) 1 takes only the hashed form; `sv_rcontime` (`sv_timestamplen`) is how far its time may be off; `sv_rconrate` (`sv_rconlim`) is how many tries a second are answered.

Servers keep a ban list by address: `addip <address> [ban|safe] [+seconds]`, `removeip`, `listip` and `writeip` (saved to the game folder's `listip.cfg` and read back at start); a banned address is refused at connect and gets no remote console. `sv_banmode` (classic `filterban`) 0 turns the list round, so only the addresses on it may join.
