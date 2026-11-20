---
title: A loop played as the world (entity -1) that loses its slot never comes back; give it an entity
date: 2026-11-20
scope: engine/audio/channels.hpp, games/shooters_edge/client/demo_sound.cpp
concepts: [channels, static-sound, loop, steal, pseudo-entity, slot]
---

`Channels::play` never matches a slot for a negative entity and a burst of louder sounds steals the
quietest slot, so a level's static loops started once as entity -1 went silent for good after a
fight. Each static now plays as a pseudo-entity of its own (`-2 - index`), the slots are read each
frame to see which still sound, and a lost loop is tried again only when a slot is free (two loops
retried into a full table would steal from each other every frame). A one-shot static is never
retried. In a test, a voice that ended frees its slot only at the next `update`, one frame later.
