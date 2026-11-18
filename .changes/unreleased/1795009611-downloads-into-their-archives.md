---
type: Added
title: Downloads go back into the archive they came from
---

A file fetched from a Shooter's Edge server is kept in the archive the server took it out of: `pak1.partial.pak` beside the game's other packs, used at once, and renamed to `pak1.pak` (the server's, byte for byte) once every file of it is held. A file that was loose on the server, or one from a server that does not say, lands at its place in the game's folder. The engine's `vfs::keep_in_partial` and `vfs::archive_origin` (`engine/vfs/partial.hpp`) do it for any game.
