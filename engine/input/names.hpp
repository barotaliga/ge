// THE NAMES: every key, button, axis and kind as text, both ways. The bindings file
// (actions.hpp), the event log (event_log.hpp), a lab's table and a rebinding prompt all spell a
// control the same way, from here.
//
// KEYS are named by their USB HID usage (the physical key, whatever the layout prints on it):
// `a`..`z`, `0`..`9`, `enter`, `left_shift`, `kp_7`, `f12`. A usage with no name here is
// `key<usage>` ("key135"), so every key the platform reports can be bound and read back.
//
// A GAMEPAD BUTTON's name is positional (`south`); `pad_label` is what the device's family
// prints on it (Family::playstation's south is "cross"), for the prompt a HUD draws.
//
// ge:summary Every key, mouse button, pad button, pad axis and device kind named, both ways
// ge:keywords input names key-name hid usage scancode key_name key_from_name glyph label
// ge:keywords prompt pad_label button-label family face-buttons cross circle square triangle
// ge:keywords kind_name pad_button_name pad_axis_name mouse_button_name usb_key
// ge:skill input
// ge:test tests/input_names_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "input/device.hpp"
#include "input/event.hpp"

namespace ge::input {

// HID usages the engine's own code refers to (keys.hpp would be every one; these are the
// common ones, and `key_from_name` reads any).
namespace key {
inline constexpr uint16_t a = 4, c = 6, d = 7, e = 8, f = 9, q = 20, r = 21, s = 22, w = 26;
inline constexpr uint16_t enter = 40, escape = 41, backspace = 42, tab = 43, space = 44;
inline constexpr uint16_t right = 79, left = 80, down = 81, up = 82;
inline constexpr uint16_t left_ctrl = 224, left_shift = 225, left_alt = 226;
inline constexpr uint16_t right_ctrl = 228, right_shift = 229, right_alt = 230;
}  // namespace key

std::string key_name(uint16_t usage);
std::optional<uint16_t> key_from_name(std::string_view name);

std::string_view pad_button_name(PadButton b);
std::optional<PadButton> pad_button_from_name(std::string_view name);
std::string_view pad_axis_name(PadAxis a);
std::optional<PadAxis> pad_axis_from_name(std::string_view name);
std::string_view mouse_button_name(MouseButton b);
std::optional<MouseButton> mouse_button_from_name(std::string_view name);

std::string_view kind_name(DeviceKind k);
std::optional<DeviceKind> kind_from_name(std::string_view name);
std::string_view family_name(Family f);
std::optional<Family> family_from_name(std::string_view name);
std::string_view event_type_name(EventType t);
std::optional<EventType> event_type_from_name(std::string_view name);

// What the device's family prints on a pad button ("A", "cross", "B"), for a prompt.
std::string_view pad_label(Family f, PadButton b);

}  // namespace ge::input
