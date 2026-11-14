---
type: Added
title: Demos convert between formats, as a file or on the fly
---

`shooters_edge_cli demos convert <name> <file>` writes a recording in the format the file's name asks for: a single-player `.dem` becomes a multiview `.mvd` (the same eye, view and status every frame, about a third of the size), and a sequel `.dm2` of an older protocol becomes protocol 34, which every stock client plays. The converter is a stream (`client/demo_convert.hpp`), so a broadcast can serve a recording in its viewers' format as it reads it. Multiview recordings that open with a converter's banner now play.
