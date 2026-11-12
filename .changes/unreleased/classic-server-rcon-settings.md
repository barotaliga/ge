---
type: Added
title: The remote console's hashed password
---

A Shooter's Edge server's remote console takes a hashed password: the SHA-1 of the password, the time and the line, so the password never travels. `sv_rconcrypt` (classic `sv_crypt_rcon`) 1 takes only the hashed form; `sv_rcontime` (`sv_timestamplen`) is how far its time may be off; `sv_rconrate` (`sv_rconlim`) is how many tries a second are answered.
