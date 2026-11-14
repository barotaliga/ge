---
type: Added
title: A game played in a hub's page is heard and can be played by touch
---

`ge::web::audio_open` and `audio_pump` play the engine's synth through the page's Web Audio, each paint rendering what keeps the page a little ahead, from the page's first click or key (a browser's rule); before that the synth is rendered and dropped at the paint's pace. `take_play_input` now also reads the fingers on a touch screen into the input state. Shooter's Edge's web player is heard in its play and join modes as the window is, and plays by touch (`client/touch_play.hpp`: a walk stick on the left, a look drag on the right, fire, jump and next-weapon buttons, drawn once a finger touches).
