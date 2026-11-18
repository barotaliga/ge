---
type: Added
title: The sequel's server takes an administrator's remote console, kicks and bans
---

The sequel's server (`sesv -game 2`) now answers `rcon` with `rcon_password`, and its administrator's words `status`, `kick`, `dumpuser`, `addip`, `removeip`, `listip` and `writeip`, with the answers a stock client of the sequel expects. The ban list is the engine's, kept in the game folder's `listip.cfg` and read back at start; `sv_banmode` (the sequel's `filterban` is its synonym) decides whether it bans or only lets in, and a refused connect reads "Banned.". `server::ban_file_read` and `ban_file_write` keep any endpoint's list in its file.
