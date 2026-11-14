---
title: A banner the pack lacks is another banner's frame, its plain stretched, and the tag letters
date: 2026-11-14
scope: games/shooters_edge/menu/classic_view.cpp
concepts: [banners, plates, tags, page-title, shading, carved-letters]
---

The sequel's page banners are tags of the same metal as its menu tags: the same rows for the letters
(7 to 21) and the same rim profile, so a title the pack has no banner for is one banner's frame
(the hole and logo left of x 40, the maker's mark from x 236), a flat plain column of it stretched
between, and the tag letters on top (`banner_title`). Draw the frame as boxes over the pack's own
picture with a brightness multiplier (`Box::fill`) rather than building an image: no new pictures
to keep alive. The banners are about a tenth brighter than the darkest tag the letters are shaded
to, so darken the frame to the letters (`banner_shade`), never brighten the letters. A title in
capitals wider than the 320 px screen fits in small letters after each word's capital.
