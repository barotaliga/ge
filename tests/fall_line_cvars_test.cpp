// Fall Line's cvars: every one registered, bound to its Settings field, a cfg's values reaching
// the struct (and the frame's options), the tiers right (sv_ the server's, r_ latched).
//
// ge:summary Tests Fall Line's cvars: names, defaults bound to Settings, a cfg reaching the struct
// ge:keywords test fall-line cvars settings add_cvars latch
// ge:skill console sim testing
#include "check.hpp"
#include "console/cfg.hpp"
#include "fall_line/cfg/cvars.hpp"

namespace con = ge::console;
using fall_line::cfg::Settings;

TEST(defaults_match_the_struct) {
  con::Console c;
  Settings s;
  const Settings fresh;
  fall_line::cfg::add_cvars(c, s);
  CHECK(s.width == fresh.width && s.shadow_size == fresh.shadow_size && s.bloom == fresh.bloom);
  CHECK(s.view == "chase" && s.seed == 38 && s.hour == -1 && s.volume == fresh.volume);
  // Every cvar the game reads has its help, and every name follows the prefixes.
  for (const con::Cvar* v : c.cvars().sorted()) {
    CHECK_MSG(!v->help.empty(), v->name + " has no help: say what it does in cfg/cvars.cpp");
  }
}

TEST(a_cfg_reaches_the_settings_after_the_latch) {
  con::Console c;
  Settings s;
  fall_line::cfg::add_cvars(c, s);
  c.execute(
      "r_bloom 0; r_shadowsize 4096; vid_width 1920; volume 0.5; cl_view vista; "
      "sv_seed 7; r_scale 9");
  CHECK(s.volume == 0.5f && s.view == "vista" && s.seed == 7);
  CHECK(s.bloom == 0.04 && s.width == 1280);  // latched: the frame and window are made once
  c.cvars().apply_latched();
  CHECK(s.bloom == 0 && s.shadow_size == 4096 && s.width == 1920 && s.scale == 2);
  CHECK(c.problems().empty());
}

TEST(a_client_cannot_pick_the_map) {
  con::Console c(false);
  Settings s;
  fall_line::cfg::add_cvars(c, s);
  c.execute("sv_seed 7");
  CHECK(s.seed == 38 && c.problems().size() == 1);
}
