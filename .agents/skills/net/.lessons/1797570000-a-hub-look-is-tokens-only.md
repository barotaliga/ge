---
title: A hub page's look is colour tokens on the root, never a second stylesheet
date: 2026-11-14
scope: engine/hub/style.cpp
concepts: [hub, stylesheet, look, theme, dark-mode, data-look, lab-pictures]
---

The hub serves one `/style.css` for every game; a game's look (`HubSpec::look`) only sets
`data-look` on the page's root, and each look is a block of tokens, light then dark, after the
engine's own. Order matters: a look's light block outranks the engine's dark media block by
specificity, so the look's own dark block must follow it. To see a change, run the `hub-pages` lab
(`--tag=before` first): its pictures are Chromium's, both schemes and a phone's width; a design tried
outside the tree first (static HTML from the lab's attached pages with a new stylesheet) saved
rebuilds. A page the lab does not draw (radar, results, ladder) shares the same classes, so check it
by hand when its markup is unusual.
