// Tests: a map's intent drafted from its items (plugins/shooter/intent_draft.hpp): two rooms and
// a corridor compiled from a plan, the red armour's room a camp boxed to that room alone, the mega
// health and the launcher in the other room one power area naming both, and the text read back as
// an .intent the labs take.
//
// ge:summary Tests: a map's intent drafted from its items: rooms boxed, items merged, it reads back
// ge:keywords test intent draft-intent auto-intent area camp power room flood-fill
// ge:skill map-design shooter testing
#include <algorithm>
#include <string>
#include <vector>

#include "brush/compile/compile.hpp"
#include "brush/compile/plan.hpp"
#include "brush/read.hpp"
#include "check.hpp"
#include "shooter/intent_draft.hpp"

namespace br = ge::brush;
namespace mc = ge::brush::mapc;
namespace sh = ge::shooter;

namespace {

// West room 0..384, east room 640..1024, a corridor between at y 160..224.
constexpr const char* kPlan = R"(room west z=0..192 box 0,0 384,384
room east z=0..192 box 640,0 1024,384
room hall z=0..128 box 384,160 640,224
spawn 64,64,24
entity item_armorInv 192,192,24
entity item_health 832,96,24 spawnflags=2
entity weapon_rocketlauncher 832,288,24
light 192,192,160
light 832,192,160
)";

bool read_map(br::Map& m) {
  mc::Source src;
  std::string err;
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  mc::CompileOptions opt;
  opt.light = false;
  CHECK_MSG(mc::read_plan(kPlan, &src, &err), err);
  CHECK_MSG(mc::compile_map(src, opt, &bytes, &rep, &err), err);
  CHECK_MSG(br::read_bsp(bytes, &m, &err), err);
  return true;
}

}  // namespace

TEST(each_power_and_weapon_room_is_an_area_boxed_to_its_room) {
  br::Map m;
  read_map(m);
  const std::string text = sh::draft_intent(m);
  mc::PlanIntent in;
  std::string err;
  CHECK_MSG(mc::read_intent(text, &in, &err), err + "\n" + text);
  CHECK_MSG(in.areas.size() == 2, text);
  bool camp = false, power = false;
  for (const mc::PlanArea& a : in.areas) {
    double x0 = 1e9, x1 = -1e9;
    for (const auto& [x, y] : a.poly) x0 = std::min(x0, x), x1 = std::max(x1, x);
    if (a.intent == "camp") {
      camp = true;
      CHECK_MSG(a.name == "ra", text);
      CHECK_MSG(x0 >= -8 && x1 <= 384 + 260, text);  // the west room and some of the corridor
      CHECK(mc::area_holds(a, 192, 192, 24));
    }
    if (a.intent == "power") {
      power = true;
      CHECK_MSG(a.note.find("mega health") != std::string::npos, text);
      CHECK_MSG(a.note.find("rocket launcher") != std::string::npos, text);
      CHECK(mc::area_holds(a, 832, 96, 24) && mc::area_holds(a, 832, 288, 24));
      CHECK_MSG(x1 <= 1024 + 8, text);
    }
  }
  CHECK(camp && power);
}

TEST(a_map_without_the_items_drafts_no_area) {
  br::Map m;
  read_map(m);
  m.entities.erase(m.entities.begin() + 1, m.entities.end());
  const std::string text = sh::draft_intent(m);
  CHECK(text.find("\narea ") == std::string::npos);
}
