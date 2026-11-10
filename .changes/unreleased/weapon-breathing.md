---
type: Added
title: the held weapon breathes at rest
---

The first-person weapon now rises and falls with the holder's breath when the player stands still (`viewmodel::PlaceRow`'s breath, in both the classic and the modern row): about 15 breaths a minute, each a little different in depth and length, with an occasional deeper sigh and a slow wander of the hands. A run leaves the body exerted, so right after it the breath is faster and deeper, settling back over the next 20 seconds. Shooter's Edge and the classic games' own weapons take it with three new player cvars: `cl_gunidle` (its share, 0 off), `cl_gunidle_rate` (breaths a minute at rest) and `cl_gunidle_exertion` (how much a run quickens and deepens it).
