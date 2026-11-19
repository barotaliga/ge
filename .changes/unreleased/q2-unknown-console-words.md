---
type: Fixed
title: Quake II's own console commands reach its rules
---

In Shooter's Edge, a console line the window doesn't know, such as `use Railgun`, `drop`, `weapnext` or `players`, now goes to Quake II's rules while Quake II is playing, the way that game's client passes its commands to its server. A line the rules refuse too is still reported as an unknown command. Running the install's config files no longer drops the window's own handler for such lines.
