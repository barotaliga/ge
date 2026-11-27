// A menu's keys off the window's frame: the arrows and WASD move, ENTER and SPACE choose, ESCAPE
// goes back, DELETE (a Mac's BACKSPACE) removes, R and F5 refresh, and the first of a page's own
// `letters` tapped (ui::MenuInput::letter). The pointer is the caller's: a game decides when its
// pointer is the menu's (a held mouse is not).
//
// ge:summary A menu's keys read off the window's frame: the arrows, WASD, enter, escape, letters
// ge:keywords menu-keys menu-input arrows wasd enter escape delete refresh letters tapped_letter
// ge:skill platform ui
#pragma once

#include <string_view>

#include "platform/window.hpp"
#include "ui/menu.hpp"

namespace ge::platform {

inline void menu_keys(const InputSnapshot& in, std::string_view letters, ui::MenuInput& mi) {
  mi.up = in.tapped(Key::up) || in.tapped(Key::w);
  mi.down = in.tapped(Key::down) || in.tapped(Key::s);
  mi.left = in.tapped(Key::left) || in.tapped(Key::a);
  mi.right = in.tapped(Key::right) || in.tapped(Key::d);
  mi.accept = in.tapped(Key::enter) || in.tapped(Key::space);
  mi.back = in.tapped(Key::escape);
  mi.del = in.tapped(Key::del) || in.tapped(Key::backspace);
  mi.refresh = in.tapped(Key::r) || in.tapped(Key::f5);
  mi.letter = in.tapped_letter(letters);
}

}  // namespace ge::platform
