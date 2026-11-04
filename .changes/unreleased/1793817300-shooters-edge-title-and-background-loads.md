---
type: Fixed
title: Shooter's Edge opens on a title and loads maps without freezing; remastered maps no longer white out
---

Shooter's Edge opens its window at once on a title: a night sky with the full moon in a total eclipse, blood red, and cloud carried across it, behind the main menu, while the start map loads. Every map now loads behind the window (its textures and models read on a thread, its pipelines compiled while frames go on), with a spinner in the corner naming the map and the seconds so far; the window used to stop for about 12 seconds at start. A background classic-enhance run that changes nothing no longer remakes the world. In the modern look, maps lit by a modern compiler (the remastered `start` and `dm6`) were white: their lights' fall-off kinds (`delay`), spotlight cones (`mangle`, `target`, `angle`) and `_color` are now read, by the live lights and by the rebake alike (the rebake runs again once). The engine gains `ge::hud::draw_spinner` and `PipelineSet::ready`.
