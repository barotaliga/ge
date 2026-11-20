---
type: Added
title: Quake II's server takes an administrator's remote console, kicks and bans
---

Quake II's server (`sesv -classic q2`) now answers `rcon` with `rcon_password`, and its administrator's words `status`, `kick`, `dumpuser`, `addip`, `removeip`, `listip` and `writeip`, with the answers a stock client of Quake II expects. The ban list is the engine's, kept in the game folder's `listip.cfg` and read back at start; `sv_banmode` (Quake II's `filterban` is its synonym) decides whether it bans or only lets in, and a refused connect reads "Banned.". `server::ban_file_read` and `ban_file_write` keep any endpoint's list in its file.
