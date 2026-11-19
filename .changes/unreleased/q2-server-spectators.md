---
type: Added
title: Spectators on Quake II's server have their own password and slots
---

Quake II's server now asks a spectator for `sv_spectatorpassword` (given as the `spectator` key, as a client of Quake II sends it; Quake II's `spectator_password` is its synonym), gives spectators `sv_maxspectators` slots of their own beside the players' (0: they take players' slots; Quake II's `maxspectators` is its synonym), and tells `maxspectators` and `needpass` in its status, with Quake II's own refusals.
