// Tests: the server's own chat (games/shooters_edge/server/chat_rules.hpp): a player's say reaches
// everyone and his say_team his team; a spectator's say_team reaches only the spectators with the
// [SPEC] prefix, and his say does too when sv_spectalk is 0 during a match; a team's reports
// reach the spectators following it by sv_sayteam_to_spec and the phase.
//
// ge:summary Tests: who hears a say or say_team the mod left to the server, sv_spectalk
// ge:keywords test chat say say_team spectator-chat sv_spectalk sv_sayteam_to_spec
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

TEST(sv_spectalk_mutes_spectators_only_during_a_match) {
  auto s = table();
  CHECK(s->cvars.set("sv_spectalk", "0", ge::console::Source::code) ==
        ge::console::SetResult::changed);
  s->localinfo.emplace_back("ge_phase", "warmup");
  CHECK(sv::chat_route(*s, 3, false, "gl").to == 0xFu);  // before the match: everyone
  s->localinfo.back().second = "countdown";
  CHECK(sv::chat_route(*s, 3, false, "gl").to == 0x8u);
  s->localinfo.back().second = "intermission";
  CHECK(sv::chat_route(*s, 3, false, "gg").to == 0xFu);
}

TEST(sv_sayteam_to_spec_shows_a_team_s_reports_to_its_followers) {
  auto s = table();
  const int tracks[32] = {-1, -1, -1, 0};  // the spectator follows ann (red)
  // Typed by hand (no carriage return): the team alone.
  CHECK(sv::chat_route(*s, 0, true, "go", tracks).to == 0x3u);
  // A report (its carriage return): the spectator following red reads it, in a match (1).
  const std::string report = "\r{&cf00quad&r} at mega";
  CHECK(sv::chat_route(*s, 0, true, report, tracks).to == 0xBu);
  CHECK(sv::chat_route(*s, 2, true, report, tracks).to == 0x4u);  // blue's: not followed
  s->localinfo.emplace_back("ge_phase", "warmup");
  CHECK(sv::chat_route(*s, 0, true, report, tracks).to == 0x3u);  // 1: in a match only
  CHECK(s->cvars.set("sv_sayteam_to_spec", "2", ge::console::Source::code) ==
        ge::console::SetResult::changed);
  CHECK(sv::chat_route(*s, 0, true, report, tracks).to == 0xBu);  // 2: outside one only
  CHECK(s->cvars.set("sv_sayteam_to_spec", "0", ge::console::Source::code) ==
        ge::console::SetResult::changed);
  CHECK(sv::chat_route(*s, 0, true, report, tracks).to == 0x3u);  // 0: never
  // Following nobody, the spectator's own team key decides.
  CHECK(s->cvars.set("sv_sayteam_to_spec", "3", ge::console::Source::code) ==
        ge::console::SetResult::changed);
  s->clients[3].userinfo = {{"name", "di"}, {"team", "blue"}};
  CHECK(sv::chat_route(*s, 2, true, report).to == 0xCu);
  CHECK(sv::chat_route(*s, 0, true, report).to == 0x3u);
}
