// Presets: the grid's words, a level applied as plain sets, the level computed back (custom when
// none matches), the nearest level, the command's answers, locks, and the table's check.
//
// ge:summary Tests presets: a level written as cvars, the level computed back, the command, the check
// ge:keywords test preset apply_preset preset_level nearest_level add_preset_command check_preset
// ge:skill console testing
#include "check.hpp"
#include "console/console.hpp"
#include "console/preset.hpp"

using namespace ge::console;

namespace {

constexpr PresetRow kRows[] = {
    {"r_shade", "0 1 1"},
    {"r_scale", "0.5 0.5 1"},
    {"r_water", "low  high high"},
};
constexpr PresetTable kTable{"r_quality", 3, kRows};

void add_rows(Cvars& cv) {
  cv.add({.name = "r_shade", .fallback = "1", .flags = kArchive, .kind = CvarKind::boolean});
  cv.add({.name = "r_scale", .fallback = "1", .flags = kArchive, .min = 0.25, .max = 1});
  cv.add({.name = "r_water", .fallback = "high", .flags = kArchive, .kind = CvarKind::text});
}

}  // namespace

TEST(the_grid_is_read_a_word_a_level) {
  CHECK(preset_value(kRows[2], 0) == "low");
  CHECK(preset_value(kRows[2], 1) == "high");
  CHECK(preset_value(kRows[1], 2) == "1");
  CHECK(preset_value(kRows[1], 3).empty());
}

TEST(a_level_is_plain_sets_and_the_level_is_computed_back) {
  Cvars cv;
  add_rows(cv);
  CHECK(preset_level(cv, kTable) == 2);  // the defaults are the top
  CHECK(apply_preset(cv, kTable, 0, Source::local) == 3);
  CHECK(cv.find("r_shade")->value == "0" && cv.find("r_water")->value == "low");
  CHECK(preset_level(cv, kTable) == 0);
  CHECK(apply_preset(cv, kTable, 1, Source::local) == 2);  // r_scale already 0.5
  CHECK(preset_level(cv, kTable) == 1);
  cv.set("r_scale", "0.50", Source::local);  // numbers as numbers
  CHECK(preset_level(cv, kTable) == 1);
  cv.set("r_water", "low", Source::local);  // the player's own mix: custom
  CHECK(preset_level(cv, kTable) == -1);
  std::vector<std::string_view> away;
  CHECK(nearest_level(cv, kTable, &away) == 0);  // only r_shade off level 0; level 1 has r_water
  CHECK(away.size() == 1 && away[0] == "r_shade");
  CHECK(apply_preset(cv, kTable, 3, Source::local) == -1);
}

TEST(the_command_applies_and_answers) {
  Console con;
  add_rows(con.cvars());
  add_preset_command(con, kTable, "The picture's quality, 0 to 2.");
  con.execute("r_quality");
  CHECK_MSG(con.output().back() == "r_quality is 2.", con.output().back());
  con.execute("r_quality 0");
  CHECK_MSG(con.output().back() == "r_quality 0: 3 settings changed.", con.output().back());
  CHECK(con.cvars().find("r_scale")->value == "0.5");
  con.execute("r_shade 1; r_scale 1; r_quality");
  CHECK_MSG(con.output().back() == "r_quality is custom: nearest is 2, but for r_water.",
            con.output().back());
  con.execute("r_quality 5");
  CHECK_MSG(con.output().back() == "r_quality takes a level from 0 to 2.", con.output().back());
  con.execute("r_quality 1.5");
  CHECK(con.output().back() == "r_quality takes a level from 0 to 2.");
}

TEST(a_locked_row_is_named_and_the_rest_are_set) {
  Console con;
  add_rows(con.cvars());
  add_preset_command(con, kTable, "The picture's quality, 0 to 2.");
  CvarLock lock;
  lock.owner = "ruleset test";
  lock.why = "r_shade is locked by the test ruleset.";
  lock.value = "1";
  CHECK(con.cvars().lock("r_shade", lock));
  con.execute("r_quality 0");
  const auto& out = con.output();
  CHECK_MSG(out[out.size() - 2] == "r_shade is locked by the test ruleset.", out[out.size() - 2]);
  CHECK_MSG(out.back() == "r_quality 0: 2 settings changed.", out.back());
}

TEST(the_check_names_every_fault) {
  Cvars cv;
  add_rows(cv);
  CHECK(check_preset(cv, kTable).empty());
  constexpr PresetRow kBad[] = {
      {"r_none", "0 0 0"},
      {"r_shade", "0 1"},
      {"r_scale", "0.1 0.5 1"},
      {"r_water", "a b c"},  // text: any word
  };
  const auto faults = check_preset(cv, PresetTable{"r_quality", 3, kBad});
  CHECK_MSG(faults.size() == 3, std::to_string(faults.size()));
  CHECK(faults[0].find("r_none: no such cvar") != std::string::npos);
  CHECK(faults[1].find("needs exactly 3 words") != std::string::npos);
  CHECK(faults[2].find("level 0's \"0.1\"") != std::string::npos);
}
