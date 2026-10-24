// Binds: key names, modifiers in canonical order, the tables by mode.
//
// ge:summary Tests binds: key names, modifier order, modes, press and release of + commands
// ge:keywords test bind key-name normalize_key modifier mode
// ge:skill console testing
#include "check.hpp"
#include "console/binds.hpp"
#include "console/console.hpp"

using namespace ge::console;

TEST(key_names) {
  std::string k;
  CHECK(normalize_key("SPACE", k) && k == "space");
  CHECK(normalize_key("shift+ctrl+A", k) && k == "ctrl+shift+a");
  CHECK(normalize_key("alt+f12", k) && k == "alt+f12");
  CHECK(normalize_key("mouse8", k) && normalize_key("kp_0", k) && normalize_key("pad_rt", k));
  CHECK(normalize_key("-", k) && k == "-");
  CHECK(!normalize_key("f13", k) && !normalize_key("f0", k) && !normalize_key("mouse9", k));
  CHECK(!normalize_key("f01", k) && !normalize_key("meta+a", k) && !normalize_key("ctrl+", k));
  CHECK(!normalize_key("", k) && k.empty());
}

TEST(modes_keep_their_own_tables) {
  Console c;
  c.execute("bind escape togglemenu; bind -menu escape \"menu_back\"; bind -demo space");
  CHECK(c.binds().get(BindMode::game, "escape") == "togglemenu");
  CHECK(c.binds().get(BindMode::menu, "escape") == "menu_back");
  CHECK(c.output().back() == "space is not bound");
  c.execute("unbind -menu escape");
  CHECK(c.binds().get(BindMode::menu, "escape").empty());
  CHECK(c.binds().get(BindMode::game, "escape") == "togglemenu");
}

TEST(plus_binds_run_both_edges_once) {
  Console c;
  Button& fire = c.add_button("attack", "fire while held");
  c.execute("bind mouse1 +attack");
  c.key("mouse1", true, BindMode::game);
  c.key("mouse1", true, BindMode::game);  // a repeat: no second press
  c.run();
  CHECK(fire.held == 1);
  // Rebinding while held still releases what went down.
  c.execute("bind mouse1 \"echo other\"");
  c.key("mouse1", false);
  c.run();
  CHECK(fire.held == 0);
  // A key with no bind in this mode does nothing.
  c.key("mouse1", true, BindMode::menu);
  c.run();
  CHECK(c.output().empty());
}
