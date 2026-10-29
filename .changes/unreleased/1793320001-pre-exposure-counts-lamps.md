---
type: Fixed
title: A bright light in a dark room no longer renders black
---

The frame's pre-exposure now counts the local lights' lux (`FrameView::lamp_lux`) beside the sun and the sky, so a room lit only by lamps is no longer exposed so far up that a bright emitter overflows the half-float target and the tone curve draws it black.
