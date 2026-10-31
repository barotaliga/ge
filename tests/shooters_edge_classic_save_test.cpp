// Tests: the classic text save (games/shooters_edge/server/classic_save.hpp): a game written,
// read back and written again gives the same text; the header's items and the player come back;
// a save from an engine that numbers the map's entities right after one player is moved onto this
// server's numbering with its entity values; a wrong version or a short header is refused with
// the line; and the value text reads back to the same values (progs/entity_text.hpp).
//
// ge:summary Tests: the classic text save written, read back and written the same; renumbering
// ge:keywords test classic-save sav text-save round-trip renumber version value_text
// ge:skill shooter save testing
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/entity_text.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/classic_save.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace {

namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;

bool load(void* /*ctx*/, const std::string& /*name*/, sv::MapSpec& out, std::string& /*error*/) {
  out.models = sv::arena_models();
  out.entities = sv::kStubEntities;
  return true;
}

sv::CampaignOptions options() {
  sv::CampaignOptions o;
  o.server.loop.max_edicts = 64;
  o.load = &load;
  o.settings = {"deathmatch 0"};
  return o;
}

struct Game {
  pg::Progs mod;
  sv::Campaign c;
  std::string error;
  Game() {
    CHECK_MSG(sv::build_campaign_stub(mod, error), error);
    CHECK_MSG(sv::campaign_new_game(c, mod, options(), "first", error), error);
    sv::campaign_join(c, 0, "player");
  }
  float f(int e, int ofs) { return pg::as_float(*pg::entity_field(c.server->q.vm, e, ofs)); }
};

}  // namespace

TEST(a_game_written_read_and_written_again_is_the_same_text) {
  Game a;
  for (int k = 0; k < 10; k++) CHECK_MSG(sv::campaign_frame(a.c, nullptr, 0, a.error), a.error);
  std::string text, again, error;
  CHECK_MSG(sv::classic_save_text(a.c, text, error), error);
  CHECK(text.rfind("5\n", 0) == 0);

  Game b;
  CHECK_MSG(sv::classic_load_text(b.c, b.mod, options(), text, "s0", "player", error), error);
  CHECK_MSG(sv::classic_save_text(b.c, again, error), error);
  if (again != text) {
    size_t d = 0;
    while (d < again.size() && d < text.size() && again[d] == text[d]) d++;
    CHECK_MSG(false, "parts at " + std::to_string(d) + ": [" +
                         text.substr(d > 80 ? d - 80 : 0, 200) + "] vs [" +
                         again.substr(d > 80 ? d - 80 : 0, 200) + "]");
  }
  const pg::LayoutFields& f = a.c.server->q.layout->f;
  CHECK(b.f(1, f.health) == a.f(1, f.health));
  CHECK(b.c.server->sv.time == a.c.server->sv.time);
  CHECK(b.c.server->clients[0].spawn_parms[0] == a.c.server->clients[0].spawn_parms[0]);
  CHECK(b.c.players[0].active && b.c.server->sv.clients[0].active);
  CHECK(b.c.server->sv.edicts[1].origin.x == a.c.server->sv.edicts[1].origin.x);
  // It plays on.
  for (int k = 0; k < 30; k++) CHECK_MSG(sv::campaign_frame(b.c, nullptr, 0, error), error);
  CHECK(b.c.server->q.faults == 0);
}

TEST(a_save_numbered_after_one_player_is_moved_onto_this_servers_numbering) {
  Game a;
  std::string text, error;
  CHECK_MSG(sv::classic_save_text(a.c, text, error), error);
  // The same save as an engine with one player slot writes it: the world, the player, then a
  // thing at 2 that the player owns and that owns the player back.
  const size_t globals = text.find("{\n");
  const size_t world_end = text.find("}\n", text.find("{\n", globals + 2)) + 2;
  const size_t player_end = text.find("}\n", world_end) + 2;
  std::string compact = text.substr(0, player_end);
  compact.insert(player_end - 2, "\"owner\" \"2\"\n");
  compact += "{\n\"classname\" \"thing\"\n\"owner\" \"1\"\n\"health\" \"7\"\n}\n";
  Game b;
  CHECK_MSG(sv::classic_load_text(b.c, b.mod, options(), compact, "s1", "player", error), error);
  const sv::QcServer& s = *b.c.server;
  const int first = s.sv.set.first_dynamic;
  CHECK(first > 2);
  CHECK(s.sv.num_edicts == first + 1);
  CHECK(!s.sv.edicts[static_cast<size_t>(first)].free);
  CHECK(s.sv.edicts[2].free);
  const pg::LayoutFields& f = s.q.layout->f;
  CHECK(pg::as_float(*pg::entity_field(s.q.vm, first, f.health)) == 7);
  const auto owner = [&](int e) {
    return pg::entity_number(s.q.vm, static_cast<int32_t>(*pg::entity_field(s.q.vm, e, f.owner)));
  };
  CHECK(owner(1) == first);
  CHECK(owner(first) == 1);
}

TEST(a_wrong_version_or_a_short_header_is_refused_with_its_line) {
  Game a;
  std::string text, error;
  CHECK_MSG(sv::classic_save_text(a.c, text, error), error);
  Game b;
  std::string bad = text;
  bad[0] = '4';
  CHECK(!sv::classic_load_text(b.c, b.mod, options(), bad, "old", "player", error));
  CHECK_MSG(error.find("line 1") != std::string::npos, error);
  CHECK(!b.c.server);
  CHECK(!sv::classic_load_text(b.c, b.mod, options(), text.substr(0, 40), "cut", "player", error));
  CHECK_MSG(error.find("header") != std::string::npos, error);
  bad = text;
  bad.replace(bad.find('\n') + 1, 0, "x\n");  // every item after the comment a line late
  CHECK(!sv::classic_load_text(b.c, b.mod, options(), bad, "late", "player", error));
}

TEST(the_value_text_reads_back_to_the_same_values) {
  pg::Progs mod;
  std::string error;
  CHECK_MSG(sv::build_campaign_stub(mod, error), error);
  pg::Vm vm;
  pg::create_vm(vm, mod);
  vm.num_entities = 4;
  const pg::LayoutFields& f = pg::kSinglePlayerLayout.f;
  const float odd = 0.1f + 1e-7f;
  *pg::entity_field(vm, 2, f.health) = pg::as_word(odd);
  uint32_t* org = pg::entity_field(vm, 2, f.origin);
  org[0] = pg::as_word(-3.25f), org[1] = pg::as_word(1e-6f), org[2] = pg::as_word(123456.7f);
  *pg::entity_field(vm, 2, f.netname) = static_cast<uint32_t>(pg::engine_string(vm, "a\nb"));
  pg::EntityBlock b;
  for (const char* name : {"health", "origin", "netname"}) {
    const pg::Def* d = pg::find_field(mod, name);
    b.pairs.emplace_back(name, pg::value_text(vm, d->type, pg::entity_field(vm, 2, d->offset)));
  }
  pg::clear_entity(vm, 3);
  pg::EntityExtras x;
  std::vector<std::string> messages;
  CHECK_MSG(pg::apply_entity_block(vm, 3, b, x, messages, error), error);
  CHECK(*pg::entity_field(vm, 3, f.health) == pg::as_word(odd));
  CHECK(std::memcmp(pg::entity_field(vm, 3, f.origin), org, 12) == 0);
  const char* name = pg::vm_string(vm, static_cast<int32_t>(*pg::entity_field(vm, 3, f.netname)));
  CHECK(std::strcmp(name, "a\nb") == 0);
  const uint32_t minus_zero = 0x80000000u;
  CHECK(pg::value_is_zero(pg::kTypeFloat, &minus_zero));
}
