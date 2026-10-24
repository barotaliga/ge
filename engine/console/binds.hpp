// BINDS: which command a key runs, in which mode. A bind is text (`bind space +jump`), so a
// player's whole control scheme is part of their cfg.
//
//   - KEYS are named, lower case: `a`…`z`, `0`…`9`, `f1`…`f12`, `space`, `enter`, `tab`,
//     `escape`, `backspace`, the arrows (`up`, `down`, `left`, `right`), the modifiers (`shift`,
//     `ctrl`, `alt`), `ins` `del` `home` `end` `pgup` `pgdn`, the keypad (`kp_0`…`kp_9`,
//     `kp_enter`, `kp_plus`, `kp_minus`, `kp_star`, `kp_slash`, `kp_del`), the punctuation keys by
//     their character (`[`, `]`, `,`, `.`, `/`, `\`, `'`, `-`, `=`, `` ` ``) and as `semicolon`;
//     the mouse (`mouse1`…`mouse8`, `mwheelup`, `mwheeldown`) and the gamepad
//     (`pad_a` `pad_b` `pad_x` `pad_y`, `pad_lb` `pad_rb` `pad_lt` `pad_rt`, `pad_back`
//     `pad_start`, `pad_ls` `pad_rs`, `pad_up` `pad_down` `pad_left` `pad_right`).
//   - A MODIFIER is written before the key: `ctrl+s`, `alt+shift+f1` (kept in the order ctrl,
//     alt, shift). A key held with modifiers runs the modified bind if there is one, else its own.
//   - MODES: `game` (the default), `console`, `menu` and `demo` each keep a table; `bind -menu
//     escape togglemenu` binds in one (docs/console.md § Binds).
//   - A bind starting with `+` runs that command when the key goes down and the same command with
//     `-` when it comes up (a held button); any other bind runs on the press alone.
//
// ge:summary Binds: key names, modifiers, the bind tables by mode, press and release
// ge:keywords bind binds key keys key-name modifier ctrl alt shift mode game menu demo
// ge:keywords plus-command button held press release normalize_key mouse gamepad
// ge:skill console
// ge:test tests/console_binds_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace ge::console {

enum class BindMode : uint8_t { game, console, menu, demo };
inline constexpr int kBindModes = 4;

// "game", "console", "menu", "demo".
std::string_view mode_name(BindMode m);
// The mode named `name` (without its `-`); false when there is none.
bool parse_mode(std::string_view name, BindMode& out);

// Whether `key` (lower case, no modifiers) names a key.
bool known_key(std::string_view key);

// `name` in canonical form: lower case, modifiers in the order ctrl, alt, shift, each known, and
// a known key last. False (with `out` empty) when it names no key.
bool normalize_key(std::string_view name, std::string& out);

class Binds {
 public:
  // The command bound to `key` (normalized) in `mode`; empty when none.
  const std::string& get(BindMode mode, std::string_view key) const;
  void set(BindMode mode, const std::string& key, std::string_view command);
  bool remove(BindMode mode, const std::string& key);
  void clear();  // every mode
  // Each mode's table, by key.
  const std::map<std::string, std::string>& table(BindMode mode) const {
    return tables_[static_cast<size_t>(mode)];
  }

 private:
  std::array<std::map<std::string, std::string>, kBindModes> tables_;
};

}  // namespace ge::console
