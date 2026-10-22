---
type: Added
title: The falling snow and the wind
---

`plugins/mountain/air/air.hpp` gives the air over a map at any moment: the wind breathing in gusts and veering about the mean the map dealt (never up the mountain), and the falling snow breathing in squalls by its sky, with how far one can see through it, both pure functions of the map and the clock. `engine/render/snowfall.hpp` draws the falling snow round the camera: a box of flakes that follows the eye, carried by the wind, drawn by how hard it is falling. The particles now drift on the wind, and deep powder throws a cloud that hangs behind a skier. Fall Line draws both, and the new `snowfall` lab shows a flurry, a steady snow and a storm.
