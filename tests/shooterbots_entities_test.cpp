// The bots' entity text (shooterbots/entities.hpp): blocks in order, values by key, refusals.
//
// ge:summary Tests the bots' entity text: blocks, values by key, an unclosed block refused
// ge:keywords entities parse_entity_text block_value test
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
