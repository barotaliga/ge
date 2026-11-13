---
type: Added
title: our own client logs itself in
---

Shooter's Edge's own client now logs itself in on a server that uses the same account master: after `account signin` in the game CLI, joining such a server trades the saved sign-in for a one-time code with the master and sends the login line, so the player types nothing. The sign-in itself never goes to the game server. A server that keeps logins says whose accounts it uses in its server info (`*accountkey`). The game CLI's `account code` now makes a code from the saved sign-in too, without the password.
