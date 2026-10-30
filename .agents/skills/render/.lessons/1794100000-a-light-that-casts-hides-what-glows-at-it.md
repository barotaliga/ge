---
title: A lamp's own bulb or flame must not cast its shadow, and a light inside a prop goes dark
date: 2026-10-30
scope: engine/render/lamp_shadows.hpp
concepts: [lamp-shadows, casters, emissive, glows, caster-slots, wgsl-reserved-words]
---

A lamp's tiles are drawn from the lamp's own place, so anything that encloses that place (the
bulb sphere round a street lamp's light, the post it was set inside) fills every tile at its near
plane and puts the whole scene in shadow. Keep the glowing parts in a family made without caster
slots (`lit_scene`'s `glows`), and set the light below or beside any solid part (the lab's lamp
hangs under an arm). Also: `from` is a reserved word in WGSL; a variable named so compiles
nowhere, and the error names the first shader that includes the file (sky.wgsl), not the file.
