// Tests: the server's own chat (games/shooters_edge/server/chat_rules.hpp): a player's say reaches
// everyone and his say_team his team; a spectator's say_team reaches only the spectators with the
// [SPEC] prefix, and his say does too when sv_spectalk is 0.
//
// ge:summary Tests: who hears a say or say_team the mod left to the server, sv_spectalk
// ge:keywords test chat say say_team spectator-chat sv_spectalk
// ge:skill shooter testing
#include <memory>
#include <string>

#include "check.hpp"
#include "shooters_edge/server/chat_rules.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace sv = shooters_edge::server;

namespace {

// Four seated: 0 and 1 on red, 2 on blue, 3 a spectator.
std::unique_ptr<sv::RulesCommon> table() {
  auto s = std::make_unique<sv::RulesCommon>();
  sv::add_chat_settings(s->cvars);
  const char* names[] = {"ann", "bo", "cy", "di"};
  const char* teams[] = {"red", "red", "blue", ""};
  for (int i = 0; i < 4; i++) {
    sv::ClientInfo& c = s->clients[i];
    c.active = true;
    c.spectator = i == 3;
    c.userinfo = {{"name", names[i]}, {"team", teams[i]}};
  }
  return s;
}

}  // namespace

TEST(a_player_s_chat_reaches_everyone_or_his_team) {
  auto s = table();
  const sv::ChatLine all = sv::chat_route(*s, 0, false, "hi");
  CHECK(all.to == 0xFu);
  CHECK(all.text == "ann: hi\n");
  const sv::ChatLine team = sv::chat_route(*s, 0, true, "go");
  CHECK(team.to == 0x3u);
  CHECK(team.text == "(ann): go\n");
}

TEST(a_spectator_s_chat_keeps_to_the_spectators_by_sv_spectalk) {
  auto s = table();
  sv::ChatLine said = sv::chat_route(*s, 3, false, "nice");
  CHECK(said.to == 0xFu);  // sv_spectalk 1: everyone
  CHECK(said.text == "di: nice\n");
  const sv::ChatLine team = sv::chat_route(*s, 3, true, "psst");
  CHECK(team.to == 0x8u);
  CHECK(team.text == "[SPEC] di: psst\n");
  CHECK(s->cvars.set("sv_spectalk", "0", ge::console::Source::code) ==
        ge::console::SetResult::changed);
  said = sv::chat_route(*s, 3, false, "nice");
  CHECK(said.to == 0x8u);
  CHECK(said.text == "[SPEC] di: nice\n");
}
