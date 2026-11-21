// games/shooters_edge/client/team_items: the item sets' names, groups and defaults; a list
// replaces, + and - keep and change with a sign that holds, `none` empties, a word it does not
// know is said; deathmatch 4 leaves ammunition and weapons out of a pickup; an item with no
// classic name always counts; tp_took runs f_took and tp_pickup only remembers.
//
// ge:summary Tests the team's item sets: tp_took, tp_pickup, tp_point
// ge:keywords test tp_took tp_pickup tp_point item-set apply_items
// ge:skill shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/team_chat.hpp"
#include "shooters_edge/client/team_items.hpp"

namespace cl = shooters_edge::client;

TEST(the_sets_take_names_groups_and_signs) {
  cl::ItemSet s = cl::default_items(cl::ItemSetKind::took);
  CHECK(cl::items_text(s) ==
        "quad pent ring suit ra ya ga mh lg rl gl sng ssg pack cells rockets flag rune1 rune2 "
        "rune3 rune4");
  CHECK(cl::items_text(cl::default_items(cl::ItemSetKind::point))
            .find("teammate enemy eyes "
                  "sentry disp") != std::string::npos);
  CHECK(cl::apply_items(s, cl::ItemSetKind::took, {"Quad", "armor"}).empty());
  CHECK(cl::items_text(s) == "quad ra ya ga");
  CHECK(cl::apply_items(s, cl::ItemSetKind::took, {"-ra", "ya", "+mh"}).empty());
  CHECK(cl::items_text(s) == "quad ga mh");  // the sign holds until the next
  CHECK(cl::apply_items(s, cl::ItemSetKind::took, {"+players", "bogus"}) ==
        std::vector<std::string>{"bogus"});
  CHECK(cl::items_text(s) == "quad ga mh");  // no players in a pickup set
  cl::apply_items(s, cl::ItemSetKind::took, {"none"});
  CHECK(cl::items_text(s) == "none");
  cl::apply_items(s, cl::ItemSetKind::point, {"all"});
  CHECK(s == cl::kAllItems);
}

TEST(a_pickup_counts_by_its_word_and_the_deathmatch) {
  const cl::ItemSet took = cl::default_items(cl::ItemSetKind::took);
  CHECK(cl::item_counts(took, "tp_name_backpack", 1));
  CHECK(!cl::item_counts(took, "tp_name_ng", 1));
  CHECK(cl::item_counts(took, "tp_name_rl", 1) && !cl::item_counts(took, "tp_name_rl", 4));
  CHECK(cl::item_counts(took, "tp_name_bfg", 4));  // Quake II's own: always
}

TEST(tp_took_runs_f_took_and_tp_pickup_only_remembers) {
  ge::console::Console c;
  cl::TeamChat chat;
  cl::add_team_chat(c, chat);
  c.execute("tp_forceTriggers 1; alias f_took \"set took_runs 1\"; set took_runs 0");
  c.execute("tp_took quad; tp_pickup ra");
  shooters_edge::hud::Status s;
  s.health = 100;
  cl::team_frame(c, chat, s, "dm6");
  c.run();
  s.t = 1;
  s.armour = 200;  // heavy armour taken: remembered, no f_took
  cl::team_frame(c, chat, s, "dm6");
  c.run();
  CHECK(chat.took == "tp_name_ra" && c.cvars().find("took_runs")->value == "0");
  s.t = 2;
  s.armour = 0;
  cl::team_frame(c, chat, s, "dm6");
  c.run();
  c.execute("tp_pickup none; tp_took none");
  s.t = 3;
  s.armour = 200;
  cl::team_frame(c, chat, s, "dm6");
  c.run();
  CHECK(chat.took_t == 1);  // in neither set: the last one kept
  c.execute("tp_took ra");
  s.t = 4;
  s.armour = 0;
  cl::team_frame(c, chat, s, "dm6");
  c.run();
  s.t = 5;
  s.armour = 200;
  cl::team_frame(c, chat, s, "dm6");
  c.run();
  CHECK(chat.took_t == 5 && c.cvars().find("took_runs")->value == "1");
}
