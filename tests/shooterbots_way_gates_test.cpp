// The way's gates and the buttons that end a map (plugins/shooterbots/way_gates.hpp,
// buttons.hpp): a trigger that waits to be made live wakes when the monster whose death fires its
// name dies (biggun's boss arms the strip by the only door north; the bot, never told, stood south
// of it until the base blew up), and a button whose relays reach a killbox or a blast no one lives
// through is one that dooms (biggun's evacuate button, pressed last and then run from).
//
// ge:summary Tests for gates woken by a monster's death and the buttons that blow a map up
// ge:keywords test way_gates way_gate_names way_wake_gates gate_dead deathtarget
// ge:keywords button_dooms killbox biggun button_spins func_rotating boss2
// ge:keywords door_is_lever lever mine4
// ge:skill shooterbots testing
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooter/rules_world.hpp"
#include "shooter/server.hpp"
#include "shooterbots/buttons.hpp"
#include "shooterbots/entities.hpp"
#include "shooterbots/way_gates.hpp"

namespace sh = ge::shooter;
namespace sb = ge::shooterbots;

namespace {

// The least a BotView reads: the server's entities and the names.
struct Rules {
  sh::Server sv;
  sh::RulesWorld world;
  std::vector<std::string> models{""}, sounds{""};
  std::string map_name = "test";
};

constexpr int kBoss = 5;  // the boss's edict

// One client, the boss standing at its spot.
std::unique_ptr<Rules> with_boss() {
  auto r = std::make_unique<Rules>();
  r->sv.set.maxclients = 1;
  r->sv.edicts.assign(8, sh::Edict{});
  r->sv.num_edicts = 8;
  for (sh::Edict& e : r->sv.edicts) e.free = true;
  sh::Edict& b = r->sv.edicts[kBoss];
  b.free = false;
  b.solid = sh::kSolidSlidebox;
  b.health = 3000;
  b.origin = {1352, 96, -350};  // a little off its spot: risen at the start
  return r;
}

std::vector<sb::EntityBlock> blocks_of(const char* text) {
  std::vector<sb::EntityBlock> blocks;
  std::string error;
  CHECK_MSG(sb::parse_entity_text(text, blocks, error), error);
  return blocks;
}

// biggun's way north, cut down: the strip that opens the door waits for t93, which only the
// boss's death fires (the trigger_always that would is deathmatch's alone).
constexpr const char* kNorth = R"({
"classname" "worldspawn"
}
{
"classname" "monster_boss2"
"origin" "1352 96 -360"
"deathtarget" "t93"
}
{
"classname" "trigger_multiple"
"targetname" "t93"
"target" "t92"
"spawnflags" "4"
}
{
"classname" "func_door"
"targetname" "t92"
}
)";

}  // namespace

TEST(a_trigger_armed_by_a_monsters_death_waits_while_it_lives) {
  auto r = with_boss();
  const sb::BotView s(*r);
  const std::vector<sb::EntityBlock> blocks = blocks_of(kNorth);
  sb::WayThrough w;
  sb::WayStep st;
  st.kind = sb::WayKind::trigger;
  st.target = "t92";
  sb::way_gate_names(st, "t93", blocks, s);
  CHECK(st.gate_dead.size() == 1 && st.gate_dead[0] == kBoss);
  w.steps.push_back(st);
  sb::way_wake_gates(w, s);
  CHECK(!w.steps[0].gate.empty());
  CHECK(w.steps[0].left_until >= sb::kWayForever);
}

TEST(a_trigger_armed_by_a_monsters_death_wakes_once_it_dies) {
  auto r = with_boss();
  const sb::BotView s(*r);
  const std::vector<sb::EntityBlock> blocks = blocks_of(kNorth);
  sb::WayThrough w;
  sb::WayStep st;
  st.kind = sb::WayKind::trigger;
  st.target = "t92";
  sb::way_gate_names(st, "t93", blocks, s);
  w.steps.push_back(st);
  r->sv.edicts[kBoss].health = -40;  // killed
  sb::way_wake_gates(w, s);
  CHECK(w.steps[0].gate.empty());
  CHECK(w.steps[0].left_until < 0);
}

TEST(a_button_whose_relays_reach_a_killbox_dooms_and_one_that_opens_a_door_does_not) {
  const std::vector<sb::EntityBlock> blocks = blocks_of(R"({
"classname" "worldspawn"
}
{
"classname" "trigger_relay"
"targetname" "t41"
"target" "t107"
"delay" "12"
}
{
"classname" "trigger_relay"
"targetname" "t107"
"target" "t117"
"delay" "10"
}
{
"classname" "func_killbox"
"targetname" "t117"
}
{
"classname" "func_door"
"targetname" "t5"
}
{
"classname" "target_explosion"
"targetname" "t6"
"dmg" "10000"
}
{
"classname" "target_explosion"
"targetname" "t7"
"dmg" "120"
}
)");
  CHECK(sb::button_dooms(blocks, "t41"));
  CHECK(sb::button_dooms(blocks, "t6"));  // a blast no one lives through
  CHECK(!sb::button_dooms(blocks, "t5"));
  CHECK(!sb::button_dooms(blocks, "t7"));  // a blast a player lives through
  CHECK(!sb::button_dooms(blocks, nullptr));
}

TEST(a_button_that_sets_a_blade_spinning_spins_through_its_relays_and_a_door_button_does_not) {
  // boss2's id Gallery: a button that starts a blade across a doorway and opens nothing.
  const std::vector<sb::EntityBlock> blocks = blocks_of(R"({
"classname" "worldspawn"
}
{
"classname" "trigger_relay"
"targetname" "t20"
"target" "t21"
}
{
"classname" "func_rotating"
"targetname" "t21"
}
{
"classname" "func_door"
"targetname" "t22"
}
)");
  CHECK(sb::button_spins(blocks, "t20"));  // through a relay
  CHECK(sb::button_spins(blocks, "t21"));
  CHECK(!sb::button_spins(blocks, "t22"));
  CHECK(!sb::button_spins(blocks, nullptr));
}

// mine4's lever beside the carts' track: a turning door with no name whose target starts the
// carts is pressed like a button; a door that only names its area portal, one with a name (opened
// by something else) and a shot one are not levers.
TEST(a_turning_door_touched_open_that_starts_the_carts_is_a_lever) {
  const std::vector<sb::EntityBlock> blocks = blocks_of(R"({
"classname" "worldspawn"
}
{
"classname" "func_door_rotating"
"model" "*22"
"target" "t678"
}
{
"classname" "func_train"
"targetname" "t678"
}
{
"classname" "func_door_rotating"
"model" "*23"
"target" "p1"
}
{
"classname" "func_areaportal"
"targetname" "p1"
}
{
"classname" "func_door_rotating"
"model" "*24"
"targetname" "named"
"target" "t678"
}
{
"classname" "func_door_rotating"
"model" "*25"
"health" "10"
"target" "t678"
}
)");
  CHECK_MSG(sb::door_is_lever(blocks, blocks[1]),
            "mine4's lever is not taken for one: read door_is_lever in buttons.cpp");
  CHECK(!sb::door_is_lever(blocks, blocks[2]));  // the train itself
  CHECK(!sb::door_is_lever(blocks, blocks[3]));  // only its portal
  CHECK(!sb::door_is_lever(blocks, blocks[5]));  // named: something else opens it
  CHECK(!sb::door_is_lever(blocks, blocks[6]));  // shot open
}
