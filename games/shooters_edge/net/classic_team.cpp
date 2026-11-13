// A recorded team game of bots over our server (classic_team.hpp).
//
// voice: agent (a lab's harness: its errors land in the lab's report; docs/architecture.md § Player
// text).
//
// ge:summary The team game of bots: the map, the nav graph and its movers, the clients, the record
// ge:keywords run_team_match team-match movers doors plats fighters clients mvd digest
// ge:skill shooter bots net
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/net/classic_team.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/entity_text.hpp"
#include "shooter/locs.hpp"
#include "shooter/nav.hpp"
#include "shooter/swim.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/map_nav.hpp"
#include "shooters_edge/bots/mind.hpp"
#include "shooters_edge/bots/packs.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/bots/team.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/record_cvars.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_mirror.hpp"
#include "shooters_edge/net/classic_record.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/net/classic_team_nav.hpp"
#include "shooters_edge/net/classic_wire.hpp"
#include "shooters_edge/server/commands.hpp"

namespace shooters_edge::net {

namespace {

namespace bt = shooters_edge::bots;
namespace gn = ge::net;
namespace pg = ge::progs;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;

constexpr double kHz = 1000.0 / bt::kPilotMsec;
constexpr int kTickUs = 13000;
constexpr int32_t kItQuad = 4194304;
constexpr int32_t kItRockets = 32;  // the launcher's weapon bit

// The drill: every player's weapons cut to `bit` alone (the blade kept, so nothing is left with no
// weapon) and put in hand, and the ammunition it fires topped up when it runs under half (so a
// refill, which the hit book would read as a pickup, comes once every hundred shots or so).
void drill_frame(server::QcServer& s, int32_t bit) {
  constexpr int32_t kWeapons = 1 | 2 | 4 | 8 | 16 | 32 | 64;  // the baseline mod's guns
  constexpr int32_t kBlade = 4096;
  const pg::LayoutFields& f = s.q.layout->f;
  const int ammo = bit <= 2    ? f.ammo_shells
                   : bit <= 8  ? f.ammo_nails
                   : bit <= 32 ? f.ammo_rockets
                               : f.ammo_cells;
  for (int e = 1; e <= s.sv.set.maxclients; e++) {
    if (!s.clients[static_cast<size_t>(e - 1)].active || f.items < 0 || ammo < 0) continue;
    uint32_t* items = pg::entity_field(s.q.vm, e, f.items);
    const int32_t had = static_cast<int32_t>(pg::as_float(*items));
    *items = pg::as_word(static_cast<float>((had & ~kWeapons) | bit | kBlade));
    uint32_t* a = pg::entity_field(s.q.vm, e, ammo);
    if (pg::as_float(*a) < 50) *a = pg::as_word(100.f);
    // And in hand: a player comes in holding the starting gun, which he would fire before his
    // switch to the drill's weapon went through.
    if (f.weapon >= 0) {
      uint32_t* held = pg::entity_field(s.q.vm, e, f.weapon);
      const int32_t w = static_cast<int32_t>(pg::as_float(*held));
      if ((w & kWeapons) != 0 && w != bit) *held = pg::as_word(static_cast<float>(bit));
    }
  }
}

uint64_t fold(uint64_t h, uint32_t w) { return (h ^ w) * 1099511628211ull; }

uint64_t fold_f(uint64_t h, float f) {
  uint32_t b;
  std::memcpy(&b, &f, 4);
  return fold(h, b);
}

struct Seat {
  bt::Fighter fighter;
  bool ready = false;  // the fighter initialised on its slot
  bool alive = false;
  int team = 0, index = 0;
  sh::AimSkill skill = sh::AimSkill::average;
  bool had_quad = false;
  bool had_rl = false;
  size_t heard = 0;  // its client's prints read so far
  // The last frames' places and pilot modes (a ring), for where a death came from.
  struct Was {
    double t = -1;
    sh::Vec3f at;
    uint8_t mode = 0;
    sh::Vec3f from, to;  // the link it was on
    uint8_t kind = 0;
  };
  std::array<Was, 512> was{};
  size_t was_n = 0;
  std::unique_ptr<ClientMirror> mirror;  // client-fed: its world, off its client alone
};

}  // namespace

TeamMatchReport run_team_match(const TeamMatchOptions& opt) {
  TeamMatchReport r;
  if (!opt.progs) {
    r.error = "run_team_match: no progs; next: compile the baseline mod (baseline_progs)";
    return r;
  }
  std::unique_ptr<cl::LoadedMap> map = cl::map_from_bytes(opt.map, opt.map_name, r.error);
  if (!map) return r;
  if (!opt.entities.empty()) map->spec.entities = opt.entities;
  sv::QcServerOptions so;
  so.seed = opt.seed;
  so.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(*opt.progs, so, r.error);
  if (!qc) return r;
  sv::RulesRunner rules = sv::qc_runner(std::move(qc));
  sv::QcServer& q = *sv::qc_of(rules);
  rules.console("deathmatch " + std::to_string(opt.deathmatch));
  rules.console("teamplay 1");
  rules.console("timelimit " + std::to_string(opt.timelimit));
  for (const std::string& line : opt.settings) rules.console(line);
  if (!rules.start(map->spec, r.error)) return r;
  sv::use_map_sets(q, map->bsp);

  // The nav graph off the world and the movers (bots/map_nav.hpp).
  TeamNav tn;
  tn.given = opt.locs;
  if (!opt.view_book.entries.empty()) tn.view_book = &opt.view_book;
  bt::MapNav& nav = tn.nav;
  if (!bt::build_map_nav(q, map->spec, "info_player_deathmatch", nav, r.error)) {
    r.error = "run_team_match: " + r.error;
    return r;
  }
  sh::NavGraph& graph = nav.graph;
  sh::SwimField& swim = nav.swim;
  const std::vector<sh::NavMover>& movers = nav.movers;
  ge::ai::Search& best = nav.main;  // what the map's main part reaches
  const int32_t best_n = nav.main_n;
  r.teleport_links = nav.teleport_links;
  r.closed_links = nav.closed_links;
  r.swim_wet = swim.wet;
  r.swim_leading = swim.leading;
  r.nav_build_ms = nav.build_ms;
  r.nav_brink_links = nav.brink_links;
  r.nav_side_brink_links = nav.side_brink_links;
  r.lo = graph.lo;
  r.hi = graph.hi;
  r.nav_nodes = static_cast<int64_t>(graph.nodes.size());
  r.nav_links = static_cast<int64_t>(graph.links.size());
  r.movers = static_cast<int64_t>(movers.size());
  if (graph.nodes.empty()) {
    r.error = "run_team_match: the nav graph has no nodes; next: run the se-nav lab on the map";
    return r;
  }

  // The server over the simulated network, recording.
  gn::LinkConditions lan;
  gn::conditions_preset("lan", lan);
  if (opt.latency_ms > 0) {
    lan.latency_ms = opt.latency_ms;
    lan.jitter_ms = opt.latency_ms / 10;
  }
  gn::SimNetwork net = gn::make_sim_network(opt.seed, lan);
  gn::Transport st;
  gn::open_sim_transport(st, net, gn::sim_address(0, kPortServer));
  auto server = std::make_unique<ClassicServer>();
  ServerConfig cfg;
  cfg.hostname = "Shooter's Edge bots 4on4";
  cfg.antilag = opt.antilag;
  if (!classic_start(*server, rules, st, cfg, r.error)) return r;
  MultiviewRecorder mvd;
  if (opt.record) {
    mvd_start(mvd, false, opt.packed ? cl::kDefaultPackLevel : 0);
    server->mvd = &mvd;
  }

  // The players: a client each, a fighter at its controls once its slot is in the game.
  int n = 0;
  for (const TeamSide& side : opt.teams) n += static_cast<int>(side.skills.size());
  if (n < 2 || n > 16) {
    r.error = "run_team_match: 2..16 players; next: pass fewer skills";
    return r;
  }
  std::vector<Seat> seats(static_cast<size_t>(n));
  std::vector<gn::Transport> ct(static_cast<size_t>(n));
  auto clients = std::make_unique<ClassicClientEnd[]>(static_cast<size_t>(n));
  bt::Team teams[2];
  r.players.resize(static_cast<size_t>(n));
  sv::QcServer& s = q;
  {
    int k = 0;
    for (int t = 0; t < 2; t++) {
      const TeamSide& side = opt.teams[t];
      teams[t].name = side.name;
      for (size_t i = 0; i < side.skills.size(); i++, k++) {
        Seat& seat = seats[static_cast<size_t>(k)];
        seat.team = t;
        seat.index = k;
        seat.skill = side.skills[i];
        TeamPlayer& p = r.players[static_cast<size_t>(k)];
        p.name = i < side.names.size() ? side.names[i] : side.name + std::to_string(i + 1);
        p.team = t;
        p.skill = seat.skill;
        if (i < side.tempers.size()) p.temper = side.tempers[i];
        p.traits = sh::traits_of(p.skill, p.temper);
        if (i < side.traits.size() && !sh::apply_traits(side.traits[i], p.traits, r.error)) {
          r.error = p.name + ": " + r.error;
          return r;
        }
        p.features = sh::features_of(p.skill);
        if (!sh::apply_features(side.features, p.features, r.error)) {
          r.error = p.name + ": " + r.error;
          return r;
        }
        gn::open_sim_transport(ct[static_cast<size_t>(k)], net,
                               gn::sim_address(static_cast<uint32_t>(k + 1), kPortClient));
        ClientConfig cc;
        cc.name = p.name;
        cc.team = side.name;
        cc.topcolor = cc.bottomcolor = side.colour;
        cc.qport = static_cast<uint16_t>(200 + k);
        cc.seed = k + 1;
        ClassicClientEnd* c = &clients[static_cast<size_t>(k)];
        Seat* sp = &seat;
        cc.brain = [sp, c, &s, lead = opt.lead_lag, hook = opt.on_command,
                    hook_ctx = opt.on_command_ctx, k](int) -> sh::MoveCmd {
          if (!sp->ready) return {};
          // Client-fed, its world is what its client was sent, nothing more.
          if (sp->mirror) mirror_update(*sp->mirror, *c);
          const server::RulesCommon& w = sp->mirror ? sp->mirror->s : s;
          // The team lines its client was sent, read (or missed) by its own mind.
          for (; sp->heard < c->prints.size(); sp->heard++) {
            std::string who, words;
            if (bt::split_team_line(c->prints[sp->heard], who, words))
              bt::mind_hear(sp->fighter, who, words, w.sv.time);
            else
              bt::mind_obituary(sp->fighter, c->prints[sp->heard], w.sv.time);
            bt::packs_obituary(sp->fighter, w, c->prints[sp->heard], w.sv.time);
          }
          bt::packs_truth(sp->fighter, s);  // the lab's score of its guesses, off the server
          // Its shots led on the network's round trip, as on any server across one (bot.hpp).
          if (sp->mirror) {
            sp->fighter.bot.lag_s = lead ? static_cast<float>(c->latency_avg) : 0.f;
            const std::string antilag = info_get(c->serverinfo, "sv_antilag");
            sp->fighter.bot.rewound = !antilag.empty() && antilag != "0";
          }
          const sh::MoveCmd cmd = bt::fighter_command(sp->fighter, w, c->playernum, bt::kPilotMsec);
          if (hook != nullptr) hook(hook_ctx, k, sp->fighter, w, cmd);
          // The fighter's team messages, as a player types them.
          for (const std::string& line : sp->fighter.outbox)
            client_command(*c, "say_team \"" + line + "\"");
          sp->fighter.outbox.clear();
          return cmd;
        };
        client_start(*c, ct[static_cast<size_t>(k)], gn::sim_address(0, kPortServer), cc, 0);
      }
    }
  }

  const std::vector<sh::Vec3f>* known_items = nullptr;
  int64_t now = 0;
  uint64_t h = 1469598103934665603ull;
  HitBook hits;  // every player's shots and hits by weapon
  for (int frame = 0; frame < opt.frames; frame++) {
    now += kTickUs;
    gn::sim_advance(net, now);
    s.sv.sound_count = 0;
    classic_frame(*server, now);
    hits_frame(hits, s);
    if (opt.drill != 0) drill_frame(s, opt.drill);
    // Fighters take their slots once their players are in the game.
    for (int k = 0; k < n; k++) {
      Seat& seat = seats[static_cast<size_t>(k)];
      const int slot = clients[k].playernum;
      if (seat.ready || slot < 0 || !server->clients[slot].rules_active) continue;
      if (opt.client_fed) {
        // Its world off its client: started on the map file's models once the signon is in, and
        // set up on the baselines (every item the map has, as a client that loaded it knows).
        const ClassicClientEnd& ce = clients[k];
        if (ce.stage != ClientStage::active || !ce.player_seen[slot]) continue;
        seat.mirror = std::make_unique<ClientMirror>();
        if (!mirror_start(*seat.mirror, map->spec.models, ce, r.error)) return r;
        seat.mirror->baselines = true;
        mirror_update(*seat.mirror, ce);
        seat.mirror->baselines = false;
      }
      const server::RulesCommon& seen = seat.mirror ? seat.mirror->s : s;
      seat.fighter.features = r.players[static_cast<size_t>(k)].features;
      team_fighter_setup(tn, seat.fighter, seen, slot, map->spec.entities.c_str(), seat.skill,
                         r.players[static_cast<size_t>(k)].traits, kHz,
                         opt.seed + 31u * static_cast<uint32_t>(k),
                         r.players[static_cast<size_t>(k)].name);
      seat.fighter.bot.pilot.why = opt.why;
      seat.ready = true;
      if (r.locs_from.empty()) {
        r.locs_from = tn.locs.from;
        r.locs = static_cast<int64_t>(tn.locs.points.size());
        r.places = sh::loc_places(tn.locs);
      }
      r.dives = static_cast<int64_t>(tn.dives.size());
      r.players[static_cast<size_t>(k)].slot = slot;
      if (r.nav_reached.empty()) {
        ge::ai::Search search;
        sh::nav_reach(graph,
                      sh::nav_nearest(graph, s.sv.edicts[static_cast<size_t>(slot) + 1].origin),
                      search);
        r.nav_reached.assign(graph.nodes.size(), 0);
        for (size_t i = 0; i < graph.nodes.size(); i++)
          r.nav_reached[i] = search.reached(static_cast<int32_t>(i)) ? 1 : 0;
      }
      bt::Team& tm = teams[seat.team];
      tm.members.push_back(&seat.fighter);
      seat.fighter.team = &tm;
      bt::team_init(tm);
      for (bt::Fighter* f : tm.members) f->bot.senses.friends = tm.friends;
    }
    for (bt::Team& tm : teams) bt::team_count(tm);
    for (int k = 0; k < n; k++) client_tick(clients[k], now);
    r.frames = frame + 1;
    // Client-fed: how far each fighter's own place, as its world holds it, is from the server's.
    for (int k = 0; k < n; k++) {
      const Seat& seat = seats[static_cast<size_t>(k)];
      const int slot = clients[k].playernum;
      if (!seat.mirror || !seat.ready || slot < 0) continue;
      const sh::Edict& truth = s.sv.edicts[static_cast<size_t>(slot) + 1];
      if (truth.health <= 0) continue;
      const sh::Vec3f d =
          seat.mirror->s.sv.edicts[static_cast<size_t>(slot) + 1].origin - truth.origin;
      TeamPlayer& p = r.players[static_cast<size_t>(k)];
      p.mirror_err += std::sqrt(static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y +
                                static_cast<double>(d.z) * d.z);
      p.mirror_err_n++;
      p.lag_s += clients[k].latency;
      p.lag_n++;
    }

    // The key items lying untaken, and what the fighters made of each (every 4 frames). The items
    // are read in the same order by every fighter, so an index names one spot for all of them.
    if (frame % 4 == 0 && n > 0 && seats[0].ready) {
      const std::vector<bt::ItemSpot>& spots = seats[0].fighter.items;
      for (size_t i = 0; i < spots.size(); i++) {
        const bt::ItemSpot& sp = spots[i];
        int kind = -1;
        if (sp.kind == bt::ItemKind::armor1) kind = 0;
        if (sp.kind == bt::ItemKind::armor2) kind = 1;
        if (sp.kind == bt::ItemKind::armor3) kind = 2;
        if (sp.kind == bt::ItemKind::mega) kind = 3;
        if (sp.kind == bt::ItemKind::quad) kind = 4;
        if (sp.kind == bt::ItemKind::weapon && sp.weapon == kItRockets) kind = 5;
        if (kind < 0 || sp.ent < 0) continue;
        const sh::Edict& e = s.sv.edicts[static_cast<size_t>(sp.ent)];
        const auto ki = static_cast<size_t>(kind);
        r.lying_n[ki]++;
        if (e.free || e.modelindex <= 0 || e.solid == sh::kSolidNot) continue;
        r.lying[ki]++;
        int why = 0;
        for (int k = 0; k < n; k++) {
          const Seat& seat = seats[static_cast<size_t>(k)];
          const int sl = r.players[static_cast<size_t>(k)].slot;
          if (!seat.ready || sl < 0 || s.sv.edicts[static_cast<size_t>(sl) + 1].health <= 0)
            continue;
          const bt::Fighter& f = seat.fighter;
          if (i >= f.items.size()) continue;
          const bt::ItemSpot& mine = f.items[i];
          int w = mine.present && (mine.known || mine.in_sight) ? 1 : 0;
          if (f.goal_item == static_cast<int32_t>(i)) {
            const sh::Vec3f d = s.sv.edicts[static_cast<size_t>(sl) + 1].origin - mine.at;
            const float far = std::sqrt(d.x * d.x + d.y * d.y);
            w = f.mode != bt::FightMode::roam ? std::max(w, 2) : far < 64 ? 5 : far < 400 ? 4 : 3;
          }
          why = std::max(why, w);
        }
        r.lying_why[ki][static_cast<size_t>(why)]++;
        // Whether anyone alive could take it: an armour is refused to one whose own protection
        // (type times value) is at least its own; anything else anyone takes.
        const float gives = kind == 0 ? 0.3f * 100 : kind == 1 ? 0.6f * 150 : 0.8f * 200;
        bool taker = kind > 2;
        for (int k = 0; k < n && !taker; k++) {
          const int sl = r.players[static_cast<size_t>(k)].slot;
          if (sl < 0 || s.sv.edicts[static_cast<size_t>(sl) + 1].health <= 0) continue;
          const auto& num = s.world.clients[static_cast<size_t>(sl)].numbers;
          taker = num.armour_type * num.armour < gives;
        }
        if (taker) r.lying_takers[ki]++;
      }
    }

    // The report's frame.
    for (int k = 0; k < n; k++) {
      Seat& seat = seats[static_cast<size_t>(k)];
      TeamPlayer& p = r.players[static_cast<size_t>(k)];
      if (!seat.ready) continue;
      const sh::Edict& d = s.sv.edicts[static_cast<size_t>(p.slot) + 1];
      const bool live = d.health > 0;
      if (live) {
        const sh::Pilot& pw = seat.fighter.bot.pilot.pilot;
        Seat::Was w{s.sv.time, d.origin, static_cast<uint8_t>(pw.mode), {}, {}, 0};
        if (pw.g && pw.k + 1 < pw.route.size()) {
          w.from = pw.g->nodes[static_cast<size_t>(pw.route[pw.k])].origin;
          w.to = pw.g->nodes[static_cast<size_t>(pw.route[pw.k + 1])].origin;
          const sh::NavNode& na = pw.g->nodes[static_cast<size_t>(pw.route[pw.k])];
          for (int32_t x = 0; x < na.links; x++) {
            const sh::NavLink& l =
                pw.g->links[static_cast<size_t>(na.first_link) + static_cast<size_t>(x)];
            if (l.to == pw.route[pw.k + 1]) w.kind = static_cast<uint8_t>(l.kind);
          }
        }
        seat.was[seat.was_n++ % seat.was.size()] = w;
      }
      if (seat.alive && !live) {
        p.deaths++;
        // Where it was three seconds before.
        Seat::Was back;
        for (const Seat::Was& w : seat.was)
          if (w.t >= 0 && w.t <= s.sv.time - 3 && w.t > back.t) back = w;
        const sh::Pilot& pl = seat.fighter.bot.pilot.pilot;
        const char* link = "-";
        if (pl.g && pl.k + 1 < pl.route.size()) {
          const sh::NavNode& a = pl.g->nodes[static_cast<size_t>(pl.route[pl.k])];
          for (int32_t x = 0; x < a.links; x++) {
            const sh::NavLink& l =
                pl.g->links[static_cast<size_t>(a.first_link) + static_cast<size_t>(x)];
            if (l.to == pl.route[pl.k + 1]) link = sh::nav_kind_name(l.kind);
          }
        }
        const char* liquid = d.watertype == sh::kContentsLava    ? "lava"
                             : d.watertype == sh::kContentsSlime ? "slime"
                             : d.watertype == sh::kContentsWater ? "water"
                                                                 : "-";
        if (d.watertype == sh::kContentsLava || d.watertype == sh::kContentsSlime) p.lava_deaths++;
        // And its way in: where it was 2, 1 and a half second before.
        std::string way;
        for (const double ago : {3.0, 2.5, 2.0, 1.5, 1.0, 0.5}) {
          Seat::Was at;
          for (const Seat::Was& w : seat.was)
            if (w.t >= 0 && w.t <= s.sv.time - ago && w.t > at.t) at = w;
          char c[128];
          std::snprintf(c, sizeof c, "%s%.0f %.0f %.0f %s", way.empty() ? "" : " > ", at.at.x,
                        at.at.y, at.at.z, sh::pilot_mode_name(static_cast<sh::PilotMode>(at.mode)));
          way += c;
          if (ago > 3.5) continue;
          std::snprintf(c, sizeof c, " [%s %.0f %.0f %.0f to %.0f %.0f %.0f]",
                        sh::nav_kind_name(static_cast<sh::NavKind>(at.kind)), at.from.x, at.from.y,
                        at.from.z, at.to.x, at.to.y, at.to.z);
          way += c;
        }
        char b[1400];
        std::snprintf(b, sizeof b, "%.1f\t%s\t%.0f %.0f %.0f\t%s\t%s\t%s\t%.0f %.0f %.0f\t%s\t%s",
                      s.sv.time, p.name.c_str(), d.origin.x, d.origin.y, d.origin.z, liquid,
                      sh::pilot_mode_name(pl.mode), link, back.at.x, back.at.y, back.at.z,
                      sh::pilot_mode_name(static_cast<sh::PilotMode>(back.mode)), way.c_str());
        r.deaths.push_back(b);
      }
      seat.alive = live;
      const bt::Stats rst = bt::read_stats(s, p.slot + 1, bt::first_arsenal());
      const bool quad = (rst.items & kItQuad) != 0;
      if (quad && !seat.had_quad) r.teams[seat.team].quad_pickups++;
      seat.had_quad = quad;
      // The launcher taken (off the map or out of a pack), and the rockets in hand when it was.
      const bool rl = live && (rst.items & kItRockets) != 0;
      if (rl && !seat.had_rl) p.rl_pickups++;
      seat.had_rl = rl;
      if (live && frame % 4 == 0) {
        p.rl_samples += rl;
        p.rl_loaded += rl && bt::launcher_rounds(rst) > 0;
      }
      if (frame % opt.path_every == 0) {
        p.path.push_back(d.origin);
        p.roles.push_back(live ? static_cast<uint8_t>(seat.fighter.role) : 255);
        p.known.push_back(static_cast<int16_t>(bt::items_known(seat.fighter)));
      }
      // How often an enemy stands in plain view of him (in range, a clear line, any way he looks),
      // against how often he sees one (in his cone): a bot that roams with nobody known while
      // an enemy is in view misses him; one with nobody in view is in the wrong place.
      if (live && frame % 4 == 0) {
        sh::SenseSettings all = seat.fighter.bot.senses.set;
        all.fov = 360;
        const sh::Vec3f eye{d.origin.x, d.origin.y, d.origin.z + all.view_height};
        bool any = false;
        float near = 1e30f, near_off = 0;  // the nearest in plain view, and his yaw off the view
        for (int j = 0; j < n; j++) {
          const Seat& o = seats[static_cast<size_t>(j)];
          const int os = r.players[static_cast<size_t>(j)].slot;
          if (!o.ready || o.team == seat.team || os < 0) continue;
          const sh::Edict& e = s.sv.edicts[static_cast<size_t>(os) + 1];
          if (e.health <= 0 || !sh::sees_player(s.sv, all, p.slot + 1, eye, {}, e.origin)) continue;
          any = true;
          const float dx = e.origin.x - eye.x, dy = e.origin.y - eye.y;
          const float dd = dx * dx + dy * dy;
          if (dd < near) {
            near = dd;
            float yaw = 0, pitch = 0;
            sh::aim_angles(eye, e.origin, yaw, pitch);
            float off = std::fmod(yaw - seat.fighter.bot.yaw, 360.f);
            if (off > 180) off -= 360;
            if (off < -180) off += 360;
            near_off = std::fabs(off);
          }
        }
        bool seen = false;
        for (const sh::Contact& c : seat.fighter.bot.senses.contacts) seen = seen || c.visible;
        const sh::Vec3f vel = s.world.entities[static_cast<size_t>(p.slot) + 1].velocity;
        const float vh = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        if (!any && vh > 100) {
          const sh::Bot& bb = seat.fighter.bot;
          const sh::PilotMode pm = bb.pilot.pilot.mode;
          const bool walk = pm == sh::PilotMode::walk || pm == sh::PilotMode::idle ||
                            pm == sh::PilotMode::wait || pm == sh::PilotMode::arrived ||
                            pm == sh::PilotMode::ride;
          const int holder = bb.aiming    ? 0
                             : bb.looking ? 1
                             : walk       ? (bb.calm ? 2 : 3)
                             : (pm == sh::PilotMode::hop || pm == sh::PilotMode::strafe ||
                                pm == sh::PilotMode::air || pm == sh::PilotMode::jump ||
                                pm == sh::PilotMode::run_up)
                                 ? 4
                                 : 5;
          float off = static_cast<float>(
              std::fmod(static_cast<double>(seat.fighter.view_sent[1]) -
                            ge::core::libm::atan2(vel.y, vel.x) * 180 / ge::core::kPi,
                        360.0));
          if (off > 180) off -= 360;
          if (off < -180) off += 360;
          if (holder == 2) {
            float ro = static_cast<float>(
                std::fmod(static_cast<double>(bb.pilot.pilot.yaw) -
                              ge::core::libm::atan2(vel.y, vel.x) * 180 / ge::core::kPi,
                          360.0));
            if (ro > 180) ro -= 360;
            if (ro < -180) ro += 360;
            p.calm_route_off += std::fabs(ro) > 45;
          }
          p.moving_n[holder]++;
          p.moving_off[holder] += std::fabs(off) > 45;
        }
        if (any && !seen) {
          p.unseen_off[near_off < 60 ? 0 : near_off < 90 ? 1 : near_off < 135 ? 2 : 3]++;
          p.unseen_calm += seat.fighter.bot.calm && !seat.fighter.bot.aiming &&
                           (seat.fighter.bot.pilot.pilot.mode == sh::PilotMode::walk ||
                            seat.fighter.bot.pilot.pilot.mode == sh::PilotMode::idle ||
                            seat.fighter.bot.pilot.pilot.mode == sh::PilotMode::wait ||
                            seat.fighter.bot.pilot.pilot.mode == sh::PilotMode::arrived);
        }
        p.view_n++;
        p.in_view += any;
        p.seen_n += seen;
        p.unnoticed += any && seat.fighter.mode == bt::FightMode::roam;
        // With one in plain view: did it see him, aim at him, hold its fire?
        if (any) {
          p.in_view_seen += seen;
          p.in_view_aiming += seat.fighter.bot.aiming;
          p.in_view_reacting += sh::bot_reacting(seat.fighter.bot, s.sv.time);
        }
      }
      if (live) {
        static_assert(std::size(TeamPlayer{}.role_commands) == bt::kRoles);
        p.role_commands[static_cast<int>(seat.fighter.role)]++;
        p.pilot_modes[static_cast<int>(seat.fighter.bot.pilot.pilot.mode)]++;
      }
      h = fold_f(fold_f(fold_f(fold_f(h, d.origin.x), d.origin.y), d.origin.z), d.health);
    }
    if (frame % opt.path_every == 0) {
      float tf[2] = {0, 0};
      for (const TeamPlayer& p : r.players)
        if (p.slot >= 0) tf[p.team] += s.world.clients[p.slot].numbers.frags;
      r.team_frags[0].push_back(tf[0]);
      r.team_frags[1].push_back(tf[1]);
    }
    if (!known_items)
      for (const Seat& seat : seats)
        if (seat.ready) {
          r.item_at.clear();
          r.item_name.clear();
          r.item_node_at.clear();
          r.item_routed.clear();
          for (const bt::ItemSpot& it : seat.fighter.items) {
            r.item_at.push_back(it.at);
            r.item_name.push_back(it.name);
            r.item_node_at.push_back(it.node >= 0 ? graph.nodes[static_cast<size_t>(it.node)].origin
                                                  : sh::Vec3f{});
            r.item_routed.push_back(it.node >= 0 && best_n > 0 && best.reached(it.node));
          }
          r.buttons.clear();
          for (const bt::ButtonSpot& b : seat.fighter.button_spots) {
            TeamButton tb;
            tb.at = b.at;
            tb.shoot = b.shoot;
            for (int d : b.doors) {
              const sh::Edict& de = s.sv.edicts[static_cast<size_t>(d)];
              tb.doors.push_back({(de.absmin.x + de.absmax.x) * 0.5f,
                                  (de.absmin.y + de.absmax.y) * 0.5f,
                                  (de.absmin.z + de.absmax.z) * 0.5f});
            }
            for (int32_t i : b.opens)
              tb.opens.push_back(seat.fighter.items[static_cast<size_t>(i)].name);
            if (b.node >= 0) tb.from = graph.nodes[static_cast<size_t>(b.node)].origin;
            r.buttons.push_back(std::move(tb));
          }
          known_items = &r.item_at;
          break;
        }
    if (s.map_stopped) {
      r.error = "the mod stopped the map: " + s.stop_reason +
                "; next: read the console's last lines and the fault's QC stack";
      break;
    }
    if (!s.pending_map.empty()) break;  // the match ended and the mod asked for the map again
  }
  if (opt.record) {
    mvd_stop(mvd);
    r.mvd = std::move(mvd.bytes);
  }
  r.seconds = s.sv.time;
  for (int k = 0; k < n; k++) {
    TeamPlayer& p = r.players[static_cast<size_t>(k)];
    const Seat& seat = seats[static_cast<size_t>(k)];
    if (p.slot >= 0) p.frags = s.world.clients[p.slot].numbers.frags;
    const bt::FighterStats& fs = seat.fighter.stats;
    p.shots = fs.shots;
    p.pickups = fs.pickups;
    p.switches = fs.switches;
    p.packs = fs.packs;
    p.held_fire = fs.held_fire;
    p.trigger_checks = fs.trigger_checks;
    p.trigger_far = fs.trigger_far;
    p.trigger_off = fs.trigger_off;
    p.reactions = seat.fighter.bot.stats.reactions;
    p.pre_aimed = seat.fighter.bot.stats.pre_aimed;
    p.strokes = seat.fighter.bot.hand.stats.strokes;
    p.lifts = seat.fighter.bot.hand.stats.lifts;
    p.target_kept = seat.fighter.bot.stats.kept;
    p.sound_swipes = seat.fighter.bot.stats.sound_swipes;
    p.react_s = seat.fighter.bot.stats.react_s;
    if (p.slot >= 0 && static_cast<size_t>(p.slot) < hits.players.size())
      p.hits = hits.players[static_cast<size_t>(p.slot)];
    r.pellets_at = hits.pellets_at;
    r.rockets_at = hits.rockets_at;
    r.given_at = hits.given_at;
    r.frames_at = hits.frames_at;
    if (p.slot >= 0 && static_cast<size_t>(p.slot) < hits.given.size())
      p.given = hits.given[static_cast<size_t>(p.slot)];
    if (p.slot >= 0 && static_cast<size_t>(p.slot) < hits.lead_n.size()) {
      p.lead_sum = hits.lead_sum[static_cast<size_t>(p.slot)];
      p.lead_abs = hits.lead_abs[static_cast<size_t>(p.slot)];
      p.lead_n = hits.lead_n[static_cast<size_t>(p.slot)];
    }
    p.dives = seat.fighter.dives;
    p.dives_given_up = fs.dives_given_up;
    p.dive_wet_s = fs.dive_wet_s;
    p.off_graph = seat.fighter.lost;
    for (int m = 0; m < 4; m++) p.mode_commands[m] = fs.mode_commands[m];
    for (int w = 0; w < 9; w++) p.weapon_commands[w] = fs.weapon_commands[w];
    const sh::PilotStats& ps = seat.fighter.bot.pilot.pilot.stats;
    if (p.slot >= 0) {
      const bt::Fighter& f = seat.fighter;
      const sh::Vec3f o = s.sv.edicts[static_cast<size_t>(p.slot) + 1].origin;
      char b[256];
      const sh::Vec3f gn =
          f.goal_node >= 0 ? graph.nodes[static_cast<size_t>(f.goal_node)].origin : sh::Vec3f{};
      std::snprintf(b, sizeof b,
                    "at %.0f %.0f %.0f, %s, goal node %.0f %.0f %.0f, item %s%s, pilot %s, buttons "
                    "%lld, rockets held %lld commands",
                    o.x, o.y, o.z, bt::role_name(f.role), gn.x, gn.y, gn.z,
                    f.goal_item >= 0 ? f.items[static_cast<size_t>(f.goal_item)].name.c_str() : "-",
                    f.unlocking ? " (button first)" : "",
                    sh::pilot_mode_name(f.bot.pilot.pilot.mode), static_cast<long long>(f.buttons),
                    static_cast<long long>(fs.weapon_commands[7]));
      p.last = b;
    }
    p.plans = ps.plans;
    p.goal_changes = seat.fighter.stats.goal_changes;
    p.surfaced = seat.fighter.stats.surfaced;
    p.surface_failed = seat.fighter.stats.surface_failed;
    p.swim_fights = seat.fighter.stats.swim_fights;
    {
      const bt::FighterStats& pk = seat.fighter.stats;
      for (size_t pf = 0; pf < 5; pf++) p.packs_known[pf] = pk.packs_known[pf];
      p.pack_guesses = pk.pack_guesses;
      p.pack_right = pk.pack_guess_right;
      p.pack_guess_rl = pk.pack_guess_rl;
      p.pack_truth_rl = pk.pack_truth_rl;
      p.pack_trips = pk.pack_trips;
      p.pack_reached = pk.pack_reached;
      p.pack_seen_gone = pk.pack_seen_gone;
    }
    p.wet = seat.fighter.stats.wet;
    for (size_t w = 0; w < 12; w++) {
      p.wet_why[w] = seat.fighter.stats.wet_why[w];
    }
    for (size_t w = 0; w < 3; w++) {
      p.wet_speed[w] = seat.fighter.stats.wet_speed[w];
      p.wet_n[w] = seat.fighter.stats.wet_n[w];
      p.wet_still[w] = seat.fighter.stats.wet_still[w];
    }
    p.on_the_way = seat.fighter.stats.on_the_way;
    for (size_t w = 0; w < size_t(profiles::kItemKinds); w++)
      p.took[w] = seat.fighter.stats.took[w];
    p.stuck = ps.stuck;
    p.lost = ps.lost;
    p.waits = ps.waits;
    p.barred = ps.barred;
    for (size_t i = 1; i < p.path.size(); i++) {
      const sh::Vec3f d = p.path[i] - p.path[i - 1];
      const float l = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
      if (l < 160) p.travelled += l;
    }
    r.teams[p.team].frags += p.frags;
    r.teams[p.team].deaths += p.deaths;
  }
  for (int t = 0; t < 2; t++) {
    const bt::TeamStats& ts = teams[t].stats;
    r.teams[t].plans = ts.plans;
    r.teams[t].rushes = ts.rushes;
    r.teams[t].quads = ts.quads;
    r.teams[t].held_shots = ts.held_shots;
    r.teams[t].callouts = ts.callouts;
    r.teams[t].messages = ts.messages;
    r.teams[t].lost_seen = ts.lost_seen;
    r.teams[t].retakes = ts.retakes;
    r.teams[t].fetches = ts.fetches;
    r.teams[t].fetched = ts.fetched;
    for (int e = 0; e < 5; e++) r.teams[t].fetch_end[e] = ts.fetch_end[e];
    r.teams[t].pushes = ts.pushes;
    r.teams[t].joins = ts.joins;
    r.teams[t].spam_shots = ts.spam_shots;
  }
  if (tn.plan_made && !seats.empty()) {
    r.plan = bt::plan_text(tn.plan, seats[0].fighter.items);
    r.goals = bt::goals_text(tn.goals);
    r.plan_areas = static_cast<int32_t>(tn.plan.areas.size());
    r.plan_base = tn.plan.base;
  }
  for (size_t k = 0; k < seats.size(); k++) {
    const bt::Mind& mn = seats[k].fighter.mind;
    if (k == 0) r.prints = clients[0].prints;
    r.players[k].read = mn.read;
    r.players[k].missed = mn.missed;
    const bt::Fighter& fi = seats[k].fighter;
    r.players[k].sounds = fi.hear_stats.sounds;
    r.players[k].heard = fi.hear_stats.taken;
    r.players[k].heard_taken = fi.stats.heard_taken;
    r.players[k].heard_spawns = fi.stats.heard_spawns;
    r.players[k].spam_shots = fi.stats.spam_shots;
    r.players[k].outmatched = fi.stats.outmatched;
    r.players[k].sound_chases = fi.stats.sound_chases;
    r.players[k].dodging = fi.stats.dodging;
    r.players[k].dodge_turns = fi.stats.dodge_turns;
    r.players[k].press_weighed = fi.stats.press_weighed;
    r.players[k].press_on = fi.stats.press_on;
    r.players[k].press_back = fi.stats.press_back;
    r.players[k].press_near = fi.stats.press_near;
    r.players[k].press_blocked = fi.stats.press_blocked;
    r.players[k].rocket_jumps = fi.bot.pilot.pilot.stats.rocket_jumps;
    r.players[k].flips = fi.bot.pilot.pilot.stats.flips;
    r.players[k].escapes = fi.bot.pilot.pilot.stats.escapes;
    r.players[k].escapes_out = fi.bot.pilot.pilot.stats.escapes_out;
    r.players[k].gap_jumps = fi.bot.pilot.pilot.stats.gap_jumps;
    r.players[k].trap_presses = fi.stats.trap_presses;
    r.players[k].trap_fired = fi.stats.trap_fired;
    r.players[k].trap_opens = fi.stats.trap_opens;
    r.players[k].trap_kept = fi.stats.trap_kept;
    r.players[k].gap_landed = fi.bot.pilot.pilot.stats.gap_landed;
    r.players[k].gap_refused = fi.bot.pilot.pilot.stats.gap_refused;
    r.players[k].gap_near = fi.bot.pilot.pilot.stats.gap_near;
    r.players[k].hops = fi.bot.pilot.pilot.stats.hops;
    r.players[k].hop_jumps = fi.bot.pilot.pilot.stats.hop_jumps;
    r.players[k].circles = fi.bot.pilot.pilot.stats.circles;
    r.players[k].circle_speed = fi.bot.pilot.pilot.stats.circle_speed;
    {
      const sh::PilotStats& ps = fi.bot.pilot.pilot.stats;
      r.players[k].recipe_asks = ps.recipe_asks;
      r.players[k].recipe_commits = ps.recipe_commits;
      r.players[k].recipe_aborts = ps.recipe_aborts;
      r.players[k].recipe_takeoffs = ps.recipe_takeoffs;
      r.players[k].recipe_speed = ps.recipe_speed;
      r.players[k].chained = fi.stats.chained;
      r.players[k].vias = ps.vias;
      r.players[k].retargets = ps.retargets;
      r.players[k].turned_round = fi.stats.turned_round;
      r.players[k].run_ups = ps.run_ups;
      r.players[k].jumps = ps.jumps;
      r.players[k].jumps_flown = ps.jumps_flown;
      r.players[k].via_touched = ps.via_touched;
      r.players[k].via_missed = ps.via_missed;
      r.players[k].via_speed = ps.via_speed;
    }
    r.players[k].watching = fi.stats.watching;
    r.players[k].speed_sum = fi.stats.speed_sum;
    for (int m = 0; m <= sh::kPilotModes; m++) {
      r.players[k].mode_speed[m] = fi.stats.mode_speed[m];
      r.players[k].mode_n[m] = fi.stats.mode_n[m];
    }
    if (seats[k].mirror) {
      r.players[k].mirrored = seats[k].mirror->updates;
      r.players[k].predicted = seats[k].mirror->predicted;
    }
    r.lines_read += mn.read;
    r.lines_missed += mn.missed;
  }
  // The teams' messages, merged in time order (a tie: the first team first).
  {
    std::vector<std::pair<double, std::string>> lines;
    for (int t = 0; t < 2; t++)
      for (const bt::TeamLine& l : teams[t].log) {
        std::string who = "?";
        for (const TeamPlayer& p : r.players)
          if (p.slot == l.slot) who = p.name;
        char at[32];
        std::snprintf(at, sizeof at, "%6.1f ", l.t);
        lines.push_back({l.t, at + teams[t].name + " " + who + ": " + l.text});
      }
    std::stable_sort(lines.begin(), lines.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    for (auto& l : lines) r.chat.push_back(std::move(l.second));
    for (const bt::TeamLine& l : teams[0].log)
      for (TeamPlayer& p : r.players) p.said += p.slot == l.slot ? 1 : 0;
    for (const bt::TeamLine& l : teams[1].log)
      for (TeamPlayer& p : r.players) p.said += p.slot == l.slot ? 1 : 0;
  }
  r.hold_item = teams[0].hold_item;
  r.quad_item = teams[0].quad_item;
  r.frag_log = s.frag_log;
  r.faults = s.q.faults;
  r.digest = h;
  r.ok = r.error.empty();
  return r;
}

const char* const kTeamHallsEntities = R"({
"classname" "worldspawn"
"message" "The fixture halls"
}
{
"classname" "info_player_deathmatch"
"origin" "64 64 24"
"angle" "0"
}
{
"classname" "info_player_deathmatch"
"origin" "448 64 24"
"angle" "90"
}
{
"classname" "info_player_deathmatch"
"origin" "64 320 24"
"angle" "270"
}
{
"classname" "info_player_deathmatch"
"origin" "448 320 24"
"angle" "180"
}
{
"classname" "info_player_deathmatch"
"origin" "800 128 24"
"angle" "90"
}
{
"classname" "info_player_deathmatch"
"origin" "800 400 24"
"angle" "180"
}
{
"classname" "info_player_deathmatch"
"origin" "224 700 24"
"angle" "270"
}
{
"classname" "info_player_deathmatch"
"origin" "680 760 24"
"angle" "180"
}
{
"classname" "item_armor2"
"origin" "544 672 8"
}
{
"classname" "item_artifact_super_damage"
"origin" "800 256 32"
}
{
"classname" "weapon_rocketlauncher"
"origin" "256 96 8"
}
{
"classname" "item_health"
"origin" "224 520 8"
"spawnflags" "2"
}
{
"classname" "item_health"
"origin" "96 192 8"
}
{
"classname" "item_health"
"origin" "640 400 8"
}
{
"classname" "item_health"
"origin" "420 760 8"
}
{
"classname" "item_shells"
"origin" "400 300 8"
}
{
"classname" "item_rockets"
"origin" "160 300 8"
}
{
"classname" "item_rockets"
"origin" "700 600 8"
}
{
"classname" "weapon_supershotgun"
"origin" "600 120 8"
}
{
"classname" "weapon_grenadelauncher"
"origin" "450 600 8"
}
{
"classname" "item_spikes"
"origin" "260 640 8"
}
{
"classname" "weapon_supernailgun"
"origin" "700 300 8"
}
)";

}  // namespace shooters_edge::net
