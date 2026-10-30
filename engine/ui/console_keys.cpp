// The console at the keyboard (console_keys.hpp).
//
// ge:summary Feeds a frame's key and text events to the console's binds and the view's input line
// ge:keywords ui console keyboard keys text edit-key bind modifier repeat utf-8 feed_console
// ge:skill ui console
// ge:test tests/ui_console_keys_test.cpp
#include "ui/console_keys.hpp"

#include <string>
#include <string_view>

#include "console/binds.hpp"

namespace ge::ui {
namespace {

namespace con = ge::console;

constexpr uint16_t kGrave = 53;   // the key under the tilde
constexpr uint16_t kNonUs = 100;  // the ISO key, the key under Escape on some Macs (binds.cpp)

// The code point as UTF-8.
std::string utf8(uint32_t cp) {
  std::string s;
  if (cp < 0x80) {
    s += static_cast<char>(cp);
  } else if (cp < 0x800) {
    s += static_cast<char>(0xc0 | (cp >> 6));
    s += static_cast<char>(0x80 | (cp & 0x3f));
  } else if (cp < 0x10000) {
    s += static_cast<char>(0xe0 | (cp >> 12));
    s += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    s += static_cast<char>(0x80 | (cp & 0x3f));
  } else if (cp < 0x110000) {
    s += static_cast<char>(0xf0 | (cp >> 18));
    s += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
    s += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    s += static_cast<char>(0x80 | (cp & 0x3f));
  }
  return s;
}

// The key's bind name with the held modifiers before it (none before a modifier itself).
std::string spelled(std::string_view key, const ConsoleKeys& k) {
  if (key == "ctrl" || key == "alt" || key == "shift") return std::string(key);
  std::string s;
  if (k.ctrl) s += "ctrl+";
  if (k.alt) s += "alt+";
  if (k.shift) s += "shift+";
  return s + std::string(key);
}

}  // namespace

std::optional<con::EditKey> edit_key(uint16_t usage) {
  switch (usage) {
    case 40:  // enter
    case 88:  // kp_enter
      return con::EditKey::enter;
    case 41:
      return con::EditKey::escape;
    case 42:
      return con::EditKey::backspace;
    case 43:
      return con::EditKey::tab;
    case 74:
      return con::EditKey::home;
    case 75:
      return con::EditKey::page_up;
    case 76:
      return con::EditKey::del;
    case 77:
      return con::EditKey::end;
    case 78:
      return con::EditKey::page_down;
    case 79:
      return con::EditKey::right;
    case 80:
      return con::EditKey::left;
    case 81:
      return con::EditKey::down;
    case 82:
      return con::EditKey::up;
    default:
      return std::nullopt;
  }
}

void feed_console(con::Console& c, con::ConsoleView& view, const input::Queue& queue,
                  ConsoleKeys& keys) {
  keys.held = view.open();
  bool toggled = false;  // the tilde went down this frame: its character types nothing
  for (const input::Event& e : queue.events) {
    if (e.type == input::EventType::text) {
      const auto cp = static_cast<uint32_t>(e.v[0]);
      const bool own = cp == '`' || cp == '~' || cp == 0xa7 || cp == 0xbd || cp == '<' || cp == '>';
      if (view.open() && !(toggled && own)) view.type(utf8(cp));  // § ½ < > on the ISO key
      continue;
    }
    if (e.type != input::EventType::key) continue;
    const bool down = e.v[0] > 0.5f;
    const std::string_view key = con::usage_key(e.code);
    if (key == "ctrl") keys.ctrl = down;
    if (key == "alt") keys.alt = down;
    if (key == "shift") keys.shift = down;
    const bool open = view.open();
    if (open && down) {
      if (const auto edit = edit_key(e.code)) view.key(*edit);
    }
    if (key.empty() || (down && e.aux != 0)) continue;  // a repeat edits; it presses no bind
    if (down && (e.code == kGrave || e.code == kNonUs)) toggled = true;
    if (down) {
      c.key(spelled(key, keys), true, open ? con::BindMode::console : con::BindMode::game);
    } else {
      // The console keeps a held `+` bind by the name it went down with, whatever modifiers were
      // held then; a release under every spelling finds it (the others are not held: no-ops).
      for (int m = 0; m < 8; ++m)
        c.key(spelled(key, {.ctrl = (m & 1) != 0, .alt = (m & 2) != 0, .shift = (m & 4) != 0}),
              false);
    }
    c.run();  // a toggle takes effect before the next event
    keys.held = keys.held || view.open();
  }
  c.run();
}

}  // namespace ge::ui
