// The listen server (listen.hpp).
//
// ge:summary The listen server: the mod server and its local player, maps loaded and changed
// ge:keywords listen-server listen_frame listen_advance load_named_map baseline_progs forced-view
// ge:skill shooter progs
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/listen.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <utility>
#include <vector>

#include "core/num.hpp"
#include "progs/layout.hpp"
#include "progs/qc_compile.hpp"
#include "progs/vm.hpp"
#include "shooter/client.hpp"
#include "shooters_edge/server/announce.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/campaign_save.hpp"
#include "shooters_edge/server/classic_save.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"

// The baseline mod's sources, built into the game (CMakeLists.txt: ge_embed_binary), in the
// order its project file compiles them.
extern const unsigned char se_qc_arena_engine[];
extern const std::size_t se_qc_arena_engine_size;
extern const unsigned char se_qc_common_defs[];
extern const std::size_t se_qc_common_defs_size;
extern const unsigned char se_qc_arena_glue[];
extern const std::size_t se_qc_arena_glue_size;
extern const unsigned char se_qc_common_util[];
extern const std::size_t se_qc_common_util_size;
extern const unsigned char se_qc_common_combat[];
extern const std::size_t se_qc_common_combat_size;
extern const unsigned char se_qc_common_weapons[];
extern const std::size_t se_qc_common_weapons_size;
extern const unsigned char se_qc_common_items[];
extern const std::size_t se_qc_common_items_size;
extern const unsigned char se_qc_common_dmm4[];
extern const std::size_t se_qc_common_dmm4_size;
extern const unsigned char se_qc_common_player[];
extern const std::size_t se_qc_common_player_size;
extern const unsigned char se_qc_common_map[];
extern const std::size_t se_qc_common_map_size;
extern const unsigned char se_qc_arena_world[];
extern const std::size_t se_qc_arena_world_size;

namespace shooters_edge::client {

namespace pg = ge::progs;

namespace {

// The message bytes of the baseline mod's temporary entities (qc/common/defs.qc's SVC_TEMPENTITY
// and its TE_ kinds, the multiplayer protocol's layout: a gunshot and blood carry a count).
constexpr uint8_t kTempEntity = 23, kExplosion = 3;

// The player's own server lets the easy jump (pm_speedjump) take 0.8 of the best strafe's gain; it
// acts only once he asks for it (cl_speedjump 1). A server.cfg or the console may change it.
constexpr float kListenSpeedJump = 0.8f;

// A match's temporary entity (one multicast's bytes) as an effect the window draws: false for a
// kind it does not draw or bytes cut short. The explosion is Listen::explosions' already.
bool arena_effect(std::span<const uint8_t> b, Effect& e) {
  if (b.size() < 2 || b[0] != kTempEntity) return false;
  size_t i = 2;
  const auto coord = [&] {
    const auto raw = static_cast<int16_t>(b[i] | (b[i + 1] << 8));
    i += 2;
    return static_cast<float>(raw) / 8.0f;
  };
  const auto point = [&] {
    const float x = coord(), y = coord();
    return sh::Vec3f{x, y, coord()};
  };
  e.code = b[1];
  size_t body = 6;
  switch (b[1]) {
    case 0:  // a spike, a big spike, the two monsters' spikes
    case 1:
    case 7:
    case 8:
      e.kind = EffectKind::spike;
      e.count = b[1] == 1 ? 2 : 1;
      break;
    case 2:  // a gunshot: its count of pellets
      e.kind = EffectKind::impact;
      body = 7;
      break;
    case 5:  // the beams: an entity and two points
    case 6:
    case 9:
      e.kind = EffectKind::beam;
      body = 14;
      break;
    case 10:
      e.kind = EffectKind::lava;
      break;
    case 11:
      e.kind = EffectKind::teleport;
      break;
    case 12:  // blood: its count; the beam's blood
      e.kind = EffectKind::blood;
      body = 7;
      break;
    case 13:
      e.kind = EffectKind::blood;
      break;
    default:
      return false;
  }
  if (b.size() < 2 + body) return false;
  if (body == 7) e.count = b[i++];
  if (e.kind == EffectKind::beam) {
    e.entity = static_cast<int16_t>(b[i] | (b[i + 1] << 8));
    i += 2;
  }
  e.at = point();
  e.end = e.kind == EffectKind::beam ? point() : e.at;
  return true;
}

// A mod's named effect for the player (pointparticles, trailparticles; the events' `to` holds
// slot 0): a classic name as the classic effect (its temporary entity's bytes read as a match's
// are; an explosion into `explosions`), any other by its name, for the files' effect of it.
void take_named(Listen& l, const sv::NetEvent& ev) {
  if ((ev.to & 1u) == 0) return;
  Effect e;
  e.code = -1;
  e.name = ev.text;
  if (ev.kind == sv::EventKind::trail_effect) {
    e.kind = EffectKind::named_trail;
    e.entity = ev.ent;
    e.at = ev.origin;
    e.end = {ev.values[0], ev.values[1], ev.values[2]};
    l.effects.push_back(std::move(e));
    return;
  }
  if (ev.kind != sv::EventKind::point_effect) return;
  const float n = ev.values[3];  // the times asked for, 1..255 (NaN: once)
  const int count = !(n >= 1) ? 1 : n > 255 ? 255 : static_cast<int>(n);
  if (uint8_t kind = 0; sv::classic_temp_entity(ev.text, kind)) {
    if (kind == kExplosion || kind == 4) {
      l.explosions.push_back(ev.origin);
      return;
    }
    std::vector<uint8_t> b = {kTempEntity, kind};
    if (kind == 2 || kind == 12) b.push_back(static_cast<uint8_t>(count));
    for (float c : {ev.origin.x, ev.origin.y, ev.origin.z})
      sv::encode(b, sv::WriteKind::coord, c, "");
    if (Effect classic; arena_effect(b, classic)) l.effects.push_back(classic);
    return;
  }
  e.kind = EffectKind::named;
  e.at = e.end = ev.origin;
  e.way = {ev.values[0], ev.values[1], ev.values[2]};
  e.count = std::min(count, 64);
  l.effects.push_back(std::move(e));
}

pg::QcSource source(const char* name, const unsigned char* bytes, std::size_t size) {
  pg::QcSource s;
  s.name = name;
  s.text.reserve(size);
  for (std::size_t i = 0; i < size; i++)
    if (bytes[i] != '\r') s.text.push_back(static_cast<char>(bytes[i]));  // a Windows checkout
  return s;
}

// The program's own state behind the rules (the reads the rules' world does not carry yet: a
// field by name, an entity's class, the program's statistics); null for another program form.
const sv::QcServer* program(const Listen& l) { return l.rules ? sv::qc_of(*l.rules) : nullptr; }

std::string text_field(const Listen& l, int e, int field) {
  const sv::QcServer* q = program(l);
  if (q == nullptr || field < 0) return "";
  const pg::Vm& vm = q->q.vm;
  const int32_t v = static_cast<int32_t>(*pg::entity_field(vm, e, field));
  const char* t = pg::vm_string(vm, v);
  return t == nullptr ? "" : t;
}

// A sound name's place on the server's sound list (0: not on it).
int sound_index(const sv::RulesCommon& s, const std::string& name) {
  for (size_t i = 1; i < s.sounds.size(); i++)
    if (s.sounds[i] == name) return static_cast<int>(i);
  return 0;
}

// A sound the rules started (sound, ambientsound) as the player's client would read it.
void take_sound(Listen& l, const sv::RulesCommon& s, const sv::NetEvent& ev) {
  if (ev.kind == sv::EventKind::ambient) {  // every client hears the level's loops
    DemoStaticSound st;
    st.origin = ev.origin;
    st.sound = sound_index(s, ev.text);
    st.volume = byte_volume(static_cast<int>(ev.values[0]));
    st.attenuation = byte_attenuation(static_cast<int>(ev.values[1]));
    l.static_sounds.push_back(st);
    return;
  }
  if (ev.kind != sv::EventKind::sound || (ev.to & 1u) == 0) return;
  if (l.sounds.size() >= kMostListenSounds) return;  // nobody plays them (a test, a stall)
  DemoSound d;
  d.entity = ev.ent;
  d.channel = ev.level;
  d.sound = sound_index(s, ev.text);
  d.volume = byte_volume(static_cast<int>(ev.values[0]));
  d.attenuation = ev.values[1];
  d.origin = ev.origin;
  l.sounds.push_back(d);
}

// What the client keeps of a map is new: the commands, the counts, the view the server set.
void entered(Listen& l) {
  l.sequence = 0;
  for (sh::MoveCmd& c : l.cmds) c = {};
  l.frames = 0;
  l.owed = 0;
  l.pending_map.clear();
  l.explosions.clear();
  l.blows.clear();
  l.effects.clear();
  l.entries++;
  l.sounds.clear();
  l.voices.clear();
  l.static_sounds.clear();  // the map's load sets its loops off before the first frame
  for (const sv::NetEvent& ev : l.server->out.events)
    if (ev.kind == sv::EventKind::ambient) take_sound(l, *l.server, ev);
  l.server->pending_map.clear();
  const sh::Edict& me = l.server->sv.edicts[1];
  l.view = me.v_angle;
  l.forced = true;
  l.entered_at = l.server->sv.time;
  l.campaign = CampaignView{};
  const sv::QcServer* q = program(l);
  const sv::CampaignStats st = q != nullptr ? sv::campaign_stats(*q) : sv::CampaignStats{};
  join_stats(l.campaign, st.total_secrets, st.total_monsters, st.found_secrets, st.killed_monsters);
}

// What the rules set off for the player in a frame (slot 0's prints and centre prints, the
// explosions and the match's effects, the blow he took), read before anything routes it. A
// match's prints come routed to him like any client's (ListenHost::print), so `prints` is false.
void take_events(Listen& l, const sv::RulesCommon& s, bool prints = true) {
  for (const sv::NetEvent& ev : s.out.events) {
    take_sound(l, s, ev);
    if (prints && ev.kind == sv::EventKind::print && (ev.to & 1u) != 0) l.prints.push_back(ev.text);
    if (ev.kind == sv::EventKind::centerprint && (ev.to & 1u) != 0) l.centres.push_back(ev.text);
    // The baseline mod's explosion: its temporary-entity message's first two bytes.
    if (ev.kind == sv::EventKind::multicast && ev.bytes.size() >= 2 && ev.bytes[0] == kTempEntity &&
        ev.bytes[1] == kExplosion)
      l.explosions.push_back(ev.origin);
    else if (Effect e;
             !l.campaign_host && ev.kind == sv::EventKind::multicast && arena_effect(ev.bytes, e))
      l.effects.push_back(e);
    else if (!l.campaign_host)
      take_named(l, ev);
  }
  if (const sv::ClientBlow& b = s.clients[0].blow; b.hit)
    l.blows.push_back({b.armor, b.blood, b.from});
}

// A voice the match relayed to the player (ListenHost::heard), kept for the window to play.
void host_heard(void* ctx, int talker, int gen, int seq, const std::vector<uint8_t>& data) {
  Listen& l = *static_cast<Listen*>(ctx);
  if (l.voices.size() >= kMostListenSounds) return;  // nobody plays them (a test, a stall)
  l.voices.push_back({talker, gen, seq, data});
}

// The match server's calls back into the listen (ListenHost::tap, ::print).
void host_tap(void* ctx, const sv::RulesCommon& s) {
  take_events(*static_cast<Listen*>(ctx), s, false);
}
void host_print(void* ctx, const std::string& text) {
  static_cast<Listen*>(ctx)->prints.push_back(text);
}

void close_door(Listen& l);

// The match server let go (its rules' views into the maps go before the maps do); a campaign's
// door closed.
void close_match(Listen& l) {
  close_door(l);
  l.rules = nullptr;
  l.server = nullptr;
  l.room = nullptr;
  l.match.reset();
  l.host.reset();
  if (l.port) ge::net::close_transport(*l.port);
  l.port.reset();
}

// The listen caught up with the match server: its rules, room and map now, and a map change it
// made (a lobby's restart into the match, the same map: `restarted`, and true).
bool catch_up(Listen& l) {
  ListenHost& h = *l.host;
  h.ctx = &l;
  l.rules = h.rules;
  l.server = h.rules != nullptr ? h.rules->common : nullptr;
  l.room = h.room;
  if (!h.asked.empty()) {
    l.pending_map = h.asked;
    h.asked.clear();
  }
  if (h.changes == l.changes) return false;
  l.changes = h.changes;
  const bool same = l.map && h.map && l.map->name == h.map->name;
  l.map = h.map;
  if (same) l.restarted = true;
  return same;
}

// A match: the engine's server opened on `opt.game` with the window's program and map, the player
// slot 0, on `opt.port` for others to join (none: a socket that is never read).
bool start_map(Listen& l, std::string& error) {
  close_match(l);
  if (l.opt.game == nullptr) {
    error =
        "no server game for the match; next: set ListenOptions::game to "
        "net/classic_game.hpp's classic_game()";
    return false;
  }
  auto h = std::make_unique<ListenHost>();
  h->progs = l.progs;
  h->map = l.map;
  h->files = l.opt.files;
  h->lines = l.opt.item_names;
  h->lines.insert(h->lines.end(), l.opt.arsenal.begin(), l.opt.arsenal.end());
  h->lines.insert(h->lines.end(), l.opt.console.begin(), l.opt.console.end());
  h->lobby = l.opt.lobby;
  h->lobby_settings = l.opt.lobby_settings;
  h->player = l.opt.player;
  h->speedjump = kListenSpeedJump;
  h->tap = host_tap;
  h->print = host_print;
  h->heard = host_heard;
  h->ctx = &l;
  ge::server::ServerConfig c;
  c.hostname = l.opt.hostname;
  c.map = l.map->name;
  c.seed = l.opt.seed;
  c.maxclients = l.opt.maxclients;
  c.lobby = l.opt.lobby ? 1 : 0;
  c.port = l.opt.port;
  if (l.opt.voice) c.rules.emplace("sv_voip", "1");
  c.host = h.get();
  auto port = std::make_unique<ge::net::Transport>();
  if (l.opt.port > 0 && l.opt.transport == nullptr) {
    const auto at = static_cast<uint16_t>(l.opt.port);
    if (!ge::net::open_udp_transport(*port, ge::net::ipv4(0, 0, 0, 0, at), &error)) {
      error = "the match's port " + std::to_string(l.opt.port) + " did not open: " + error +
              "; next: pick another port or 0 (nobody joins)";
      return false;
    }
  }
  auto server = std::make_unique<ge::server::Server>();
  l.host = std::move(h);
  l.port = std::move(port);
  ge::net::Transport* socket = l.opt.transport != nullptr ? l.opt.transport : l.port.get();
  if (!ge::server::server_open(*server, *l.opt.game, c, socket, nullptr, error)) {
    error = "the map " + l.map->name + " did not load: " + error +
            "; next: read the console's lines above";
    close_match(l);
    return false;
  }
  l.match = std::move(server);
  l.changes = l.host->changes;
  catch_up(l);
  entered(l);
  return true;
}

// The campaign host's map loader: the map read ahead when it is the one asked for, else read from
// the search path. The listen keeps it (the spec's models point into it) until the next call.
bool campaign_map(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  Listen& l = *static_cast<Listen*>(ctx);
  std::unique_ptr<LoadedMap> m = std::move(l.next);
  if (!m || m->name != name) {
    if (l.opt.files == nullptr) {
      error = "no search path to read maps/" + name + ".bsp from; next: set ListenOptions::files";
      return false;
    }
    m = load_named_map(*l.opt.files, name, error);
    if (!m) return false;
  }
  l.map = std::move(m);
  out = l.map->spec;
  l.map_changed = true;
  return true;
}

// The campaign's map now playing, as the listen's: the server, its sets, the player's arrival.
void campaign_entered(Listen& l) {
  l.rules = &l.campaign_host->server;
  l.server = l.rules->common;
  l.maps_started = l.campaign_host->history.size();
  if (l.map->from_file) sv::use_map_sets(*l.server, l.map->bsp);
  if (l.party) {
    std::string error;
    party_enter(*l.party, *l.campaign_host, l.map->spec, error);
    if (!error.empty()) l.prints.push_back(error);
  }
  entered(l);
}

bool coop_on(const Listen& l) {
  const ge::console::Cvar* v = l.server ? l.server->cvars.find("coop") : nullptr;
  return v != nullptr && v->on();
}

// A companion seated now (in a campaign in coop); false with the line printed when it cannot be.
bool seat_companion(Listen& l, sh::AimSkill skill) {
  if (!coop_on(l)) {
    l.prints.push_back(
        "addbot: coop is off; next: `coopgame` (a new coop game), or `coop 1` "
        "then `restart`");
    return false;
  }
  if (!l.party) {
    l.party = std::make_unique<Party>();
    l.party->seed = l.opt.seed;
  }
  std::string error;
  const int slot = party_add(*l.party, *l.campaign_host, l.map->spec, skill, error);
  if (slot < 0) {
    l.prints.push_back("addbot: " + error);
    return false;
  }
  const std::string& name = l.party->mates[static_cast<size_t>(slot)]->name;
  if (l.door != nullptr && !l.door_calls->seat(l.door, slot, name, true, error)) {
    party_remove(*l.party, *l.campaign_host);
    l.prints.push_back("addbot: " + error);
    return false;
  }
  l.prints.push_back(name + " (" + sh::aim_skill_name(skill) + ") joins the game");
  return true;
}

// The campaign host's options for the listen's settings (its loader the listen's).
sv::CampaignOptions campaign_options(Listen& l, const ListenOptions& opt) {
  sv::CampaignOptions co;
  co.server.seed = opt.seed;
  co.server.loop.max_edicts = 1024;
  co.server.loop.move.speedjump = kListenSpeedJump;
  co.load = campaign_map;
  co.load_ctx = &l;
  if (opt.files != nullptr) {
    co.server.model_bounds = &search_path_model_bounds;
    co.server.model_ctx = const_cast<SearchPath*>(opt.files);
    co.server.localized = localized_text(*opt.files);
  }
  co.registered = opt.registered;
  co.settings = opt.arsenal;
  co.settings.insert(co.settings.end(), opt.console.begin(), opt.console.end());
  return co;
}

// The program's client messages this frame (the all-clients stream, then the player's own), read
// and emptied: nothing else drains a listen server's reliable streams.
void read_streams(Listen& l, const sv::RulesCommon& s, bool clear = true) {
  const sv::MessageSink& out = s.out;
  const double now = s.sv.time;
  if (l.campaign_host) {
    read_campaign(l.campaign, out.all, now);
    read_campaign(l.campaign, out.one[0], now);
    const size_t first = l.effects.size();
    read_effects(l.campaign, out.unreliable, l.effects);
    // The particle bursts the program asked for (the server's events: blood, a big burst).
    for (const sv::NetEvent& ev : out.events) {
      if (ev.kind != sv::EventKind::particle || (ev.to & 1u) == 0) continue;
      Effect e;
      e.code = -1;
      e.kind = ev.text == "explosion" ? EffectKind::explosion : EffectKind::blood;
      e.at = e.end = ev.origin;
      // A burst's drops and its way as the wire would carry them: the count a byte, each axis of
      // the way in sixteenths from -8 to 7.94.
      e.count = std::clamp(static_cast<int>(ev.values[1]), 0, 255);
      for (int k = 0; k < 3; k++) {
        const float w = ev.dir[k];
        e.way[k] = std::clamp(std::trunc(w * 16), -128.f, 127.f) / 16;
      }
      l.effects.push_back(e);
    }
    for (size_t i = first; i < l.effects.size(); i++)
      if (l.effects[i].kind == EffectKind::explosion) l.explosions.push_back(l.effects[i].at);
    for (const sv::NetEvent& ev : out.events) take_named(l, ev);
  }
  if (!clear) return;  // the door routes them to the clients, then empties them
  sv::MessageSink& sink = l.server->out;
  sink.all.clear();
  for (auto& one : sink.one) one.clear();  // the companions' streams: nobody reads them
}

// The door's view of each frame, before it routes it: what the player sees, hears and reads.
void door_tap(void* ctx, const sv::RulesCommon& s) {
  Listen& l = *static_cast<Listen*>(ctx);
  take_events(l, s);
  read_streams(l, s, false);
}

void close_door(Listen& l) {
  if (l.door == nullptr) return;
  l.door_calls->close(l.door);
  l.door = nullptr;
}

// A coop campaign served to others: the door opened on the port, the player and the companions
// its local seats. A failure is printed and the game stays the window's alone.
void open_door(Listen& l) {
  const ListenOptions& o = l.opt;
  if (o.door == nullptr || (o.port <= 0 && o.transport == nullptr) || !coop_on(l)) return;
  std::string error;
  auto port = std::make_unique<ge::net::Transport>();
  if (o.transport == nullptr) {
    const auto at = static_cast<uint16_t>(o.port);
    if (!ge::net::open_udp_transport(*port, ge::net::ipv4(0, 0, 0, 0, at), &error)) {
      l.prints.push_back("coop: the port " + std::to_string(o.port) + " did not open: " + error +
                         "; next: pick another sv_port (0: nobody joins)");
      return;
    }
  }
  l.port = std::move(port);
  ge::net::Transport& t = o.transport != nullptr ? *o.transport : *l.port;
  l.door_calls = o.door;
  l.door =
      o.door->open(*l.campaign_host, t, o.hostname, o.seed, kPartyMost + 1, door_tap, &l, error);
  if (l.door == nullptr) {
    l.prints.push_back("coop: others cannot join: " + error);
    return;
  }
  l.net_s = 0;
  bool seated = l.door_calls->seat(l.door, 0, o.player, false, error);
  for (int i = 1; seated && l.party && i < sh::kMaxClients; i++)
    if (const auto& m = l.party->mates[static_cast<size_t>(i)])
      seated = l.door_calls->seat(l.door, i, m->name, true, error);
  if (!seated) {
    l.prints.push_back("coop: others cannot join: " + error);
    close_door(l);
    return;
  }
  l.prints.push_back("coop: others may join on port " + std::to_string(o.port) + "\n");
}

}  // namespace

bool baseline_progs(pg::Progs& out, std::string& error) {
  const pg::QcSource sources[] = {
      source("engine.qc", se_qc_arena_engine, se_qc_arena_engine_size),
      source("../common/defs.qc", se_qc_common_defs, se_qc_common_defs_size),
      source("glue.qc", se_qc_arena_glue, se_qc_arena_glue_size),
      source("../common/util.qc", se_qc_common_util, se_qc_common_util_size),
      source("../common/combat.qc", se_qc_common_combat, se_qc_common_combat_size),
      source("../common/weapons.qc", se_qc_common_weapons, se_qc_common_weapons_size),
      source("../common/items.qc", se_qc_common_items, se_qc_common_items_size),
      source("../common/dmm4.qc", se_qc_common_dmm4, se_qc_common_dmm4_size),
      source("../common/player.qc", se_qc_common_player, se_qc_common_player_size),
      source("../common/map.qc", se_qc_common_map, se_qc_common_map_size),
      source("world.qc", se_qc_arena_world, se_qc_arena_world_size),
  };
  pg::QcResult r = pg::compile_qc(sources);
  if (!r.ok) {
    error = "the baseline mod did not compile:";
    for (const pg::QcDiagnostic& d : r.diagnostics) error += "\n  " + pg::diagnostic_text(d);
    error += "\nnext: fix games/shooters_edge/qc at the lines named, then rebuild";
    return false;
  }
  out = std::move(r.progs);
  return true;
}

std::unique_ptr<LoadedMap> map_from_bytes(std::span<const uint8_t> bytes, const std::string& name,
                                          std::string& error) {
  auto m = std::make_unique<LoadedMap>();
  m->name = name;
  m->file.assign(bytes.begin(), bytes.end());
  if (!sv::read_bsp_map(m->file, name, &m->bsp, &m->spec, error)) return nullptr;
  m->from_file = true;
  return m;
}

std::unique_ptr<LoadedMap> load_named_map(const SearchPath& files, const std::string& name,
                                          std::string& error) {
  if (name == "arena") {
    auto m = std::make_unique<LoadedMap>();
    m->name = name;
    m->spec.name = name;
    m->spec.models = sv::arena_models(true);
    m->spec.entities = sv::kBaselineArenaEntities;
    return m;
  }
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, "maps/" + name + ".bsp", store);
  if (bytes.empty()) {
    error = "no map maps/" + name + ".bsp in " +
            (files.folder.empty() ? std::string("the search path") : files.folder) +
            " or its packs; next: check the name (`map arena` is built in) and -basedir";
    return nullptr;
  }
  return map_from_bytes(bytes, name, error);
}

bool listen_start(Listen& l, const pg::Progs& progs, std::unique_ptr<LoadedMap> map,
                  const ListenOptions& opt, std::string& error) {
  if (single_player(progs)) return listen_campaign(l, progs, std::move(map), opt, error);
  l.party.reset();
  close_match(l);  // a campaign's door first: it reads the campaign
  l.campaign_host.reset();
  l.progs = &progs;
  l.opt = opt;
  l.map = std::move(map);
  return start_map(l, error);
}

void listen_stop(Listen& l) {
  ListenOptions opt = std::move(l.opt);
  close_match(l);  // the rules' views into the maps go before the maps do
  l.campaign_host.reset();
  l.next.reset();
  l.map.reset();
  l = Listen{};
  l.opt = std::move(opt);
}

bool listen_change(Listen& l, std::unique_ptr<LoadedMap> map, std::string& error) {
  if (l.campaign_host) {
    const std::string name = map->name;
    l.next = std::move(map);
    if (l.door != nullptr) l.door_calls->unbind(l.door);
    if (!sv::campaign_change(*l.campaign_host, sv::MapChange::new_game, name, error)) {
      close_door(l);
      return false;
    }
    if (l.door != nullptr && !l.door_calls->rebind(l.door, error)) {
      l.prints.push_back("coop: others cannot join: " + error);
      close_door(l);
    }
    campaign_entered(l);
    l.map_changed = false;
    return true;
  }
  if (!l.match) {
    l.map = std::move(map);
    return start_map(l, error);
  }
  // The server moves onto the map with everyone on it (a new map: warmup again).
  if (!l.host->change(l.host->game, std::shared_ptr<LoadedMap>(std::move(map)), error))
    return false;
  catch_up(l);
  l.restarted = false;
  entered(l);
  return true;
}

bool listen_campaign(Listen& l, const pg::Progs& progs, std::unique_ptr<LoadedMap> map,
                     const ListenOptions& opt, std::string& error) {
  close_match(l);  // a campaign has no lobby
  l.campaign_host.reset();
  l.progs = &progs;
  l.opt = opt;
  l.failed.clear();
  const std::string name = map->name;
  l.next = std::move(map);
  sv::CampaignOptions co = campaign_options(l, opt);
  l.campaign_host = std::make_unique<sv::Campaign>();
  if (!sv::campaign_new_game(*l.campaign_host, progs, co, name, error)) {
    l.campaign_host.reset();
    return false;
  }
  sv::campaign_join(*l.campaign_host, 0, opt.player);
  l.party.reset();
  campaign_entered(l);
  for (int k = 0; k < opt.companions && k < kPartyMost; k++)
    if (!seat_companion(l, opt.companion_skill)) break;
  open_door(l);
  l.map_changed = false;
  return true;
}

std::string listen_save_problem(const Listen& l) {
  if (!l.campaign_host || l.server == nullptr)
    return "only a single-player game saves; next: start one (SINGLE PLAYER > NEW GAME)";
  if (l.campaign.end != LevelEnd::none) return "the level is over; next: save on the next map";
  return sv::campaign_save_problem(*l.campaign_host);
}

bool listen_save(const Listen& l, std::vector<uint8_t>& out, std::string& error,
                 int64_t saved_unix) {
  error = listen_save_problem(l);
  if (!error.empty()) {
    error = "save: " + error;
    return false;
  }
  return sv::campaign_save(*l.campaign_host, out, error, saved_unix);
}

namespace {

// The map a save names, read for its load and handed to the listen only once the load is good.
struct LoadAhead {
  const SearchPath* files = nullptr;
  std::unique_ptr<LoadedMap> map;
};

bool load_ahead(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  LoadAhead& a = *static_cast<LoadAhead*>(ctx);
  if (!a.map || a.map->name != name) {
    if (a.files == nullptr) {
      error = "no search path to read maps/" + name + ".bsp from; next: set ListenOptions::files";
      return false;
    }
    a.map = load_named_map(*a.files, name, error);
    if (!a.map) return false;
  }
  out = a.map->spec;
  return true;
}

}  // namespace

namespace {

// A saved game loaded by `load` (our save or the classic text) as the game now playing; the game
// before it is left playing when the load fails.
template <class Load>
bool load_game(Listen& l, const pg::Progs& progs, const ListenOptions& opt,
               std::unique_ptr<LoadedMap> map, Load load) {
  LoadAhead ahead{opt.files, std::move(map)};
  sv::CampaignOptions co = campaign_options(l, opt);
  co.load = load_ahead;
  co.load_ctx = &ahead;
  auto host = std::make_unique<sv::Campaign>();
  if (!load(*host, co)) return false;
  host->opt.load = campaign_map;
  host->opt.load_ctx = &l;
  close_match(l);
  l.campaign_host = std::move(host);
  l.progs = &progs;
  l.opt = opt;
  l.failed.clear();
  l.next.reset();
  l.map = std::move(ahead.map);
  l.party.reset();  // a save carries no companions (a coop game is never saved)
  campaign_entered(l);
  l.map_changed = true;  // the window draws the save's map
  // The player's commands go on from the saved sequence.
  l.sequence = l.server->sv.clients[0].last_sequence;
  return true;
}

}  // namespace

bool listen_load(Listen& l, const pg::Progs& progs, const ListenOptions& opt,
                 std::span<const uint8_t> bytes, std::string_view name, std::string& error,
                 std::unique_ptr<LoadedMap> map) {
  if (!load_game(l, progs, opt, std::move(map),
                 [&](sv::Campaign& c, const sv::CampaignOptions& co) {
                   return sv::campaign_load(c, progs, co, bytes, name, error);
                 }))
    return false;
  l.entered_at = l.server->sv.base_time;  // his level time from the map's start
  return true;
}

bool listen_save_text(const Listen& l, std::string& out, std::string& error) {
  error = listen_save_problem(l);
  if (!error.empty()) {
    error = "save: " + error;
    return false;
  }
  return sv::classic_save_text(*l.campaign_host, out, error);
}

bool listen_load_text(Listen& l, const pg::Progs& progs, const ListenOptions& opt,
                      std::string_view text, std::string_view name, std::string& error,
                      std::unique_ptr<LoadedMap> map) {
  if (!load_game(l, progs, opt, std::move(map),
                 [&](sv::Campaign& c, const sv::CampaignOptions& co) {
                   return sv::classic_load_text(c, progs, co, text, name, opt.player, error);
                 }))
    return false;
  // The text keeps no start: his level time from the settled start of the map.
  l.entered_at = sv::kSettleFrames * sv::kSettleSeconds + 1.0;
  return true;
}

bool search_path_model_bounds(void* ctx, const std::string& name, ge::shooter::Vec3f& mins,
                              ge::shooter::Vec3f& maxs) {
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes =
      find_file(*static_cast<const SearchPath*>(ctx), name, store);
  std::string why;
  return !bytes.empty() && sv::bsp_file_bounds(bytes, mins, maxs, why);
}

bool single_player(const pg::Progs& progs) {
  return progs.layout != nullptr && progs.layout->single_player;
}

bool wants_campaign(const ListenOptions& opt) {
  bool campaign = false;
  for (const std::string& line : opt.console) {
    const std::string_view s = line;
    constexpr std::string_view kName = "deathmatch ";
    if (s.substr(0, kName.size()) == kName) campaign = std::atof(line.c_str() + kName.size()) == 0;
  }
  return campaign;
}

std::string map_title(const Listen& l) {
  const std::string& t = l.server->world.map_title;
  return t.empty() ? l.map->name : t;
}

double listen_tic(const Listen& l) {
  return l.campaign_host ? sv::kCampaignStep : l.server->sv.set.mintic;
}

void listen_frame(Listen& l, sh::MoveCmd cmd) {
  cmd.msec = static_cast<uint8_t>(ge::core::num::round(listen_tic(l) * 1000));
  cmd.angles = l.view;
  // The easy jump: the aid strafes for him from the server's own player (speedjump_assist.hpp).
  if (l.server != nullptr && l.server->sv.clients[0].active && l.server->sv.clients[0].speedjump) {
    const sh::Edict& me = l.server->sv.edicts[1];
    sh::MoveState s;
    s.velocity = me.velocity;
    s.ground = (me.flags & sh::kFlagOnGround) != 0 ? 0 : -1;
    s.type = sh::move_type_of(me, false);
    s.waterlevel = me.waterlevel;
    // His own share when the server granted him one (`*speedjump`, net/classic_admin.hpp).
    const std::string granted = sv::info_value(l.server->clients[0].userinfo, "*speedjump");
    const float share = granted.empty() ? l.server->sv.set.move.speedjump
                                        : static_cast<float>(std::atof(granted.c_str()));
    cmd = sh::assisted_command(cmd, s, share, l.speedjump_view, l.jump_aid);
  } else {
    l.jump_aid = {};
  }
  if (l.impulse != 0) {
    cmd.impulse = static_cast<uint8_t>(l.impulse);
    l.impulse = 0;
  }
  l.cmds[0] = l.cmds[1];
  l.cmds[1] = l.cmds[2];
  l.cmds[2] = cmd;
  sh::Packet p;
  p.client = 0;
  p.sequence = ++l.sequence;
  for (int i = 0; i < 3; i++) p.cmds[i] = l.cmds[i];
  if (l.campaign_host) {
    const sh::Packet* sent = &p;
    int count = 1;
    if (l.party && party_size(*l.party) > 0) {  // the companions' packets after the player's
      std::vector<sh::Packet>& ps = l.party->packets;
      ps.clear();
      ps.push_back(p);
      party_packets(*l.party, *l.server, cmd.msec, ps);
      sent = ps.data();
      count = static_cast<int>(ps.size());
    }
    if (l.door != nullptr) {  // the endpoint's frame: the network's moves, then the local seats'
      l.net_s += sv::kCampaignStep;
      const auto now_us = static_cast<int64_t>(ge::core::num::round(l.net_s * 1e6));
      if (!l.door_calls->frame(l.door, sent, count, now_us, l.failed)) {
        close_door(l);
        l.rules = nullptr;
        l.server = nullptr;
        return;
      }
      if (l.campaign_host->history.size() != l.maps_started) campaign_entered(l);
    } else {
      if (!sv::campaign_frame(*l.campaign_host, sent, count, l.failed)) {
        l.rules = nullptr;
        l.server = nullptr;
        return;
      }
      if (l.campaign_host->history.size() != l.maps_started) campaign_entered(l);
      take_events(l, *l.server);
      read_streams(l, *l.server);
      sv::clear_frame(l.server->out);
    }
  } else {  // the match server's frame: his move first, then the bots' and the network's
    l.host->ctx = &l;
    l.host->move(l.host->game, p);
    ge::server::server_frame(*l.match);
    if (catch_up(l)) entered(l);  // the lobby's restart into the match: a new server
  }
  sv::RulesCommon& s = *l.server;
  l.frames++;
  if (l.campaign.turned) {
    l.view = {l.campaign.angles.x, l.campaign.angles.y, 0};
    l.campaign.turned = false;
  }
  // The mod's forced view (a spawn, a teleport) leaves the player's angles off the ones sent.
  const sh::Edict& me = s.sv.edicts[1];
  l.forced = s.clients[0].active && (me.v_angle.x != cmd.angles.x || me.v_angle.y != cmd.angles.y);
  if (l.forced) l.view = {me.v_angle.x, me.v_angle.y, 0};
}

bool listen_lobby(Listen& l, std::string_view line) {
  const std::string_view word = line.substr(0, line.find(' '));
  const bool bot_word = word == "addbot" || word == "removebot";
  if (bot_word && l.campaign_host && l.server != nullptr) {  // a campaign: the coop party
    if (word == "removebot") {
      int last = 0;
      for (int i = 1; l.party && i < sh::kMaxClients; i++)
        if (l.party->mates[static_cast<size_t>(i)]) last = i;
      if (!l.party || !party_remove(*l.party, *l.campaign_host))
        l.prints.push_back("removebot: no companion is seated");
      else if (l.door != nullptr)
        l.door_calls->unseat(l.door, last);
      return true;
    }
    const size_t sp = line.find(' ');
    const std::string arg = sp == std::string_view::npos ? "" : std::string(line.substr(sp + 1));
    sh::AimSkill skill = l.opt.companion_skill;
    if (!arg.empty() && !skill_named(arg, skill)) {
      l.prints.push_back("addbot: no skill " + arg + "; next: addbot novice|average|expert");
      return true;
    }
    seat_companion(l, skill);
    return true;
  }
  if (!l.room || l.server == nullptr) {
    if (bot_word || word == "ready" || word == "break" || word == "notready" ||
        word == "toggleready" || word == "team" || word == "start" || word == "lobby" ||
        word == "warmup" || word == "sitout" || word == "togglewarmup") {
      l.prints.push_back(std::string(word) +
                         ": no lobby runs; next: play a match (`map <name>` with deathmatch 1) "
                         "with `sv_lobby 1`");
      return true;
    }
    return false;
  }
  if (word == "addbot" && l.room->lobby.phase == sv::LobbyPhase::match) {
    l.prints.push_back("addbot: the match is on; next: add bots in the warmup");
    return true;
  }
  l.host->ctx = &l;
  const bool taken = l.host->words(l.host->game, std::string(line));
  catch_up(l);
  return taken;
}

bool listen_admin(Listen& l, const std::string& line, std::string& out) {
  if (!l.host || l.host->admin == nullptr || l.host->game == nullptr || l.match == nullptr)
    return false;
  return l.host->admin(l.host->game, line, out);
}

hud::LobbyView listen_lobby_view(const Listen& l) {
  if (!l.room || l.server == nullptr) return {};
  hud::LobbyView v = lobby_view(*l.room, l.server->sv.time, 0);
  v.map = l.map ? l.map->name : std::string();
  size_t k = 0;
  for (int i = 0; i < sv::kLobbySeats; i++) {
    if (!l.room->lobby.seats[static_cast<size_t>(i)].used) continue;
    if (i < sh::kMaxClients && l.server->clients[i].active)
      v.rows[k].frags = static_cast<int>(field_named(l, i + 1, "frags"));
    k++;
  }
  return v;
}

bool on_lobby_screen(const Listen& l) {
  return l.room && l.server != nullptr && !sv::lobby_on_map(l.room->lobby, 0);
}

bool listen_voice(const Listen& l) {
  return l.match && l.host && l.host->speak != nullptr && l.opt.voice;
}

void listen_mic(Listen& l, std::span<const float> samples, bool talking) {
  if (!listen_voice(l)) return;
  if (!l.voice_tried) {
    l.voice_tried = true;
    voice_out_open(l.voice);
  }
  std::vector<uint8_t> data;
  int seq = 0;
  if (voice_out_cut(l.voice, samples, talking, data, seq) > 0)
    l.host->speak(l.host->game, kVoiceOpus << 4, seq, data);
}

int listen_advance(Listen& l, double wall_s, const sh::MoveCmd& cmd) {
  const double tic = listen_tic(l);
  l.owed += wall_s;
  int n = 0;
  while (l.owed >= tic && n < kMostFrames && l.pending_map.empty() && l.server != nullptr) {
    l.owed -= tic;
    listen_frame(l, cmd);
    n++;
  }
  if (l.owed > tic) l.owed = 0;  // a stall: the time is dropped, never paid down
  return n;
}

PlayerState player_state(const Listen& l) {
  PlayerState p;
  const sh::Edict& me = l.server->sv.edicts[1];
  p.origin = me.origin;
  p.velocity = me.velocity;
  p.v_angle = me.v_angle;
  p.health = me.health;
  p.dead = me.health <= 0;
  p.on_ground = (me.flags & sh::kFlagOnGround) != 0;
  const sv::QcServer* q = program(l);
  if (q == nullptr) return p;
  const pg::Vm& vm = q->q.vm;
  const uint32_t* view_ofs = pg::entity_field(vm, 1, pg::vm_layout(vm).f.view_ofs);
  p.view_ofs = {pg::as_float(view_ofs[0]), pg::as_float(view_ofs[1]), pg::as_float(view_ofs[2])};
  p.punch = me.punchangle;
  if (const int f = pg::vm_layout(vm).f.idealpitch; f >= 0)
    p.ideal_pitch = pg::as_float(*pg::entity_field(vm, 1, f));
  return p;
}

void missiles_of(const Listen& l, std::vector<Missile>& out) {
  out.clear();
  const sv::QcServer* q = program(l);
  if (q == nullptr) return;
  const sv::RulesCommon& s = *l.server;
  for (int e = sh::kMaxClients + 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
    if (ed.free) continue;
    const std::string cn = text_field(l, e, q->q.layout->f.classname);
    if (cn != "rocket" && cn != "grenade" && cn != "spike" && cn != "missile") continue;
    out.push_back({e, ed.origin, cn != "spike", cn == "grenade"});
  }
}

bool model_shown(const Listen& l, int entity) {
  const sv::QcServer* q = program(l);
  if (q != nullptr) return sv::model_shown(*q, entity);
  return l.server != nullptr && l.server->sv.edicts[static_cast<size_t>(entity)].modelindex > 0;
}

float field_named(const Listen& l, int entity, const char* name) {
  const pg::Def* d = pg::find_field(*l.progs, name);
  if (d == nullptr) return 0;
  const sv::QcServer* q = program(l);
  if (q == nullptr) return 0;
  return pg::as_float(*pg::entity_field(q->q.vm, entity, d->offset));
}

void set_field_named(Listen& l, int entity, const char* name, float value) {
  const pg::Def* d = pg::find_field(*l.progs, name);
  if (d == nullptr || l.rules == nullptr) return;
  sv::QcServer* q = sv::qc_of(*l.rules);
  if (q == nullptr) return;
  *pg::entity_field(q->q.vm, entity, d->offset) = std::bit_cast<uint32_t>(value);
}

float global_named(const Listen& l, const char* name, bool* found) {
  if (found != nullptr) *found = false;
  const sv::QcServer* q = program(l);
  if (q == nullptr || l.progs == nullptr) return 0;
  const pg::Def* d = pg::find_global(*l.progs, name);
  if (d == nullptr) return 0;
  if (found != nullptr) *found = true;
  return pg::global_float(q->q.vm, d->offset);
}

}  // namespace shooters_edge::client
