---
type: Fixed
title: Joining a Quake II server that forces a reconnect
---

A Quake II server with `sv_force_reconnect` set no longer drops our client every time. The client now keeps the settings the server sent it when it connects again, so it comes back with the value the server asks for.
