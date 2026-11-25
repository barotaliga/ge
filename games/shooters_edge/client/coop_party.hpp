// THE CO-OP PARTY ON THE LISTEN SERVER: companions (shooterbots/companion.hpp) playing a campaign
// in coop beside the player, in his own process. The listen (listen.hpp) owns one in a campaign:
//
// - `addbot [novice|average|expert] [escort|helper]` (or `coopgame <n>`'s bots) seats a companion
//   in the lowest free slot: it joins the campaign (server/campaign.hpp's `campaign_join`), so the
//   host carries it from map to map like any player, its inventory in its spawn parms; `removebot`
//   lets the highest one go;
// - every map entered (a level change, a restart, a new game) builds the companions' NAV off the
//   map (shooterbots/map_nav.hpp: its doors, plats, teleporters and water) and makes each companion
//   afresh on it, the player (slot 0) its leader;
// - an ESCORT (the default) keeps at the player's shoulder (shooterbots/companion.hpp); a HELPER
//   works the level near the people (shooterbots/party.hpp's `PartyWork`): the helpers split the
//   map's buttons, keys, secrets and monsters between them by the walk, leave whatever lies beyond
//   the leash round the people and the exit to them, and come back to the nearest person when
//   nothing near is left;
// - each frame a companion's packet goes into the campaign's frame after the player's, in slot
//   order, made from what its client is sent like the player's.
//
// Deterministic: the companions' hands are seeded off the listen's seed and their slot.
//
// ge:summary The co-op party: companions seated in a campaign, carried and rebuilt map to map
// ge:keywords coop co-op party companion addbot removebot coopgame listen-server campaign
// ge:keywords party_add party_remove party_enter party_packets nav-graph helper escort
// ge:skill shooter bots
// ge:test tests/shooters_edge_campaign_view_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "shooterbots/aim.hpp"
#include "shooterbots/fighter.hpp"
#include "shooterbots/map_nav.hpp"
#include "shooterbots/nav.hpp"
#include "shooterbots/party.hpp"
#include "shooters_edge/bots/arsenals.hpp"
#include "shooters_edge/server/campaign.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;
namespace sv = shooters_edge::server;

inline constexpr int kPartyMost = 7;  // companions at most (a coop game's eight players)

struct PartyMate {
  bots::Fighter fighter;
  std::unique_ptr<sb::PartyWork> work;  // a helper's (null: an escort)
  sb::AimSkill skill = sb::AimSkill::average;
  std::string name;
  sh::MoveCmd cmds[3];  // the three newest, oldest first
  uint32_t sequence = 0;
};

struct Party {
  std::array<std::unique_ptr<PartyMate>, sh::kMaxClients> mates;  // by slot (null: none)
  std::unique_ptr<bots::MapNav> nav;  // this map's graph, solids and swim (shooterbots/map_nav.hpp)
  std::string graph_of;  // the map it was built for, and its place in the campaign's history
  size_t graph_at = 0;
  uint32_t seed = 1;
  int named = 0;                    // companions ever seated (their names count up)
  std::vector<sh::Packet> packets;  // a frame's, the player's first (reused)
  sb::PartyRound round;  // the helpers' people and split this frame (shooterbots/party.hpp)
};

// How many companions are seated.
int party_size(const Party& p);
// A companion of `skill` seated in the campaign `c` (playing `map`) now: its slot, or -1 with
// `error` (the party full, or a map with no floor to walk).
int party_add(Party& p, sv::Campaign& c, const sv::MapSpec& map, sb::AimSkill skill,
              std::string& error, bool helper = false);
// The highest companion let go. False when none is seated.
bool party_remove(Party& p, sv::Campaign& c);
// A map entered (`c`'s history moved on): the graph rebuilt and every companion made afresh on it.
// A map no companion can walk sends them away, said in `error`.
void party_enter(Party& p, sv::Campaign& c, const sv::MapSpec& map, std::string& error);
// The companions' packets for this frame, appended to `out`.
void party_packets(Party& p, const sv::RulesCommon& s, int msec, std::vector<sh::Packet>& out);

}  // namespace shooters_edge::client
