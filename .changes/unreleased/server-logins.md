---
type: Added
title: logins on game servers
---

A game server can now ask its players to log in to an account (`sv_login`: 1 players, 2 everyone). A dedicated server with no `sv_accountmaster` is its own account master: it keeps `accounts.log` and `account.key` in its game folder, answers the account service on `sv_accountport` (27901) and prints its key at the start. With `sv_accountmaster` set it asks that master instead, checking the master's tickets with `sv_accountkey`. On Shooter's Edge a stock classic client logs in from the console with a one-time code, `login <name> <code>` or `login <name>` and then the code; a logged-in player gets the `*auth` key and the account's name (`sv_loginname`). Registered names are kept from guests (`sv_reservednames`), a player who must log in can't chat until they have, and is dropped after `sv_logintime` seconds. `sv_loginwelcome` sets the line shown before the prompt.
