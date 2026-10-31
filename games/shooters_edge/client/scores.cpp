// What the scores key shows, read off the game playing (scores.hpp).
//
// ge:summary Reads the scoreboard's rows and the level's statistics off the listen server
// ge:keywords scores_of level_stats_of scores_screen userinfo frags-field entered_at
// ge:skill ui shooter
// ge:test tests/shooters_edge_scores_test.cpp
#include "shooters_edge/client/scores.hpp"

#include <cmath>
#include <cstdlib>
#include <string>

namespace shooters_edge::client {

namespace {

int number_of(const std::string& s) { return s.empty() ? 0 : std::atoi(s.c_str()); }

void clock_of(double seconds, int& minutes, int& secs) {
  const auto whole = static_cast<int64_t>(std::floor(std::max(seconds, 0.0)));
  minutes = static_cast<int>(whole / 60);
  secs = static_cast<int>(whole % 60);
}

}  // namespace

menu::Scoreboard scores_of(const Listen& l) {
  menu::Scoreboard b;
  if (l.server == nullptr) return b;
  const sv::QcServer& s = *l.server;
  const ge::console::Cvar* tp = s.cvars.find("teamplay");
  b.teamplay = tp != nullptr && tp->on();
  clock_of(s.sv.time - l.entered_at, b.minutes, b.seconds);
  for (int slot = 0; slot < sh::kMaxClients; slot++) {
    const sv::ClientInfo& c = s.clients[slot];
    if (!c.active) continue;
    menu::ScoreRow r;
    r.name = sv::info_value(c.userinfo, "name");
    if (r.name.empty()) continue;
    r.team = sv::info_value(c.userinfo, "team");
    r.top = number_of(sv::info_value(c.userinfo, "topcolor"));
    r.bottom = number_of(sv::info_value(c.userinfo, "bottomcolor"));
    r.spectator = c.spectator || !sv::info_value(c.userinfo, "*spectator").empty();
    r.frags = static_cast<int>(field_named(l, slot + 1, "frags"));
    r.ping = c.ping_ms;
    r.minutes = static_cast<int>(std::max(0.0, s.sv.time - c.entered_at) / 60);
    r.self = slot == 0;
    b.rows.push_back(r);
  }
  return b;
}

menu::LevelStats level_stats_of(const Listen& l) {
  menu::LevelStats st;
  const CampaignView& c = l.campaign;
  if (c.end == LevelEnd::none && l.server != nullptr) {
    CampaignView now = c;
    now.ended_at = l.server->sv.time;
    completed_time(now, l.entered_at, st.minutes, st.seconds);
  } else {
    completed_time(c, l.entered_at, st.minutes, st.seconds);
  }
  st.secrets = static_cast<int>(c.secrets);
  st.total_secrets = static_cast<int>(c.total_secrets);
  st.kills = static_cast<int>(c.monsters);
  st.total_kills = static_cast<int>(c.total_monsters);
  return st;
}

ScoresScreen scores_screen(const Listen& l, bool held) {
  if (l.server == nullptr) return ScoresScreen::none;
  if (l.campaign_host)
    return held && l.campaign.end == LevelEnd::none ? ScoresScreen::solo : ScoresScreen::none;
  if (held) return ScoresScreen::players;
  const PlayerState p = player_state(l);
  if (p.dead) return ScoresScreen::teams;
  return ScoresScreen::none;
}

}  // namespace shooters_edge::client
