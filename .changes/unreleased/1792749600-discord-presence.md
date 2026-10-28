---
type: Added
title: Discord presence, asks to join and joins, with a simulated Discord app
---

`ge::services::Discord` shows a game on the player's Discord profile (details, state, a timer, art, a party and a join secret), turns friends' asks to join and the player's joins from Discord into the same events the store's backend delivers, and answers an ask with `answer_join`. It talks to the Discord app already running on the player's machine over its local IPC, so it needs no SDK and no secret, looks again with backoff when Discord is not running, checks every activity against Discord's limits before sending it, and paces its updates under Discord's rate limit so a game may set its line whenever it changes. `SimDiscord` is the Discord app in memory for tests and the `discord` lab. `ge::net` gains `local_pipe.hpp`, a non-blocking byte stream to another program on the machine (a Unix domain socket or a Windows named pipe), and the flat JSON reader moves to `core/json_read.hpp`.
