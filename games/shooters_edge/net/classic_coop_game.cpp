// The dedicated coop server (classic_coop_game.hpp).
//
// ge:summary The dedicated coop server: the campaign, its companions, the door, the status
// ge:keywords dedicated-coop coop-server sv_coop sv_coopbots campaign companions status
// ge:skill net shooter
// ge:test tests/shooters_edge_coop_server_test.cpp
#include "shooters_edge/net/classic_coop_game.hpp"

#include <cstdlib>
#include <vector>

#include "core/num.hpp"
#include "shooter/server.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/net/classic_coop.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

// A companion's move length: the campaign's frame, ms.
const int kMsec = static_cast<int>(ge::core::num::round(sv::kCampaignStep * 1000));

std::string rule(const gs::ServerConfig& c, const char* name, const char* fallback) {
  const auto it = c.rules.find(name);
  return it == c.rules.end() || it->second.empty() ? std::string(fallback) : it->second;
}

// The campaign's map loader: from the folder, else the supply.
bool load_map(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  CoopGame& g = *static_cast<CoopGame*>(ctx);
  std::unique_ptr<cl::LoadedMap> m;
  if (g.supply != nullptr) {
    m = g.supply(name);
    if (!m) error = "no map " + name + "; next: name a map the game holds";
  } else {
    m = cl::load_named_map(g.files, name, error);
  }
  if (!m) return false;
  g.map = std::move(m);
  out = g.map->spec;
  return true;
}

// The campaign moved maps: the map's sets, and the companions made afresh on it.
void entered(CoopGame& g) {
  if (g.entered == g.campaign.history.size() || !g.campaign.server) return;
  g.entered = g.campaign.history.size();
  if (g.map && g.map->from_file) sv::use_map_sets(*g.campaign.server, g.map->bsp);
  std::string error;
  cl::party_enter(g.party, g.campaign, g.campaign.map, error);
  if (!error.empty()) g.failed = error;
}

}  // namespace

CoopGame::~CoopGame() {
  if (door != nullptr) classic_coop().close(door);
  campaign.server = {};  // its views into the map go before the map does
}

bool coop_asked(const gs::ServerConfig& c) {
  return std::atoi(rule(c, "sv_coop", "0").c_str()) != 0;
}

std::unique_ptr<CoopGame> coop_open(const gs::ServerConfig& c, std::string& error,
                                    const ge::progs::Progs* program, CoopMapSupply supply) {
  auto g = std::make_unique<CoopGame>();
  g->hostname = c.hostname;
  g->seed = c.seed;
  g->supply = supply;
  g->program = program;
  const bool start_map = c.map.empty() || c.map == "arena";  // the default: the profile's start
  g->first_map = start_map ? std::string("start") : c.map;
  bool registered = false;
  if (program == nullptr) {
    if (!cl::open_game_dir(c.root, kGameFolder, g->files, error)) return nullptr;
    if (!cl::load_programs(cl::classic_adapter(), g->files, "", g->content, error)) return nullptr;
    if (g->content.campaign == nullptr) {
      error = "sv_coop: the game folder " + g->files.folder +
              " holds no single-player program; next: put the game's packs there (--root), or "
              "set sv_coop 0 for a match";
      return nullptr;
    }
    g->program = g->content.campaign;
    registered = g->content.registered;
    if (start_map) g->first_map = g->content.first_map;
  }
  g->bots = std::atoi(rule(c, "sv_coopbots", "0").c_str());
  if (g->bots < 0 || g->bots > cl::kPartyMost) {
    error = "sv_coopbots " + std::to_string(g->bots) + ": 0 to " + std::to_string(cl::kPartyMost) +
            " companions; next: set one of those";
    return nullptr;
  }
  sv::CampaignOptions co;
  co.server.seed = c.seed;
  co.server.loop.max_edicts = 1024;
  co.server.loop.maxclients = cl::kPartyMost + 1;  // eight players: what a browser reads
  co.load = load_map;
  co.load_ctx = g.get();
  co.registered = registered;
  co.settings = {"deathmatch 0",
                 "coop 1",
                 "skill " + rule(c, "skill", "1"),
                 "sv_friendlyfire " + rule(c, "sv_friendlyfire", "0"),
                 "sv_friendlypush " + rule(c, "sv_friendlypush", "0"),
                 "sv_episodes_done " + rule(c, "sv_episodes_done", "0")};
  if (!sv::campaign_new_game(g->campaign, *g->program, co, g->first_map, error)) return nullptr;
  g->entered = g->campaign.history.size();
  if (g->map && g->map->from_file) sv::use_map_sets(*g->campaign.server, g->map->bsp);
  g->party.seed = c.seed;
  for (int i = 0; i < g->bots; i++)
    if (cl::party_add(g->party, g->campaign, g->campaign.map, sh::AimSkill::average, error) < 0)
      return nullptr;
  return g;
}

bool coop_serve(CoopGame& g, ge::net::Transport& port, std::string& error) {
  const cl::CampaignDoor& d = classic_coop();
  g.door =
      d.open(g.campaign, port, g.hostname, g.seed, cl::kPartyMost + 1, nullptr, nullptr, error);
  if (g.door == nullptr) return false;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (const auto& m = g.party.mates[static_cast<size_t>(i)])
      if (!d.seat(g.door, i, m->name, true, error)) return false;
  return true;
}

void coop_frame(CoopGame& g) {
  if (!g.campaign.server) return;
  g.party.packets.clear();
  cl::party_packets(g.party, *g.campaign.server, kMsec, g.party.packets);
  std::string error;
  if (!sv::campaign_frame(g.campaign, g.party.packets.data(),
                          static_cast<int>(g.party.packets.size()), error)) {
    g.failed = error;
    return;
  }
  if (!g.campaign.server) return;
  sv::RulesCommon& now = *g.campaign.server;
  now.out.all.clear();
  for (auto& one : now.out.one) one.clear();
  sv::clear_frame(now.out);
  entered(g);
}

void coop_net_frame(CoopGame& g, int64_t now_us) {
  if (!g.campaign.server || g.door == nullptr) return;
  g.party.packets.clear();
  cl::party_packets(g.party, *g.campaign.server, kMsec, g.party.packets);
  std::string error;
  if (!classic_coop().frame(g.door, g.party.packets.data(),
                            static_cast<int>(g.party.packets.size()), now_us, error)) {
    g.failed = error;
    return;
  }
  entered(g);
}

int coop_seated(const CoopGame& g) {
  return cl::party_size(g.party) + (g.door != nullptr ? classic_coop().joined(g.door) : 0);
}

uint64_t coop_hash(const CoopGame& g) {
  return g.campaign.server ? sh::server_hash(g.campaign.server->sv) : 0;
}

std::string coop_status(const CoopGame& g) {
  std::string out = "map=" + (g.campaign.server ? g.campaign.server->map_name : std::string("-"));
  if (!g.failed.empty()) out += " (no change: " + g.failed + ")";
  const ge::console::Cvar* skill =
      g.campaign.server ? g.campaign.server->cvars.find("skill") : nullptr;
  out += " coop=1 skill=" + (skill ? skill->value : std::string("?"));
  out += " bots=" + std::to_string(cl::party_size(g.party));
  out += " joined=" + std::to_string(g.door != nullptr ? classic_coop().joined(g.door) : 0);
  if (g.door == nullptr) out += " offline";
  return out;
}

}  // namespace shooters_edge::net
