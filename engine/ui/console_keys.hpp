// THE CONSOLE AT THE KEYBOARD: one frame of the input layer's events (input/state.hpp's Queue)
// fed to the console and its drop-down view, so any game's window drives the console the same
// way. Each key goes to the console's binds by its bind name (`console::usage_key`), held
// modifiers spelled before it (`ctrl+s`), in the `console` table while the view is down and the
// `game` table while it is up; the key under the tilde is bound to `toggleconsole` in both, so
// it opens and closes the view. While the view is down the input line also takes the edit keys
// (Enter, Backspace, Delete, the arrows, Home, End, Tab, Page Up and Down, Escape; a held key's
// repeats too) and the text typed (the code points the platform's text input sends, as UTF-8;
// the tilde's own character is dropped on the frame it toggled).
//
// The game reads its own keys only when `ConsoleKeys::held` was false for the frame: a key
// pressed to type a command must not also steer. The window turns text input on while the view
// is down (platform/devices.hpp's `set_text_input`).
//
// ge:summary The console at the keyboard: keys to binds, edit keys and typed text to the view
// ge:keywords ui console keyboard keys text typing edit-key toggleconsole tilde bind modifier
// ge:keywords feed_console console-keys repeat utf-8
// ge:skill ui console
// ge:test tests/ui_console_keys_test.cpp
#pragma once

#include <cstdint>
#include <optional>

#include "console/view.hpp"
#include "input/state.hpp"

namespace ge::ui {

// The modifiers held, kept between frames, and whether the console held the keyboard this frame.
struct ConsoleKeys {
  bool ctrl = false, alt = false, shift = false;
  bool held = false;  // the view was down at some point this frame: the game reads no keys
};

// The input line's key for a keyboard key by its HID usage; none for a key it does not edit.
std::optional<console::EditKey> edit_key(uint16_t usage);

// Feed `queue`'s keyboard and text events, in order, to `c`'s binds and `view`'s input line.
void feed_console(console::Console& c, console::ConsoleView& view, const input::Queue& queue,
                  ConsoleKeys& keys);

}  // namespace ge::ui
