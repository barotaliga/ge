// The console at the keyboard: the key under the tilde opens and closes the view, and only while
// it is down do typed text and the edit keys reach the input line; Enter runs what was typed; a
// held modifier is spelled before its key's bind; a held `+` bind is released whatever the
// modifiers do meanwhile; the game is told to read no keys while the view is down.
//
// ge:summary Tests the console's keyboard feed: the toggle, typed text, edit keys, binds, release
// ge:keywords test ui console keyboard keys text edit-key toggleconsole bind modifier usage_key
// ge:skill ui console testing
#include <string>

#include "check.hpp"
#include "console/binds.hpp"
#include "console/view.hpp"
#include "input/event.hpp"
#include "ui/console_keys.hpp"

using namespace ge;

namespace {

constexpr uint16_t kGrave = 53, kNonUs = 100, kEnter = 40, kBackspace = 42, kS = 22, kCtrl = 224;

void key(input::Queue& q, uint16_t usage, bool down, bool repeat = false) {
  q.push(input::key_event(0, 1, usage, down, repeat));
}

void text(input::Queue& q, std::string_view s) {
  for (char ch : s) q.push({0, 1, input::EventType::text, 0, 0, {static_cast<float>(ch)}});
}

}  // namespace

TEST(bind_names_by_usage) {
  CHECK(console::usage_key(4) == "a" && console::usage_key(29) == "z");
  CHECK(console::usage_key(30) == "1" && console::usage_key(39) == "0");
  CHECK(console::usage_key(53) == "`" && console::usage_key(51) == "semicolon");
  CHECK(console::usage_key(75) == "pgup" && console::usage_key(99) == "kp_del");
  CHECK(console::usage_key(229) == "shift" && console::usage_key(224) == "ctrl");
  CHECK(console::usage_key(100) == "nonus");
  CHECK(console::usage_key(50).empty() && console::usage_key(0).empty());
  // Every name it gives is one a bind takes.
  for (uint16_t u = 0; u < 256; ++u) {
    const std::string_view k = console::usage_key(u);
    if (!k.empty()) CHECK(console::known_key(k));
  }
}

TEST(the_tilde_opens_it_and_only_then_text_is_typed) {
  console::Console c;
  console::ConsoleView v(c);
  ui::ConsoleKeys keys;
  input::Queue q;
  text(q, "w");  // closed: the game's key, typed nowhere
  ui::feed_console(c, v, q, keys);
  CHECK(!v.open() && !keys.held && v.input().empty());

  q.clear();
  key(q, kGrave, true);
  text(q, "`");  // the toggle's own character
  key(q, kGrave, false);
  text(q, "/sv");
  ui::feed_console(c, v, q, keys);
  CHECK(v.open() && keys.held);
  CHECK(v.input() == "/sv");

  q.clear();
  key(q, kBackspace, true);
  key(q, kBackspace, true, true);  // a held key's repeat edits too
  ui::feed_console(c, v, q, keys);
  CHECK(v.input() == "/");

  q.clear();
  key(q, kGrave, true);
  ui::feed_console(c, v, q, keys);
  CHECK(!v.open() && keys.held);  // down for part of the frame: still the console's
  q.clear();
  ui::feed_console(c, v, q, keys);
  CHECK(!keys.held);
}

// A Mac that does not recognise its ISO keyboard reports § (under Escape) as the ISO key.
TEST(the_iso_key_opens_it_too_and_types_nothing) {
  console::Console c;
  console::ConsoleView v(c);
  ui::ConsoleKeys keys;
  input::Queue q;
  key(q, kNonUs, true);
  q.push({0, 1, input::EventType::text, 0, 0, {static_cast<float>(0xa7)}});  // §
  key(q, kNonUs, false);
  text(q, "map");
  ui::feed_console(c, v, q, keys);
  CHECK(v.open() && v.input() == "map");

  q.clear();
  key(q, kNonUs, true);
  ui::feed_console(c, v, q, keys);
  CHECK(!v.open());
}

TEST(enter_runs_the_line) {
  console::Console c;
  console::ConsoleView v(c);
  c.cvars().add({.name = "sv_deathmatch", .fallback = "0"});
  ui::ConsoleKeys keys;
  input::Queue q;
  key(q, kGrave, true);
  text(q, "/sv_deathmatch 2");
  key(q, kEnter, true);
  ui::feed_console(c, v, q, keys);
  CHECK(c.cvars().find("sv_deathmatch")->value == "2");
  CHECK(v.input().empty());
}

TEST(a_modifier_is_spelled_before_its_key_and_a_held_bind_lets_go) {
  console::Console c;
  console::ConsoleView v(c);
  c.binds().set(console::BindMode::game, "ctrl+s", "+voice_prox");
  console::Button& talk = c.add_button("voice_prox", "talk to the players near you");
  ui::ConsoleKeys keys;
  input::Queue q;
  key(q, kCtrl, true);
  key(q, kS, true);
  ui::feed_console(c, v, q, keys);
  CHECK(talk.down());
  q.clear();
  key(q, kCtrl, false);  // ctrl let go first: the bind went down as ctrl+s
  key(q, kS, false);
  ui::feed_console(c, v, q, keys);
  CHECK(!talk.down());
}
