---
title: Real stars are a table in a cube map with slots, read three by three
date: 2026-10-23
scope: engine/render/stars.hpp
concepts: [stars, star-catalogue, cube-map, slots, generated-table, base64, line-cap]
---

Swapping hashed stars for a catalogue kept the cube-cell lookup: each cell of the 6 x 160 x 160
cube holds up to N stars as (place in the cell, magnitude, B-V), and the shader reads the
pixel's cell and the eight round it, so a star at a cell's edge is drawn whole. Size the slots by
counting what falls out: one slot lost a few hundred stars, two 66, three 19 (none brighter than
4.85; they are tight clusters), four 9. Store the cell as RGBA8 at the table's own quantisation
(magnitude in twentieths, B-V in seventieths): three layers cost 1.8 MB instead of 3.7 as half
floats. A generated data file is held to the 1000-line cap too: eight bytes a star in base64 was
1,071 lines, six bytes 807. Check the result by aiming a shot at a known figure computed from
the sky's own turn, not by eye on a random patch.
