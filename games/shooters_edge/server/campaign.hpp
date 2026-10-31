// THE CAMPAIGN HOST: a single-player program (the classic game's own, or a campaign mod) played
// as a chain of maps, the players' state carried from map to map (porting/shooters-edge/
// campaign.md is the specification; docs/shooters-edge.md § Profiles says where it sits).
//
// A campaign owns one mod server (qc_server.hpp) at a time, made afresh for every map, and what
// outlives a map: the settings, `serverflags` and every player's spawn parms.
//
// A MAP'S START (`start_level`): the settings carried from the last map; `skill` rounded to the
// nearest whole number, clamped to 0..3 and written back; `registered` from the content (the
// host's `CampaignOptions::registered`: the packs carry the full game); `coop` on forces
// `deathmatch` 0; the program's `serverflags`, `deathmatch`, `coop` and `teamplay` written before
// the world spawns (qc_server.hpp's `load_map`); the loop on the single-player clock
// (shooter/frame.hpp), the server's time 1.0, and two physics frames of 0.1 s run before anyone
// joins; then every player enters: with SetNewParms on a new game, with the parms SetChangeParms
// left on a level change, with the parms the map was entered with on a restart.
//
// THE CHANGES (console lines, the program's `changelevel` builtin, or the host's calls):
// `map <name>` a new game (`serverflags` 0, every player's parms new); `changelevel <name>`
// SetChangeParms for every player, their parms and the program's `serverflags` kept, the next map
// started; `restart` the same map, the parms it was entered with, `serverflags` as it was then.
// `campaign_frame` acts on a change at the end of the frame that asked for it.
//
// THE STATISTICS (`campaign_stats`): the four the program counts in its globals (secrets and
// monsters, totals and found), what a joining client is sent as statistics 11..14.
//
// ge:summary The campaign host: a single-player program's maps in a chain, parms and runes kept
// ge:keywords campaign single-player host changelevel restart new-game spawn-parms serverflags
// ge:keywords runes skill coop deathmatch registered episodes settle single-player-clock
// ge:keywords statistics secrets monsters map-loader
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_test.cpp
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

constexpr int kSettleFrames = 2;            // physics frames before anyone joins
constexpr double kSettleSeconds = 0.1;      // each one's length, s
constexpr double kCampaignStep = 1.0 / 72;  // the single-player host's frame, s

// A map by name: `out` filled. The loader owns whatever the spec's models point into, and keeps it
// until its next call (the campaign drops the old map's server before asking for the next).
using MapLoader = bool (*)(void* ctx, const std::string& name, MapSpec& out, std::string& error);

struct CampaignOptions {
  QcServerOptions server;  // the loop's settings (the clock and the step are the campaign's)
  MapLoader load = nullptr;
  void* load_ctx = nullptr;
  bool registered = false;            // the content is the full game (the packs carry gfx/pop.lmp)
  std::vector<std::string> settings;  // console lines run before the first map (skill 2, coop 1)
};

// A player the campaign carries across maps (slot k is entity k + 1).
struct CampaignPlayer {
  bool active = false;
  std::string name;
  bool carried = false;    // `parms` came from SetChangeParms (or a restart): no SetNewParms
  float parms[16] = {};    // what the next map's ClientConnect reads
  float entered[16] = {};  // what this map was entered with (a restart's)
};

struct CampaignStats {
  float total_secrets = 0, total_monsters = 0, found_secrets = 0, killed_monsters = 0;
};

struct Campaign {
  CampaignOptions opt;
  const pg::Progs* progs = nullptr;
  std::unique_ptr<QcServer> server;  // this map's
  MapSpec map;
  float serverflags = 0;          // carried into the next map
  float entered_serverflags = 0;  // what this map started with (a restart's)
  CampaignPlayer players[sh::kMaxClients];
  std::vector<std::pair<std::string, std::string>> settings;  // carried: every changed cvar
  std::vector<std::string> history;  // every map started, in order: "new e1m1", "change e1m2"
};

// A new game on `map` for the program (which must outlive the campaign): every player slot empty.
// False with `error` when the map does not load.
bool campaign_new_game(Campaign& c, const pg::Progs& progs, const CampaignOptions& opt,
                       const std::string& map, std::string& error);
// A player joins this map now (SetNewParms, ClientConnect, PutClientInServer) and is carried on.
void campaign_join(Campaign& c, int slot, const std::string& name);
// The map `map` started for a saved game (campaign_save.hpp): the program and options taken, the
// campaign's carried state (`settings`, `serverflags`, the players) as the caller set it, the
// spawn functions run and the map settled, and nobody entered: the save's state is read over it.
// The history gains "load <map>". False with `error` when the map does not load.
bool campaign_reopen(Campaign& c, const pg::Progs& progs, const CampaignOptions& opt,
                     const std::string& map, std::string& error);
// One frame of the single-player host; then the change a console line or the program asked for.
// False with `error` when the next map does not load (the campaign has no server then).
bool campaign_frame(Campaign& c, const sh::Packet* packets, int count, std::string& error);
// A change by hand: what `map`, `changelevel` and `restart` do.
bool campaign_change(Campaign& c, MapChange change, const std::string& map, std::string& error);
// The four statistics the program keeps (zeros when its form has none).
CampaignStats campaign_stats(const QcServer& s);

}  // namespace shooters_edge::server
