# Content: installs, packs, the search path, downloads

How a classic game's files reach the engine: the player's install taken in, its archives opened,
its files ranked in the game's own order, its release known, its music kept, and a file fetched
from a server put back where it came from. The player meets it in SELECT GAME, in the install
question, and when joining a server that has a map they lack. The rules for installs, discs and
downloads are `docs/porting-games.md` § Taking a game in and § Downloads into archives; this page
places the pieces.

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/vfs/pak.hpp`, `zip.hpp`, `wad.hpp`, `palette.hpp` | The pack archive, zips (`.pk3`, `.pkz`), wads, the 256-colour palette and colour map |
| Engine | `engine/vfs/search.hpp` | The search path: game folders, packs, pk3s and pk3 folders, the winning file |
| Engine | `engine/vfs/unpack.hpp`, `lha.hpp`, `lzma.hpp`, `bzip2.hpp`, `tar.hpp`, `rar.hpp`, `zstd.hpp` | The one file inside any packed or archived bytes; installers' archives |
| Engine | `engine/vfs/disc.hpp` | Disc images: ISO 9660 files and the audio tracks of `.cue`/`.bin` |
| Engine | `engine/vfs/partial.hpp` | A download kept in the archive it came from (`archive_origin`, `keep_in_partial`, the origin's wire body) |
| Engine | `engine/vfs/browse.hpp` | A folder browser's places and the files a game opens, packed or not |
| Engine | `engine/audio/flac.hpp`, `engine/audio/music.hpp`, `music_console.hpp` | The kept soundtrack and its player (`cd`, `bgmvolume`) |
| Classic plugin | `plugins/classic/content/files.hpp` | A classic game's search path: base folder, a mod folder over it, packs and pk3s |
| Classic plugin | `plugins/classic/content/load_order.hpp` | Each game's load order: its files' ranks, its startup configs in order |
| Classic plugin | `plugins/classic/content/open.hpp` | The open calls every game shares: models by kind, sounds, map textures, maps; each takes the game's content row |
| Classic plugin | `plugins/classic/content/import.hpp` | An install, mounted disc or disc image copied into `data/<id>/` |
| Classic plugin | `plugins/classic/content/soundtrack.hpp` | `music/trackNN.flac`, the manifest, tracks known by the SHA-256 of their samples |
| Classic plugin | `plugins/classic/content/downloads.hpp` | The two ends of a download: `served_origin` on the server, `keep_download` on the client |
| Classic enhance | `plugins/classicenhance/profile.hpp` | THE CONTENT ROW: base and multiplayer folders, packs, asset kinds, palette, wad, start map, the full game's file, material words |
| Classic enhance | `plugins/classicenhance/source.hpp`, `known_sets.hpp` | The packs found in a zip, LHA archive, file or folder; known releases by SHA-256 of every file |
| Shell | `games/shooters_edge/client/take_in.hpp`, `install_search.hpp`, `install_ask.hpp`, `installs.hpp`, `game_folder.hpp`, `install_configs.hpp` | The window's side: taking a game in, finding installs round the game, asking for one, the install's configs in its own order |
| Shell | `games/shooters_edge/client/adapter_parts.hpp` | The calls every game's adapter shares (mods, programs, maps, textures, models) |
| Shell | `games/shooters_edge/profiles/game_profile.hpp` | THE GAME ROW: each mode's program files with form and side, demo extensions and folder, the fallback map, the episodes, weapon groups, place-name reach |

## Each game's

| | The first game | Quake II | Next: the third | Next: the sector family |
| --- | --- | --- | --- | --- |
| Content row | base folder, numbered packs, its picture wad, a 768-byte palette | base folder, packs, the palette at the end of a PCX picture (`palette_of`), no wad | base folder of zips read in name order | one IWAD per game plus PWADs (`engine/vfs/wad.hpp` reads the archive) |
| Load order | packs over loose files, a client folder | its own order (`LoadOrder`) | zips in name order | the IWAD, then each PWAD in command-line order |
| Installer | the disc's LHA archive opened in memory | files on the disc | files | files; DOS installers if a release needs them |
| Soundtrack | disc audio tracks | disc audio tracks | its own music files | the music lumps of its archive (a synth's score, not audio: a new reader) |
| Adapter | `client/qw_adapter.cpp` | `client/q2_adapter.cpp` | a file of its own beside them | a file of its own |

## Adding a game

1. A content row in `profile.hpp` and a game row in `profiles/game_profile.hpp`. A difference
   between games is a field of a row, never an `if` on its id.
2. The known releases: every file's SHA-256 in `known_sets.hpp`, recorded from a real install;
   a pack that looks like a repack may be another release.
3. The load order in `load_order.hpp`: the ranks the game's own client gives its files, its startup
   configs in order. `install_configs.cpp` keeps the pack's `default.cfg` from taking the window's
   move keys.
4. Any new archive or installer format is a reader in `engine/vfs` and one more case in
   `open_installers`; any folder of loose content the list does not name goes in `content_folder`
   (`import.cpp`).
5. The soundtrack row in `kSoundtracks` and, once a real disc is taken in, `kKnownTracks`.
6. Downloads: the game's protocol keeps one message id for the origin, sends it only to a client
   that agreed (`download-origin` bit), and its client calls `keep_download`
   (`network.md` § Our extensions).
7. Run every classic-enhance stage on the row and count each stage's files against the packs:
   a stage that reports zero is a gap.

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| A file a client lacks is fetched from a server that has it | the first game's community servers | `downloads.hpp`, each protocol's messages |
| Zips beside packs, a mod's folder over the base | the first game's later clients; the third | `engine/vfs/search.hpp` |
| A stand-in PNG or TGA by a texture's, skin's or picture's name, alpha and all | the first game's later clients | the shared adapter calls |
| The game's own picture files at any whole multiple of the original size | the first game's later clients | `pictures` (one density per set) |

| Drawback left behind | How |
| --- | --- |
| Downloads written loose, the server's archive lost | kept in `<pack>.partial.pak`, renamed when whole |
| Played from wherever the player keeps it, a disc needed for music | copied into `data/<id>/` once; music ripped to FLAC once |
| One config per game, a setting re-made in each | one config for the series (`settings.md`) |
| A pack's `default.cfg` taking the player's keys | the pack's startup configs run before the player's, its move keys left |

## Gaps

- The stores' music folders are as players report them; correct `docs/porting-games.md` when a real
  store install is taken in.
- The sector family's archives are read; its music format is not.

## Labs and tests

`se-installs`, `se-load`, `se-pictures`; `tests/classic_content_*_test.cpp`, `tests/vfs_*_test.cpp`.

## Lessons

- Ask a game's files through `plugins/classic/content` and the adapter, never by file name in the
  window. Shared open calls take the game's content row, never its adapter.
- Move a hard-coded path into a row and prove the move by the labs' digests, not by reading.
- Test each adapter with only that game's packs installed: Quake II once asked for the first
  game's palette and drew no weapon in hand.
