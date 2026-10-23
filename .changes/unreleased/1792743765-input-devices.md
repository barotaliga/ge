---
type: Added
title: Input devices, actions and rebinding
---

`ge::input` (`engine/input`, `docs/input.md`) is the engine's input layer: one 32-byte event for every device (keyboards, raw mice, gamepads with rumble, gyro, touchpad and lightbar, joysticks, flight sticks, throttles, wheels and pedals, touch), the state a frame reads, dead zones (axial, radial, scaled radial, sloped) with curves and key ramps, action maps in contexts that combine every source, a bindings file and in-game rebinding with conflicts, a device database of axis roles with a learn step, on-screen touch controls, local players joining by device, rumble and force feedback as pure functions, a simulated backend and a recordable event log; `play/devices.hpp` feeds actions to a game's play axes. The `input-shape`, `input-ffb` and `input-session` labs chart and replay it.
