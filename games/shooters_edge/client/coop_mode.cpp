// Co-op as a mode of the window (coop_mode.hpp): the archived cvars, the lines a coop game starts
// with, and the frame that starts it and keeps friendly fire and the episodes in step.
//
// ge:summary Co-op as a window mode: sv_coop starts the game, friendly fire and episodes kept
// ge:keywords coop co-op sv_coop sv_coopbots sv_friendlyfire sv_friendlypush sv_episodes_done
// ge:skill shooter bots
// ge:test tests/shooters_edge_coop_mode_test.cpp
#include "shooters_edge/client/coop_mode.hpp"

#include <cstdint>
#include <string>

#include "console/cvar.hpp"

namespace shooters_edge::client {

namespace {

namespace con = ge::console;

// The server's cvar `name`, null when there is no server or no such cvar.
con::Cvar* server_cvar(Listen& l, const char* name) {
  return l.server != nullptr ? l.server->cvars.find(name) : nullptr;
}

std::string bits_text(uint32_t bits) { return std::to_string(bits); }

}  // namespace

void add_coop_cvars(con::Console& c, CoopMode& m) {
  con::Cvars& v = c.cvars();
  m.coop = &v.add({.name = "sv_coop",
                   .fallback = "0",
                   .flags = con::kArchive,
                   .help = "1: the campaign is a coop game, started on the start map (at the "
                           "game's start from a cfg, at once from the console)",
                   .kind = con::CvarKind::boolean});
  m.bots = &v.add({.name = "sv_coopbots",
                   .fallback = "1",
                   .flags = con::kArchive,
                   .help = "companion bots beside you when sv_coop starts a game (0 to 7)",
                   .kind = con::CvarKind::integer,
                   .min = 0,
                   .max = kPartyMost});
  m.friendly_fire = &v.add({.name = "sv_friendlyfire",
                            .fallback = "0",
                            .flags = con::kArchive,
                            .help = "coop: 1 lets one player's shots harm another (0: teammates "
                                    "are spared)",
                            .kind = con::CvarKind::boolean});
  m.friendly_push = &v.add({.name = "sv_friendlypush",
                            .fallback = "0",
                            .flags = con::kArchive,
                            .help = "coop with sv_friendlyfire 0: 1 lets a spared teammate's blow "
                                    "still push you (a rocket boost off a friend); 0 no push",
                            .kind = con::CvarKind::boolean});
  m.episodes = &v.add({.name = "sv_episodes_done",
                       .fallback = "0",
                       .flags = con::kArchive,
                       .help = "the episodes you have finished, a bit each (1, 2, 4, 8), kept "
                               "from game to game; the start map opens each next one once the one "
                               "before is done and shuts a done one (0 opens episode 1 again)",
                       .kind = con::CvarKind::integer,
                       .min = 0,
                       .max = 15});
  m.coop_seen = m.coop->revision;
  m.window = &v;
}

void coop_game_lines(const CoopMode& m, ListenOptions& opt) {
  opt.console.emplace_back("deathmatch 0");
  opt.console.emplace_back("coop 1");
  opt.console.emplace_back("sv_friendlyfire " + m.friendly_fire->value);
  opt.console.emplace_back("sv_friendlypush " + m.friendly_push->value);
  opt.console.emplace_back("sv_episodes_done " + m.episodes->value);
}

void coop_frame(CoopMode& m, Listen& l, bool has_campaign, const std::string& start_map,
                std::string& next_map) {
  if (m.coop->revision != m.coop_seen) {
    m.coop_seen = m.coop->revision;
    if (m.coop->on() && has_campaign) {
      coop_game_lines(m, l.opt);
      l.opt.companions = m.bots->integer();
      l.opt.new_game = true;
      next_map = start_map;
    }
  }
  const con::Cvar* coop = server_cvar(l, "coop");
  // The window's friendly-fire settings are a coop game's.
  if (coop != nullptr && coop->on())
    for (con::Cvar* mine : {m.friendly_fire, m.friendly_push})
      if (con::Cvar* there = server_cvar(l, mine->name.c_str());
          there != nullptr && there->value != mine->value)
        l.server->cvars.set(*there, mine->value, con::Source::local);
  // The episodes done, in any campaign (single player or coop): either side's bits are both's.
  if (con::Cvar* there = l.campaign_host ? server_cvar(l, "sv_episodes_done") : nullptr) {
    const uint32_t mine = static_cast<uint32_t>(m.episodes->integer());
    const uint32_t theirs = static_cast<uint32_t>(there->integer());
    const uint32_t both = (mine | theirs) & 15u;
    if (both != mine) m.window->set(*m.episodes, bits_text(both), con::Source::code);
    if (both != theirs) l.server->cvars.set(*there, bits_text(both), con::Source::local);
  }
}

}  // namespace shooters_edge::client
