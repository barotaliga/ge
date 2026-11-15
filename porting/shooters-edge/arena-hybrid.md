# The hybrid-engine arena shooter: its pack files

Research only, 2026-11-15. Asked for: a reader for the pack files of the free-to-play arena
shooter (2017) built by Saber Interactive with id Software, to prepare a later port. Nothing was
built: the reasons are below. No source or tool script was read; these facts come from public
forum posts and press, in prose.

## The engine

- It is a hybrid: Saber Interactive's own engine (the Saber3D line) with id Tech pieces
  (renderer work, later a Vulkan path). It is NOT id Tech 6 and not plain Saber3D.
  [read: press 2016-2017, id and Saber interviews]
- No modding, map tools or SDK were ever released. [read: press]

## The pack files (community notes, 2017-2018)

- `.pak` files in the install's `paks` folder, 25 MB to about 16 GB each. Two kinds: one holds
  the embedded browser's data uncompressed, the other the game's data. [read]
- Layout as described: the entries' data first, each entry mostly a single zlib stream (no
  chunks); then the directory; then a footer that names a `PK` signature, version bytes, the
  directory's length and offset, and a final 40-byte block. Every pack ends with the same
  8-byte identifier. [read]
- **The directory and footer are encrypted.** The 2017 beta's cipher was a 64-bit random number
  generator (the kind in a numerical-recipes textbook) seeded from the last 40 bytes, with a
  32-byte IV from the same region. In 2018 the format changed (another signature variant, a
  different generator); the public notes end there with the new scheme unknown. [read]
- Without the directory there are no names, sizes or offsets: only unnamed zlib streams.

## Why no reader

1. Reading the directory means decrypting it, i.e. working around protection the publisher
   added on purpose. This tree does not do that (project rule), and in some jurisdictions it
   is anti-circumvention territory.
2. The game is online-only with anti-cheat; its terms forbid reverse engineering the client.
   Tools that touch the client's files put the player's account at risk.
3. The current format is not publicly known, so a reader could not be verified without
   reverse engineering it ourselves, which is point 1 again.

## What would change this

- An official release of the format, tools or an offline build.
- A port that rebuilds the game's ideas (champions with abilities, the arena maps' layouts) as
  our own content in Shooter's Edge needs none of the files: the maps can be drawn fresh with
  the map SDK and the rules written in C++.
