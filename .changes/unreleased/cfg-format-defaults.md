---
type: Added
title: a formatted config can list every setting at its default
---

`settings format --pretty --defaults` and `server format --pretty --defaults` write every setting the config leaves alone into its section too, at its default and with its one sentence of help, so a player's or a server's config shows everything it could set. A setting the config already sets keeps its value, and formatting the result again changes nothing.
