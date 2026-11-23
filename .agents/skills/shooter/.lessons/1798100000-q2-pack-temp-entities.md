---
title: The Quake II mission packs' temporary entities drawn in the base game's own pieces
date: 2026-11-23
scope: games/shooters_edge/client, games/shooters_edge/app
concepts: [temp-entity, mission-packs, heat-beam, lightning, chain, flare, nuke, tracker, negative-light]
---

- **Most of the packs' kinds are the base game's pieces with another number.** The green and blue
  bolts are the blaster's hit on the flare's skins 1 and 2; 35/53/54/62/63 are the explosion
  model (53 at scale 2, 62 and 63 unlit); 43/44 are the steam puff. Add a field (`skin`, `scale`)
  to `Q2ModelFx` before adding a new class.
- **A cable of model links is `Chain` with a step.** The lightning is 35-unit links facing back
  to the start, the heat beam 32-unit links with a roll from the clock; `chain_links` and
  `chain_share` take the step, and `place_chain` reads `back`, `roll`, `frame` and `full_bright`.
- **A dark light is a negative colour.** The tracker's burst lights with rgb -1 through
  `q2_point_light`; a model effect with `drawn` false carries only its light.
- **The local heat beam follows the view, not the wire.** Its start is the eye plus the gun's side
  (`beam_hand`: the hand, the centre gun drawn on the right, a hidden gun centred) and it runs
  along the view's forward for the server's length, so `cl_fakeshaft`'s trace can take it over.
- **Named effects are the seam both halves share.** The recording parser and the listen server's
  rules send the same `te_*` names, so the rules threads need only the name list, not the drawing.
