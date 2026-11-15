---
type: Added
title: Disc images, FLAC and games taken in with their soundtracks
---

`engine/vfs/disc.hpp` reads a game's disc from its image without mounting it: ISO 9660 with the Joliet tree, `.iso`, a `.cue` sheet with its `.bin` files (raw MODE1/MODE2 and AUDIO tracks) and a lone `.bin` or `.img`, files by path and audio tracks as samples. `engine/audio/flac.hpp` writes and reads FLAC, lossless. The classic plugin's content layer copies a classic game out of a folder, a mounted disc or a disc image into a game's data folder (`content/import.hpp`) and keeps its soundtrack one way from every source (`content/soundtrack.hpp`: `music/trackNN.flac` and a manifest, tracks known by the SHA-256 of their samples); Shooter's Edge copies a game pointed at with `-basedir`, LOAD GAME, `basedir` or the new `import` before it opens it.
