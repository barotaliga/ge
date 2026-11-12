---
type: Added
title: Shooter's Edge draws a map's coloured light from its .lit file
---

When the game's files hold a `.lit` file for the map (`maps/<name>.lit` beside it, or `lits/<name>.lit`), the window draws the map's light in its colours instead of grey, on the walls and on the models standing in it. A `.lit` made for another build of the map is set aside with a line in the console, and the map keeps its own light.
