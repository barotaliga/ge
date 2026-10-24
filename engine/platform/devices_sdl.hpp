// The devices' half of the event pump, for window.cpp: the one pump reads the window's events
// and hands every other one to the devices' translation. Internal to engine/platform (it
// includes SDL); nothing outside this directory includes it.
//
// ge:summary The devices' event translation as window.cpp's pump calls it (internal, SDL)
// ge:keywords platform sdl event-pump devices translate internal
// ge:skill input platform
// ge:test tests/platform_devices_test.cpp
#pragma once

#include <SDL3/SDL.h>

#include "input/state.hpp"
#include "platform/devices.hpp"

namespace ge::platform {

// Before a frame's events: the pooled keyboard and mouse plugged on the first pump.
void devices_begin(Devices& devices, input::Queue& queue);
// One SDL event: true when it was a device's (and is now on the queue).
bool devices_event(Devices& devices, const SDL_Event& e, input::Queue& queue);

}  // namespace ge::platform
