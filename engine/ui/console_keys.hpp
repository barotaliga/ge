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
// THE CHAT LINE (a `messagemode`, console/talk.hpp) takes the typed text and the edit keys the
// same way while it is open, the binds then read from the `console` table; the letter of the key
// that opened it types nothing on that frame.
//
// The game reads its own keys only when `ConsoleKeys::held` was false for the frame: a key
// pressed to type a command must not also steer. The window turns text input on while the view
// is down (platform/devices.hpp's `set_text_input`).
//
// A MENU UP (`ConsoleKeys::menu`, the window's to set): the menu has the keys, so only F1 to F12
// and the key under the tilde run their binds; every other key goes to the menu alone. A key let
// go always releases its bind, so a `+` bind held when the menu opened is not left down.
//
// ge:summary The console at the keyboard: keys to binds, edit keys and typed text to the view
// ge:keywords ui console keyboard keys text typing edit-key toggleconsole tilde bind modifier
// ge:keywords feed_console console-keys repeat utf-8 chat-line messagemode menu-keys function-keys
// ge:skill ui console
// ge:test tests/ui_console_keys_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "console/view.hpp"
#include "input/state.hpp"

namespace ge::ui {

// The modifiers held, kept between frames, and whether the console held the keyboard this frame.
struct ConsoleKeys {
  bool ctrl = false, alt = false, shift = false;
  bool cmd = false;  // the Command (GUI) key: a Mac's paste is Command-V
  // The clipboard's text for a paste (Command-V or Ctrl-V while typing); null: no paste. The
  // platform's `clipboard_text` in a window, a fixed string in a test.
  std::function<std::string()> paste = {};
  // The selected scrollback text put on the clipboard (Command-C or Ctrl-C while the console has a
  // selection, ConsoleView::selected_text); null: no copy.
  std::function<void(const std::string&)> copy = {};
  bool held = false;  // the view was down at some point this frame: the game reads no keys
  bool menu = false;  // a menu has the keys: only F1..F12 (and the tilde's key) run their binds
};

// The input line's key for a keyboard key by its HID usage; none for a key it does not edit.
std::optional<console::EditKey> edit_key(uint16_t usage);

// Text pasted into the input line as one line: each line break a `;` (a pasted cfg runs line by
// line), other control characters dropped.
std::string paste_line(std::string_view text);

// Feed `queue`'s keyboard and text events, in order, to `c`'s binds and `view`'s input line.
void feed_console(console::Console& c, console::ConsoleView& view, const input::Queue& queue,
                  ConsoleKeys& keys);

}  // namespace ge::ui
