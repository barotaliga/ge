---
type: Added
title: Shooter's Edge draws a map's own sky box
---

A map whose settings name a sky box (`sky`) is drawn with that box's six pictures (`env/<name>rt` .. `dn`, or under `gfx/env/`, as .tga, .png, .jpg or .pcx) through its sky in place of the two scrolling layers, wherever the pack's own sky is drawn. When a picture is missing or does not read, a console line says which and the map's sky is drawn as before.
