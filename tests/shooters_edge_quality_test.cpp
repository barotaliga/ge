// Shooter's Edge's picture quality: every row a cvar the window registers and a value it keeps,
// a fresh config reads the shipped level, every drawn classic-enhance switch is in the table,
// each level differs from its neighbours, `r_quality N` writes the settings and nothing else, and
// the menu's QUALITY row shows the level the settings make or CUSTOM.
//
// ge:summary Tests the picture quality 0 to 10: the table against the console, the menu's row
// ge:keywords test quality r_quality quality-preset custom menu-quality preset_level
// ge:skill console testing
#include <string>

#include "check.hpp"
#include "classicenhance/switches.hpp"
#include "console/console.hpp"
#include "console/preset.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/menu/main_menu.hpp"
#include "shooters_edge/menu/quality.hpp"

namespace gc = ge::console;
namespace sm = shooters_edge::menu;

namespace {

struct Window {
  shooters_edge::look::Picture picture;
  gc::Console con;
  Window() {
    shooters_edge::look::add_cvars(con, picture);
    sm::add_quality(con);
  }
};

}  // namespace

TEST(every_row_is_a_cvar_the_window_keeps) {
  Window w;
  for (const std::string& f : gc::check_preset(w.con.cvars(), sm::quality_table()))
    CHECK_MSG(false, f);
}

TEST(a_fresh_config_reads_the_shipped_level) {
  Window w;
  CHECK_MSG(gc::preset_level(w.con.cvars(), sm::quality_table()) == sm::kQualityShipped,
            std::to_string(gc::preset_level(w.con.cvars(), sm::quality_table())));
}

TEST(every_drawn_switch_is_in_the_table_and_no_bake) {
  const gc::PresetTable& t = sm::quality_table();
  for (const ge::classicenhance::SwitchSpec& s : ge::classicenhance::switches()) {
    const std::string name = "ce_" + std::string(s.id);
    bool in = false;
    for (const gc::PresetRow& r : t.rows) in = in || r.cvar == name;
    const bool wanted = !s.bake && s.group != "SOUND";  // a level never starts a bake
    CHECK_MSG(in == wanted, name + (wanted ? " is missing from the table" : " must stay out"));
  }
}

TEST(each_level_differs_from_its_neighbours_and_zero_is_classic) {
  const gc::PresetTable& t = sm::quality_table();
  for (int level = 1; level < t.levels; ++level) {
    int differ = 0;
    for (const gc::PresetRow& r : t.rows)
      differ += gc::preset_value(r, level) != gc::preset_value(r, level - 1);
    CHECK_MSG(differ > 0, "level " + std::to_string(level) + " is level " +
                              std::to_string(level - 1) + " again");
  }
  CHECK(gc::preset_value(t.rows[0], 0) == "classic");
}

TEST(a_level_writes_the_settings_and_they_stay_the_players) {
  Window w;
  w.con.execute("r_quality 4");
  CHECK(w.picture.render_scale == 1 && !w.picture.ce.shadows && !w.picture.ce.water);
  CHECK(gc::preset_level(w.con.cvars(), sm::quality_table()) == 4);
  w.con.execute("r_quality 0");
  CHECK(w.picture.look == "classic" && !w.picture.ce.textures);
  w.con.execute("ce_textures 1");  // the player's own change holds: no override
  CHECK(w.picture.ce.textures);
  w.con.execute("r_quality");
  CHECK_MSG(w.con.output().back() == "r_quality is custom: nearest is 0, but for ce_textures.",
            w.con.output().back());
}

TEST(the_menu_row_shows_the_level_or_custom) {
  Window w;
  ge::ui::Menu m = sm::build_menu();
  sm::Host host{.console = &w.con};
  sm::refresh(m, host);
  const ge::ui::Item* row = m.page("video")->find("r_quality");
  CHECK(row != nullptr && !row->disabled);
  CHECK(row->choice == sm::kQualityShipped && row->choices.size() == sm::kQualityLevels);
  w.con.execute("ce_ao 0");
  sm::refresh(m, host);
  CHECK(row->choices.back() == "CUSTOM" && row->choice == sm::kQualityLevels);
  w.con.execute("r_quality 6");
  sm::refresh(m, host);
  CHECK(row->choice == 6 && row->choices.size() == sm::kQualityLevels);
}
