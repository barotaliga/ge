// LAB `se-remix-items`: THE ITEMS A REMIX LAYS, TAKEN BY THEIR OWN GAME'S RULES (classic remix's
// `cr_items`, plugins/classic/q1_body.hpp and q2_body.hpp). Each armour, health and ammunition
// item of one game, played on the other game's rules, picked up by a fresh player (and the armour
// over the one worn): what it gave when the played rules took it by their own numbers under the
// other game's model (before), and what it gives now, by the rules of the game it belongs to.
// Quake II's rules are run for real (its item table and body); the classic rules' rows are read
// as the server is given them.
//
// ge:summary Lab: remixed armour, health and ammo taken by their own game's rules, before and now
// ge:keywords lab se-remix-items cr_items armour health ammo q1-body q2-body real-items reskin
// ge:skill shooter labs
// ge:test tests/shooters_edge_remix_bodies_test.cpp
#include <string>
#include <vector>

#include "classic/q1_body.hpp"
#include "classic/q2_body.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/profiles/arsenal.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_quake_arms.hpp"

namespace {

namespace gl = ge::lab;
namespace gc = ge::classic;
namespace pf = shooters_edge::profiles;
namespace sv = shooters_edge::server;

const gl::Rgba kPaper{246, 244, 238, 255};
const gl::Rgba kInk{30, 30, 34, 255};
const gl::Rgba kFaint{120, 120, 120, 255};
const gl::Rgba kBefore{40, 96, 150, 255};
const gl::Rgba kNow{150, 82, 40, 255};

struct Row {
  std::string item, played, before, now;
};

// Quake II's rules wearing Quake's things: a fresh Quake body takes Quake II's item `name`
// (worn first: `over`), and what it then holds.
std::string q2_rules_take(const char* name, const char* over, bool quake, bool armour) {
  sv::SqBody b = sv::sq_new_body();
  if (quake) sv::sq_quake_body(b);
  if (!armour) b.health = 50;
  if (over != nullptr) sv::sq_pick_up(sv::sq_item(over), b, true, 0, 0);
  const int had = b.armour_count;
  const bool took = sv::sq_pick_up(sv::sq_item(name), b, true, 0, 0);
  if (!armour) return took ? "health 50 -> " + std::to_string(b.health) : "left lying";
  if (!took) return "left lying (" + std::to_string(had) + ")";
  return std::to_string(had) + " -> " + std::to_string(b.armour_count);
}

void run(gl::Lab& lab) {
  std::vector<Row> rows;
  // Quake's things on Quake II's rules.
  const char* jacket = "item_armor_jacket";
  const char* combat = "item_armor_combat";
  const char* body = "item_armor_body";
  rows.push_back({"green armour", "Quake II", q2_rules_take(jacket, nullptr, false, true),
                  q2_rules_take(jacket, nullptr, true, true)});
  rows.push_back({"yellow over green", "Quake II", q2_rules_take(combat, jacket, false, true),
                  q2_rules_take(combat, jacket, true, true)});
  rows.push_back({"green over red", "Quake II", q2_rules_take(jacket, body, false, true),
                  q2_rules_take(jacket, body, true, true)});
  rows.push_back({"small health", "Quake II",
                  q2_rules_take("item_health_small", nullptr, false, false),
                  q2_rules_take("item_health_small", nullptr, true, false)});
  rows.push_back({"large health", "Quake II",
                  q2_rules_take("item_health_large", nullptr, false, false),
                  q2_rules_take("item_health_large", nullptr, true, false)});
  // Quake II's things on Quake's rules: the rows the classic rules read.
  const pf::RemixArsenal* worn = pf::remix_arsenal("q1", "q2");
  if (worn == nullptr) {
    lab.fail("no remix row for Quake's rules wearing Quake II's things",
             "Read profiles/remix_arsenal.hpp's kRemixArsenals. Skill: shooter.");
    return;
  }
  const char* names[3] = {"jacket armour", "combat armour", "body armour"};
  for (size_t i = 0; i < 3; i++) {
    const pf::ArmourRow& now = worn->arsenal.armour[i];
    rows.push_back(
        {names[i], "Quake", std::to_string(pf::kQ1Armour[i].amount) + ", replacing the worn one",
         std::to_string(now.amount) + ", adding onto it up to " + std::to_string(now.most)});
  }
  const gc::Q2Armour over = gc::q2_take_armour(3, 100, 1);
  rows.push_back({"jacket over body 100", "Quake", "left lying (100)",
                  "100 -> " + std::to_string(over.count)});
  const int quake_points[3] = {gc::kQ1HealthSmall, gc::kQ1HealthPlain, gc::kQ1HealthMega};
  constexpr const char* kHealthNames[3] = {"medium health (small)", "large health (plain)",
                                           "mega health"};
  constexpr const char* kAmmoNames[4] = {"shells box", "bullets box (nails)", "rockets box",
                                         "slugs box (cells)"};
  const char* quake_ammo[4] = {"20 shells", "25 nails", "5 rockets", "6 cells"};
  for (const pf::ItemLook& l : worn->arsenal.looks) {
    const std::string setting(l.setting);
    const char last = setting.back();
    if (setting.starts_with("arsenal_health"))
      rows.push_back({std::string(kHealthNames[last - '1']), "Quake",
                      std::to_string(quake_points[last - '1']), std::to_string(l.amount)});
    else
      rows.push_back({std::string(kAmmoNames[last - '1']), "Quake", quake_ammo[last - '1'],
                      std::to_string(l.amount)});
  }

  constexpr int kRow = 22, kW = 1100;
  gl::Picture pic = gl::create_picture(kW, static_cast<int>(rows.size() + 2) * kRow + 12, kPaper);
  const int cols[4] = {8, 230, 380, 720};
  gl::draw_text(pic, cols[0], 14, "ITEM (the other game's)", kInk);
  gl::draw_text(pic, cols[1], 14, "PLAYED ON", kInk);
  gl::draw_text(pic, cols[2], 14, "BEFORE: the played rules' numbers", kBefore);
  gl::draw_text(pic, cols[3], 14, "NOW: its own game's rules", kNow);
  std::vector<std::vector<std::string>> table;
  for (size_t i = 0; i < rows.size(); i++) {
    const int y = static_cast<int>(i + 1) * kRow + 8;
    if (i % 2 == 0) gl::fill_rect(pic, 0, y - 4, kW, kRow, {236, 233, 224, 255});
    const Row& r = rows[i];
    gl::draw_text(pic, cols[0], y + 10, r.item, kInk);
    gl::draw_text(pic, cols[1], y + 10, r.played + "'s rules", kFaint);
    gl::draw_text(pic, cols[2], y + 10, r.before, kBefore);
    gl::draw_text(pic, cols[3], y + 10, r.now, kNow);
    table.push_back({r.item, r.played, r.before, r.now});
  }
  lab.shot({"remix-items",
            "THE REMIXED ITEMS BY THEIR OWN RULES",
            "Each armour, health and ammunition item of one game played on the other game's "
            "rules: what it gave by the played rules' numbers under the other's model, and what it "
            "gives now by the rules of the game it belongs to.",
            "every NOW is the item's own game's number: Quake's armour 100/150/200 replacing, "
            "Quake II's 25/50/100 adding up to 50/100/200, each game's own boxes",
            {}},
           pic);
  lab.metric("rows", static_cast<double>(rows.size()), "items");
  lab.table({"items by their own rules", {"item", "played on", "before", "now"}, table});
}

const gl::LabSpec kSpec{
    "se-remix-items",
    gl::LabKind::sim,
    "game:shooters_edge",
    "plugins/classic/q1_body, q2_body: remixed armour, health and ammo by their own rules",
    "shooter",
    "Does every armour, health and ammunition item a remix lays give what its own game's rules "
    "give, where it gave the played game's numbers before?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
