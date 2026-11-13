---
title: The web build catches a missing standard include that the debug build forgives
date: 2026-11-13
concepts: [includes, portability, web, emscripten, libc++]
---

`std::atoi` without `<cstdlib>` built clean on Linux GCC and failed only in the web preset
(Emscripten's libc++: "no member named 'atoi' in namespace 'std'"). A change that reaches the web
viewer's sources (Shooter's Edge's `client/`, `engine/web`, the renderer) builds
`shooters_edge_viewer` on the `web` preset before the push, or includes every standard header it
uses directly.
