---
type: Added
title: showip on the client, and Quake II's map version check
---

A joined game's `showip` lists the machine's name and addresses and the socket it plays on ("Client UDP socket bound to ..."). Quake II checks the map on disk against the checksum the level states as it loads: a server's other map leaves the game ("Local map version differs from server: ..."), a recording's only warns and plays on.
