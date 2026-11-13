---
title: The classic protocol's library builds for the web once its files include what they use
date: 2026-11-13
concepts: [web, emscripten, includes, libcxx, classic-protocol]
---

`shooters_edge_classic` had never been built on the `web` preset; linking it into the player found
three files calling `std::atoi`, `std::strtol` and `std::strtoul` without `<cstdlib>` (GCC's
headers pull it in, Emscripten's libc++ does not). Build a library newly linked into a web target
with `cmake --build build/web --target <lib> -- -k 0` first: it lists every such file at once.
