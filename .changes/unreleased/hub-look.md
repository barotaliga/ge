---
type: Changed
title: The hub's pages in each game's look
---

Every hub page has a new look: a sticky header with the game's mark, map banners in each map's own colour, a pulsing badge on a match being played, a bar of the teams' shares, large counts, cards and tables that read in light and dark, and a phone layout with one column and a sideways nav. A game picks its colours with `HubSpec::look` (`arena` for Shooter's Edge, `alpine` for Fall Line; empty is the engine's own). The `hub-pages` lab now also draws the Demos, Player, Match and Leaders pages.
