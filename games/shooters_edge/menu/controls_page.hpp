// CUSTOMIZE CONTROLS' MODEL: the moves its rows show (`controls()`, the row `c<i>` the i-th), the
// keys the game's binds give each, and a key bound to a move or a move's keys cleared, the way the
// classic page does it (a third key replaces both). The page itself is built and refreshed with
// the others (main_menu.hpp); waiting for the key is the game menu's (game_menu.hpp).
//
// ge:summary CUSTOMIZE CONTROLS: the moves, the keys bound to each, binding and clearing a move
// ge:keywords customize-controls controls bind_control unbind_control bound_keys keys_text binds
// ge:skill ui console input
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace shooters_edge::menu {

// CUSTOMIZE CONTROLS: one move a row, the command a key is bound to and the row's words.
struct Control {
  std::string_view command = {};
  std::string_view text = {};
};
std::span<const Control> controls();
// The keys bound to exactly `command` in the game's binds, in the binds' order.
std::vector<std::string> bound_keys(const ge::console::Binds& binds, std::string_view command);
// A row's value: the keys upper case, as many as fit `kKeysChars` ("W, UP"), the wheel's as
// WHEEL UP and WHEEL DN; "---" with none.
inline constexpr size_t kKeysChars = 12;
std::string keys_text(const std::vector<std::string>& keys);
// `key` (a bind name: `w`, `mouse1`, `mwheelup`) bound to `command`; a move that had two keys
// already loses them first, as the classic menu does. False when `key` names no key.
bool bind_control(ge::console::Console& con, std::string_view command, std::string_view key);
// Every key bound to `command` unbound.
void unbind_control(ge::console::Console& con, std::string_view command);
// The row `c<i>`'s move; null when `id` names none.
const Control* control_of(std::string_view id);

}  // namespace shooters_edge::menu
