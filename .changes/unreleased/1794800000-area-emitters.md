---
type: Added
title: Embers, motes, drips and steam an area carries
---

`render::AreaEmitter` and `emit_area` bear an area's particles (embers rising off a glow, motes in a lit dust, drips off a ceiling, steam over warm water) at a rate, each birth seeded so the frames' split never changes them; `embers_over` sets embers over a glowing fog volume. Fall Line's frame emits a scene's `FrameScene::emitters`, and its fog lab's camp fire throws embers.
