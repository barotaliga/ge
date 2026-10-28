---
type: Added
title: Touch thumbs and the gyro's bias
---

Fall Line can be played on a phone's screen. On the left half, an edge thumb steers. On the right half, a tuck lever holds a full tuck under the thumb, stands the skier up as the thumb slides down, and skids him below that. A flick or a tap-and-hold on the lever jumps, and two corner buttons reset and change the camera. All of it is bound by the default bindings.

The engine's touch controls gain:

- a stick whose centre trails a thumb past its reach;
- a lever with a second range under the first (`touch:<lever>-`);
- gestures that watch another control: a flick, and a tap-and-hold.

`unbias_gyro` takes a gyro's bias while the device is still and takes it off every reading. The `by-hand` lab now also skis the bot's run on two thumbs, and `input-session` charts a gyro's bias settling.
