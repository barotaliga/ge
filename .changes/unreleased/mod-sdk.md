---
type: Added
title: The mod SDK ships in the release package
---

The release package installs `share/gameengine2/sdk/`: `cmake/Mods.cmake`, `cmake/Fp.cmake`, `engine/mod/abi.hpp` and each game's mod headers, laid out as the engine tree is. A mod repo builds against it with `ge mod new <dir> --game=<id> --engine=<prefix>/share/gameengine2/sdk`, with no engine tree. `cmake --install <build> --component mod_sdk` installs the SDK alone.
