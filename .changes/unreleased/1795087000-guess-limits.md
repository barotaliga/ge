---
type: Added
title: Password guessing is slowed on both games' servers
---

`sv_rconrate` and the new `sv_passwordrate` take `limit[/period][*burst]` (for example `1`, `3/min` or `10*10`): tries refill over time, a right password gives its try back, and a guess past the limit gets no answer. Quake II's server now limits remote-console and connect-password guesses to one a second (its `sv_rcon_limit` and `sv_auth_limit` are the same settings); the first game keeps ten remote-console tries a second and no connect limit.
