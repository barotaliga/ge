---
type: Added
title: Quake II's footsteps by surface, merged looping sounds and music by name
---

Quake II's footsteps follow the surface underfoot: a texture's `.mat` file names its material and
`player/steps/<material>N.wav` are its steps, never the same one twice in a row (`cl_footsteps`; 0
also asks a protocol 36 server to send none). Copies of one looping sound are heard as one, so a row
of machines is not louder than the game meant (`al_merge_looping`). The music plays a track by its
file's name (`ogg play <name>`, a server naming its level's music by text), `ogg_enable 0` pauses it
where it is, and a movie stops it. `s_mixahead` is kept for configs and the sound page.
