---
title: Ship the mod SDK as a subset of the tree, so Mods.cmake needs no second mode
date: 2026-11-14
scope: cmake/Packaging.cmake, cmake/Mods.cmake, engine/cli/mod.cpp
concepts: [mod-sdk, packaging, install, component, ge-mod-build]
---

`Mods.cmake` finds `engine/mod/abi.hpp` and `Fp.cmake` relative to itself, and the mod template
adds `${GE_ENGINE_DIR}/games` for the game's header. Installing those files at the same relative
paths under `share/gameengine2/sdk/` made the SDK work with no change to either file: a mod's
`GE_ENGINE_DIR` names a tree or the SDK alike. Put the installs in a `mod_sdk` component, so
`cmake --install build/debug --component mod_sdk --prefix <tmp>` checks the SDK in seconds without
installing the debug build's libraries (gigabytes with ASan). A game registers its headers in the
global `GE_MOD_SDK_FILES`, read by Packaging.cmake, which the root includes after the games.
