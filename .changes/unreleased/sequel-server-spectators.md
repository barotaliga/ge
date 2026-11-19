---
type: Added
title: Spectators on the sequel's server have their own password and slots
---

The sequel's server now asks a spectator for `sv_spectatorpassword` (given as the `spectator` key, as a client of the sequel sends it; the sequel's `spectator_password` is its synonym), gives spectators `sv_maxspectators` slots of their own beside the players' (0: they take players' slots; the sequel's `maxspectators` is its synonym), and tells `maxspectators` and `needpass` in its status, with the sequel's own refusals.
