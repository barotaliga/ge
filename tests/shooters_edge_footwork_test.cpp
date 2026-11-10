// A FIGHTER'S KEYS AND BUTTONS (games/shooters_edge/bots/footwork.hpp `fighter_keys`,
// buttons.hpp `button_fires`): a side key turned round passes through 1 to 3 commands with no side
// key unless the change is a clean one, never on a proven jump; and a button's doors are found
// through the relays it fires (e1m2's plate: a delayed trigger_once opens the teleporters' doors).
//
// ge:summary Tests: the side key's change held off as a hand's is; a button's doors through relays
// ge:keywords test fighter_keys key-change socd perfect-strafe button_fires trigger_relay
// ge:skill bots testing
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/entity_text.hpp"
#include "shooters_edge/bots/buttons.hpp"
#include "shooters_edge/bots/footwork.hpp"

namespace sh = ge::shooter;
namespace fb = shooters_edge::bots;
namespace pg = ge::progs;

namespace {

// The side keys sent for `wanted`, one command each.
std::vector<int16_t> sent(fb::Fighter& f, const std::vector<int16_t>& wanted) {
  std::vector<int16_t> out;
  for (const int16_t w : wanted) {
    sh::MoveCmd c;
    c.side = w;
    fb::fighter_keys(f, c);
    out.push_back(c.side);
  }
  return out;
}

bool has(const std::vector<std::string>& v, const char* s) {
  for (const std::string& x : v)
    if (x == s) return true;
  return false;
}

}  // namespace

TEST(a_side_key_turned_round_passes_through_no_side_key_unless_clean) {
  const std::vector<int16_t> turn = {320,  320,  320, -320, -320, -320,
                                     -320, -320, 320, 320,  320,  320};
  auto f = std::make_unique<fb::Fighter>();
  f->clean_keys = 0;  // no change clean
  const std::vector<int16_t> got = sent(*f, turn);
  // Each turn round: 1 to 3 commands of nothing, then the new key; the rest as wanted.
  for (const size_t at : {size_t{3}, size_t{8}}) {
    CHECK(got[at] == 0);
    size_t gap = 0;
    while (at + gap < got.size() && got[at + gap] == 0) gap++;
    CHECK(gap >= 1 && gap <= 3);
    CHECK(got[at + gap] == turn[at + gap]);
  }
  CHECK(got[0] == 320 && got[2] == 320);

  f = std::make_unique<fb::Fighter>();
  f->clean_keys = 1;  // every change clean: what was wanted
  CHECK(sent(*f, turn) == turn);

  // A key let go first is no turn: nothing held off.
  f = std::make_unique<fb::Fighter>();
  f->clean_keys = 0;
  const std::vector<int16_t> let_go = {320, 0, -320, -320};
  CHECK(sent(*f, let_go) == let_go);

  // On a proven jump the keys are the proof's.
  f = std::make_unique<fb::Fighter>();
  f->clean_keys = 0;
  f->bot.pilot.pilot.mode = sh::PilotMode::air;
  CHECK(sent(*f, turn) == turn);
}

TEST(a_buttons_doors_are_found_through_the_relays_it_fires) {
  const char* text =
      "{ \"classname\" \"func_button\" \"target\" \"t65\" \"model\" \"*16\" }\n"
      "{ \"classname\" \"trigger_once\" \"targetname\" \"t65\" \"target\" \"t73\" \"delay\" "
      "\"4.7\" }\n"
      "{ \"classname\" \"trigger_relay\" \"targetname\" \"t73\" \"target\" \"t80\" }\n"
      "{ \"classname\" \"trigger_counter\" \"targetname\" \"t80\" \"target\" \"t90\" }\n"
      "{ \"classname\" \"func_door\" \"targetname\" \"t73\" \"model\" \"*13\" }\n";
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  CHECK(pg::parse_entity_text(text, blocks, error));
  const std::vector<std::string> fired = fb::button_fires(blocks, "t65");
  CHECK(has(fired, "t65") && has(fired, "t73") && has(fired, "t80"));
  CHECK(!has(fired, "t90"));  // a counter needs more than one press: not followed
  CHECK(fb::button_fires(blocks, "").empty());
}
