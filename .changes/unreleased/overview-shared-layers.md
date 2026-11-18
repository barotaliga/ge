---
type: Added
title: The engine draws a level from above for any game's spectators
---

`engine/overview` (`ge::overview`) draws a level from above on the CPU: its floors shaded, its liquids a layer over what lies under them, the roofs that show what a bridge covers, and a model's frame drawn small as a figure or an icon. The brush plugin fills it from a classic map (`overview_floors`), the morph plugin turns a model's frame into its mesh (`overview_mesh`, `frame_at`). Shooter's Edge's live map now draws through it, unchanged to the pixel, so the next game ported brings only its items, colours and demo.
