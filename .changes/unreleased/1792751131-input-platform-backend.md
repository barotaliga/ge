---
type: Added
title: Input devices read from the OS
---

`engine/platform/devices.hpp` is `engine/input`'s platform backend over SDL3: keyboards (keys and text), mice (raw counts in relative mode, buttons, the wheel), gamepads by SDL's mapping (positional buttons and axes, family, gyro, accelerometer, touchpad, battery), joysticks, wheels, flight sticks and throttles by SDL's joystick type, touch screens and hot-plug, read in the window's pump or on their own; rumble, trigger rumble, the light, the player lights and force feedback (streamed as one constant force) played back. SDL's virtual devices drive the whole path headless, and the `input-devices` lab holds it to the simulated backend frame by frame.
