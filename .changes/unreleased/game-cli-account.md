---
type: Added
title: the game CLI's account commands
---

Every game's player CLI gains `account signup`, `account signin`, `account code`, `account recover` and `account show`. They reach a master's account service (a hub run with `--accounts`, or a server that is its own master). `account code` prints the one-time `login <name> <code>` line a classic client types in its console. The CLI keeps the service, its key and the last ticket in its state folder. It learns the key at the first contact and refuses a service that later answers with another one.
