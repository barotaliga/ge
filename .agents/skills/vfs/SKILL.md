---
name: vfs
description: "Use for engine/vfs in gameengine2: content files and archives, the pack archive (pak: its checked directory, a file as a view into the archive, the fixture writer) and zip archives (stored and deflated, CRC-checked); the search path over game directories, packs, pk3s, .pk3dir directories and loose files, and its lookup that never allocates; the wad archive and palette pictures (indices, no colours); the palette and the colour map (docs/goals.md M18a). Load it to read a file out of an archive, add an archive kind, or when a pack is refused."
---

# Content files

`engine/vfs/` (`ge_vfs`, namespace `ge::vfs`) finds a game's content files where they are
shipped: a search path of game directories, each mounting its packs, zips, unpacked zips and
loose files, every name answered by the source that wins it. Roadmap row M18a's status file
(`docs/goals/M18a.md`) says what has landed and what is open.

| File | What |
| --- | --- |
| `vfs/pak.hpp/.cpp` | The pack archive: `read_pak` (the directory, every entry checked), `pak_find`, `pak_file` (a view into the archive, never a copy), `write_pak` (fixtures) |
| `vfs/wad.hpp/.cpp` | The wad archive and palette pictures: `read_wad` (every lump checked), `wad_find` (any case), `wad_lump` (a view), `read_picture` (a picture lump or a `.lmp` file: width, height, indices), `read_raw_picture` (the font sheet's headerless lump), `write_wad` and `write_picture` (fixtures). Indices only: the colours are the reader's |
| `vfs/zip.hpp/.cpp` | Zip archives: `read_zip` (the end record and central directory, every entry checked against its local header), `zip_find`, `zip_unpack` (stored or deflated through `core::inflate_raw`, CRC-32 checked), `write_stored_zip` (fixtures) |
| `vfs/search.hpp/.cpp` | The search path: `add_game` (a game directory: `pak<n>.pak` in number order, then `*.pk3` and `*.pk3dir` in name order, then loose files), `add_archive` and `add_directory` (one source), `find` (folded name, no allocation), `read`, `view` (a pack's or a stored zip entry's bytes, no copy), `stored_name`, `list` (every name and its winner) |
| `vfs/palette.hpp/.cpp` | The palette (768 bytes) and the colour map (64 levels of 256 indices, an optional trailing byte): `read_palette`, `read_colormap`, their writers |

## Rules

- **Check the whole directory before handing anything back.** An archive is untrusted input: the
  magic, the directory inside the file, whole entries, terminated paths, and every file inside the
  archive are checked in `read_pak`, so nothing after it can read out of range. A refusal names
  the entry and the byte offset and says what to do (`docs/architecture.md` § Diagnostics).
- **A file is a view.** `pak_file` and the search path's `view` return a span into the archive's
  bytes; the caller keeps the archive (or the search path) alive. Copy only when the bytes must
  outlive it. A stored zip entry is viewed only when its packed size is its size.
- **Names are folded, once.** ASCII case and `\` against `/` are folded as a name is hashed;
  every source's names go through the same fold, so a lookup in any spelling finds the winner and
  `find` never allocates (`tests/vfs_search_test.cpp` counts the allocations).
- **Order is the contract.** Within a game directory: packs by number (pak10 after pak2), then
  pk3s and pk3dirs together by name, then loose files; a later game directory over an earlier one.
  `tests/golden/vfs_search.txt` pins the winner of every name in the fixture tree.
- **No content in the tree.** Tests and labs make their own archives with `write_pak`. A lab may
  open an archive the user passes at run time (`labs bsp --pak=<file> --map=maps/<name>.bsp`);
  nothing from such an archive is committed, and no doc or lesson names the game it came from.

## Tests and labs

`tests/vfs_pak_test.cpp`, `tests/vfs_zip_test.cpp`, `tests/vfs_wad_test.cpp` and
`tests/vfs_palette_test.cpp` (a round trip, every damage refused); `tests/vfs_search_test.cpp` (the
order, case and slashes, views, no allocation, a damaged archive named, the golden file table);
`tests/vfs_fuzz_test.cpp` (thousands of mutated packs, zips and wads: refused or read in range,
never out of it, under the sanitizers). The `pak` lab (`make lab LAB=pak`) shows a search path's
sources, winners and hidden files and draws the palette, the colour map and the pictures; its
smoke run is a fixture tree, and `--root=<dir> --game=<name>` reads content the user points it at
(never committed). The `bsp` lab's `--pak` reads its
maps and the palette out of a pack (`brush` skill).
