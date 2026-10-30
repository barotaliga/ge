---
type: Added
---
Content files are found through a search path (`engine/vfs/search.hpp`): game directories in order, each mounting its packs by number, then pk3 zips and `.pk3dir` directories by name, then loose files, with names matched across case and slashes and a lookup that never allocates. The palette and the colour map have readers (`vfs/palette.hpp`). The inflater and the CRC-32 moved to `engine/core`, so `ge_vfs` depends on the core alone. A fuzzed archive test runs mutated packs, zips and wads under the sanitizers. The `pak` lab shows which source wins each file and draws the palette, the colour map and the pictures.
