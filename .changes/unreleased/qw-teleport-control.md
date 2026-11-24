---
type: Fixed
title: Walking through a teleporter in QuakeWorld keeps you in control, and the arrival effect no longer hides the view
---

On a QuakeWorld server, the moves you make while a teleport is on its way to you now go where you face on the far side, on servers that offer the fix (ours and the competitive ones), so you no longer walk the old way or get pulled back for a moment after coming out; any turn of the mouse during that moment is kept instead of thrown away. Our own server now turns you at a teleporter's far end at all when you are joined over the network. `cl_pext_lagteleport 0` turns it off. With classic enhance's warps on, your own arrival only touches the edges of the view for a third of a second: no white flash, no zoom, no ash before your eyes, and your weapon stays in your hands; other players warp in and out in about a third of a second, so nobody seems to stand where they no longer are.
