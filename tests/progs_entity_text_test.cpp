// Tests: the map's entity text (plugins/progs/entity_text.hpp): blocks, quotes and comments, the
// parse errors that name their block and line, and a block's keys into fields: `_` keys ignored,
// angle and light renamed, alpha and colormod to the engine's own values, every field type read
// (the string's escapes), a key that is no field skipped with a message, and an unknown function
// or field name stopping the map.
//
// ge:summary Tests: the entity text's parsing and its keys into an entity's fields
// ge:keywords test qc entity-text parse_entity_text apply_entity_block angle light alpha colormod
// ge:skill progs testing
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/assembler.hpp"
#include "progs/entity_text.hpp"

namespace pg = ge::progs;

namespace {

struct Mod {
  pg::Progs progs;
  pg::Vm vm;
  int speed = 0, mangle = 0, target = 0, fld = 0, use = 0, light_lev = 0;
  int32_t fn = 0;

  Mod() {
    pg::Assembler a;
    a.field("speed", pg::kTypeFloat);
    a.field("mangle", pg::kTypeVector);
    a.field("target_ent", pg::kTypeEntity);
    a.field("fld", pg::kTypeField);
    a.field("light_lev", pg::kTypeFloat);
    fn = a.begin_function("door_use", {}, 0);
    a.emit(pg::kOpDone);
    a.end_function();
    std::string error;
    CHECK_MSG(a.build(progs, error), error);
    pg::create_vm(vm, progs);
    vm.num_entities = 3;
    speed = pg::find_field(progs, "speed")->offset;
    mangle = pg::find_field(progs, "mangle")->offset;
    target = pg::find_field(progs, "target_ent")->offset;
    fld = pg::find_field(progs, "fld")->offset;
    light_lev = pg::find_field(progs, "light_lev")->offset;
    use = pg::kFUse;
  }
  float f(int e, int ofs, int k = 0) { return pg::as_float(pg::entity_field(vm, e, ofs)[k]); }
  std::string s(int e, int ofs) {
    return pg::vm_string(vm, static_cast<int32_t>(*pg::entity_field(vm, e, ofs)));
  }
};

}  // namespace

TEST(the_text_splits_into_blocks_of_pairs) {
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  const char* text =
      "// a comment\n{\n\"classname\" \"worldspawn\"\n}\n{ classname light \"origin\" \"1 2 3\" }";
  CHECK_MSG(pg::parse_entity_text(text, blocks, error), error);
  CHECK(blocks.size() == 2);
  CHECK(blocks[0].pairs.size() == 1 && blocks[0].line == 2);
  CHECK(blocks[1].pairs.size() == 2);
  CHECK(std::string(pg::block_value(blocks[1], "classname")) == "light");
  CHECK(std::string(pg::block_value(blocks[1], "origin")) == "1 2 3");
  CHECK(pg::block_value(blocks[1], "angle") == nullptr);
  CHECK(pg::parse_entity_text("", blocks, error) && blocks.empty());
}

TEST(a_broken_text_names_its_block_and_line) {
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  CHECK(!pg::parse_entity_text("{\n\"classname\" \"x\"\n", blocks, error));
  CHECK(error.find("line") != std::string::npos);
  CHECK(!pg::parse_entity_text("{ \"key\" }", blocks, error));
  CHECK(!pg::parse_entity_text("{ \"key\" { }", blocks, error));
  CHECK(!pg::parse_entity_text("\"stray\" { }", blocks, error));
}

TEST(a_blocks_keys_go_into_the_fields) {
  Mod m;
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  CHECK(pg::parse_entity_text(R"({
"classname" "func_door"
"_tb_group" "3"
"angle" "90"
"speed" "  12.5"
"mangle" "1 2"
"target_ent" "2"
"fld" "speed"
"use" "door_use"
"light" "200"
"message" "line one\nline two \q"
"alpha" "1.5"
"colormod" "0.5 1 2"
"sounds" "4"
"nosuchkey" "1"
})",
                              blocks, error));
  pg::EntityExtras extras;
  std::vector<std::string> messages;
  CHECK_MSG(pg::apply_entity_block(m.vm, 1, blocks[0], extras, messages, error), error);
  CHECK(m.s(1, pg::kFClassname) == "func_door");
  CHECK(m.f(1, pg::kFAngles, 0) == 0.0f && m.f(1, pg::kFAngles, 1) == 90.0f &&
        m.f(1, pg::kFAngles, 2) == 0.0f);
  CHECK(m.f(1, m.speed) == 12.5f);
  CHECK(m.f(1, m.mangle, 0) == 1.0f && m.f(1, m.mangle, 1) == 2.0f && m.f(1, m.mangle, 2) == 0.0f);
  CHECK(*pg::entity_field(m.vm, 1, m.target) == static_cast<uint32_t>(pg::entity_value(m.vm, 2)));
  CHECK(*pg::entity_field(m.vm, 1, m.fld) == static_cast<uint32_t>(m.speed));
  CHECK(*pg::entity_field(m.vm, 1, m.use) == static_cast<uint32_t>(m.fn));
  CHECK(m.f(1, m.light_lev) == 200.0f);
  // A backslash and n is a newline; a backslash and anything else is a backslash.
  CHECK(m.s(1, pg::find_field(m.progs, "message")->offset) == "line one\nline two \\");
  CHECK(extras.has_alpha && extras.alpha == 1.0f);
  CHECK(extras.has_colormod && extras.colormod[0] == 0.5f && extras.colormod[2] == 2.0f);
  bool skipped = false;
  for (const std::string& msg : messages)
    if (msg == "nosuchkey is not a field") skipped = true;
  CHECK(skipped);
}

TEST(a_colormod_with_a_zero_is_ignored) {
  Mod m;
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  CHECK(pg::parse_entity_text("{ \"colormod\" \"1 0 1\" \"alpha\" \"-2\" }", blocks, error));
  pg::EntityExtras extras;
  std::vector<std::string> messages;
  CHECK(pg::apply_entity_block(m.vm, 1, blocks[0], extras, messages, error));
  CHECK(!extras.has_colormod);
  CHECK(extras.has_alpha && extras.alpha == 0.0f);
}

TEST(an_unknown_function_or_field_name_stops_the_map) {
  Mod m;
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  pg::EntityExtras extras;
  std::vector<std::string> messages;
  CHECK(pg::parse_entity_text("{ \"use\" \"no_such_fn\" }", blocks, error));
  CHECK(!pg::apply_entity_block(m.vm, 1, blocks[0], extras, messages, error));
  CHECK(error.find("no_such_fn") != std::string::npos);
  CHECK(pg::parse_entity_text("{ \"fld\" \"no_such_field\" }", blocks, error));
  CHECK(!pg::apply_entity_block(m.vm, 1, blocks[0], extras, messages, error));
  CHECK(error.find("no_such_field") != std::string::npos);
}
