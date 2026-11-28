// The bots' entity text (shooterbots/entities.hpp): blocks in order, values by key, refusals.
//
// ge:summary Tests the bots' entity text: blocks, values by key, names fired, a block refused
// ge:keywords entities parse_entity_text block_value name_fired pathtarget test
// ge:skill shooterbots
#include <string>
#include <vector>

#include "check.hpp"
#include "shooterbots/entities.hpp"

namespace sb = ge::shooterbots;

TEST(entity_text_reads_blocks_and_values_in_order) {
  std::vector<sb::EntityBlock> blocks;
  std::string error;
  CHECK_MSG(sb::parse_entity_text(R"({ "classname" "worldspawn" }
// a comment
{ "classname" "func_door" "targetname" "t1" "classname" "second" })",
                                  blocks, error),
            error);
  CHECK(blocks.size() == 2);
  CHECK(std::string(sb::block_value(blocks[1], "targetname")) == "t1");
  CHECK(std::string(sb::block_value(blocks[1], "classname")) == "func_door");  // the first pair
  CHECK(sb::block_value(blocks[0], "target") == nullptr);
}

TEST(entity_text_refuses_an_unclosed_block_and_reads_none) {
  std::vector<sb::EntityBlock> blocks;
  std::string error;
  CHECK(!sb::parse_entity_text(R"({ "classname" "a" } { "classname")", blocks, error));
  CHECK(blocks.empty());
  CHECK(!error.empty());
  CHECK(sb::parse_entity_text(nullptr, blocks, error));
  CHECK(blocks.empty());
}

// space's dish, cut down: the train's path_corner fires the trigger in the pit as it arrives, so
// that trigger is fired (no step to touch); one nothing names is not.
TEST(a_name_a_trains_path_fires_is_fired_and_one_nothing_names_is_not) {
  std::vector<sb::EntityBlock> blocks;
  std::string error;
  CHECK_MSG(sb::parse_entity_text(R"(
{ "classname" "func_train" "targetname" "dish1" "target" "fall1" }
{ "classname" "path_corner" "targetname" "fall1" "target" "fall2" }
{ "classname" "path_corner" "targetname" "fall2" "pathtarget" "explo1" }
{ "classname" "trigger_once" "targetname" "explo1" "target" "boom" }
{ "classname" "trigger_once" "targetname" "lonely" }
{ "classname" "func_door" "targetname" "gone" }
{ "classname" "trigger_relay" "killtarget" "gone" }
)",
                                  blocks, error),
            error);
  CHECK(sb::name_fired(blocks, "explo1"));
  CHECK(sb::name_fired(blocks, "fall2"));
  CHECK(sb::name_fired(blocks, "gone"));
  CHECK(!sb::name_fired(blocks, "lonely"));
  CHECK(!sb::name_fired(blocks, "dish1"));
  CHECK(!sb::name_fired(blocks, nullptr));
}
