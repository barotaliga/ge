// An existing game library of Quake II hosted unchanged (q2_native.hpp): the library loaded,
// the runner's calls, the level and the frame.
//
// ge:summary The hosted game library's runner: load, init, spawn, connect, think, frame, view
// ge:keywords game-library native-runner find-library load spawn-entities client-connect
// ge:keywords client-begin client-think run-frame event-clear baselines level-change m18v10
// ge:skill shooter mod
// ge:test tests/shooters_edge_q2_native_test.cpp
#include "shooters_edge/server/q2_native.hpp"

#include <algorithm>
#include <csetjmp>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <utility>

#include "brush/areas.hpp"
#include "net/address.hpp"
#include "shooters_edge/server/q2_campaign.hpp"
#include "shooters_edge/server/q2_native_host.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

namespace sh = ge::shooter;
namespace br = ge::brush;
using native::NativeEntityHead;

constexpr double kFrameSeconds = 0.1;
constexpr int kCsName = 0, kCsAirAccel = 29, kCsMaxClients = 30, kCsChecksum = 31, kCsModels = 32;
constexpr int kCsStatusBar = 5, kCsSounds = 288, kCsImages = 544, kCsItems = 1056;

int16_t coord(float v) { return static_cast<int16_t>(static_cast<int32_t>(v * 8.0f)); }
uint8_t angle8(float deg) {
  return static_cast<uint8_t>(static_cast<int32_t>(deg * 256.0f / 360.0f) & 0xFF);
}
int16_t angle16(float deg) {
  return static_cast<int16_t>(static_cast<int32_t>(deg * 65536.0f / 360.0f) & 0xFFFF);
}
int8_t quarter(float v) { return static_cast<int8_t>(std::clamp(v * 4.0f, -128.0f, 127.0f)); }

// A command as the library reads it: the angles back in 65536ths of a turn, nearest.
native::NativeCmd native_cmd(const sh::MoveCmd& c) {
  const auto angle = [](float degrees) {
    const double v = double(degrees) * (65536.0 / 360.0);
    return static_cast<int16_t>(static_cast<int32_t>(v < 0 ? v - 0.5 : v + 0.5));
  };
  native::NativeCmd n{};
  n.msec = c.msec;
  n.buttons = c.buttons;
  n.angles[0] = angle(c.angles.x);
  n.angles[1] = angle(c.angles.y);
  n.angles[2] = angle(c.angles.z);
  n.forward = c.forward;
  n.side = c.side;
  n.up = c.up;
  n.impulse = c.impulse;
  return n;
}

Q2EntityState wire_state(const native::NativeState& s) {
  Q2EntityState w;
  w.present = true;
  for (int k = 0; k < 3; k++) {
    w.origin[k] = coord(s.origin[k]);
    w.old_origin[k] = coord(s.old_origin[k]);
    w.angles[k] = angle8(s.angles[k]);
  }
  for (int k = 0; k < 4; k++) w.model[k] = s.model[k];
  w.frame = s.frame;
  w.skin = static_cast<uint32_t>(s.skin);
  w.effects = s.effects;
  w.renderfx = static_cast<uint32_t>(s.renderfx);
  w.solid = s.solid;
  w.sound = s.sound;
  w.event = s.event;
  return w;
}

Q2PlayerState wire_player(const native::NativePlayer& p) {
  Q2PlayerState w;
  w.type = static_cast<uint8_t>(p.move.type);
  for (int k = 0; k < 3; k++) {
    w.origin[k] = p.move.origin[k];
    w.velocity[k] = p.move.velocity[k];
    w.delta_angles[k] = p.move.delta_angles[k];
    w.view_offset[k] = quarter(p.view_offset[k]);
    w.view_angles[k] = angle16(p.view_angles[k]);
    w.kick[k] = quarter(p.kick_angles[k]);
    w.gun_offset[k] = quarter(p.gun_offset[k]);
    w.gun_angles[k] = quarter(p.gun_angles[k]);
  }
  w.time = p.move.time;
  w.flags = p.move.flags;
  w.gravity = p.move.gravity;
  w.gun_index = static_cast<uint8_t>(p.gun_index);
  w.gun_frame = static_cast<uint8_t>(p.gun_frame);
  for (int k = 0; k < 4; k++)
    w.blend[k] = static_cast<uint8_t>(std::clamp(p.blend[k], 0.0f, 1.0f) * 255.0f);
  w.fov = static_cast<uint8_t>(std::clamp(p.fov, 0.0f, 255.0f));
  w.rdflags = static_cast<uint8_t>(p.rdflags);
  for (int k = 0; k < 32; k++) w.stats[k] = p.stats[k];
  return w;
}

// THE WINDOW'S GAME reads what every runner shows (rules_world.hpp): the lists from the
// configstrings (a model's index one below the wire's, as Quake II's rules number them, and a
// player's own model, the wire's 255, the last), each entity's row and each player's numbers, and
// the QuakeWorld server's entities the same (`sv.edicts`: the angles, the place, the box).
int row_model(const NativeQ2& s, int wire) {
  if (wire == 255) return static_cast<int>(s.models.size()) - 1;
  return wire >= 2 && wire - 1 < static_cast<int>(s.models.size()) - 1 ? wire - 1 : 0;
}

void fill_lists(NativeQ2& s) {
  const std::vector<std::string>& cs = s.raw.config;
  s.models.assign(1, std::string());
  for (int m = 2; m < 255; m++) s.models.push_back(cs[size_t(kCsModels) + size_t(m)]);
  while (s.models.size() > 1 && s.models.back().empty()) s.models.pop_back();
  s.models.emplace_back(kQ2PlayerModel);
  s.sounds.assign(1, std::string());
  for (int k = 1; k < 256; k++) s.sounds.push_back(cs[size_t(kCsSounds) + size_t(k)]);
  while (s.sounds.size() > 1 && s.sounds.back().empty()) s.sounds.pop_back();
  RulesWorld& w = s.world;
  w.map_name = s.map_name;
  w.max_clients = s.host->max_clients;
  w.models = s.models;
  w.sounds = s.sounds;
  w.status_bar = cs[kCsStatusBar];
  w.images.assign(1, std::string());
  for (int k = 1; k < 256; k++) w.images.push_back(cs[size_t(kCsImages) + size_t(k)]);
  w.item_names.assign(1, std::string());
  for (int k = 1; k < 256; k++) w.item_names.push_back(cs[size_t(kCsItems) + size_t(k)]);
}

sh::Vec3f vec3(const float* v) { return {v[0], v[1], v[2]}; }

void fill_world(NativeQ2& s) {
  NativeHost& h = *s.host;
  fill_lists(s);
  RulesWorld& w = s.world;
  w.time = static_cast<double>(s.frame_number) * kFrameSeconds;
  w.paused = s.paused;
  const int count = std::min(h.game->num_edicts, static_cast<int>(w.entities.size()));
  w.entity_count = count;
  for (size_t n = 0; n < w.entities.size(); n++) {
    WorldEntity& row = w.entities[n];
    row = {};
    if (int(n) >= count || n == 0) continue;
    const NativeEntityHead* e = h.entity(static_cast<int>(n));
    if (e->inuse == 0 || (e->svflags & native::kSvfNoClient) != 0) continue;
    const bool player = n <= size_t(h.max_clients);
    if (player && !h.begun[n - 1]) continue;
    row.free = false;
    row.origin = vec3(e->s.origin);
    row.angles = vec3(e->s.angles);
    row.mins = vec3(e->mins);
    row.maxs = vec3(e->maxs);
    row.absmin = vec3(e->absmin);
    row.absmax = vec3(e->absmax);
    row.modelindex = row_model(s, e->s.model[0]);
    row.carried_model = row_model(s, e->s.model[1]);
    row.frame = e->s.frame;
    row.skin = player ? 0 : e->s.skin;
    row.effects = static_cast<int>(e->s.effects);
    row.render_fx = e->s.renderfx;
    row.loop = e->s.sound > 0 && size_t(e->s.sound) < s.sounds.size() ? e->s.sound : 0;
    if (!player || e->client == nullptr) continue;
    const native::NativePlayer& ps = e->client->ps;
    row.view_angles = vec3(ps.view_angles);
    row.velocity = {ps.move.velocity[0] * 0.125f, ps.move.velocity[1] * 0.125f,
                    ps.move.velocity[2] * 0.125f};
    row.flags = (ps.move.flags & 4) != 0 ? sh::kFlagOnGround : 0;  // the move's on-ground bit
    row.health = ps.stats[1];
  }
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClientInfo& c = s.clients[i];
    WorldClient& wc = w.clients[i];
    const bool in = i < h.max_clients && c.active && h.begun[i];
    wc.active = c.active;
    wc.spectator = c.spectator;
    wc.bot = c.bot;
    wc.userid = c.userid;
    wc.userinfo = c.userinfo;
    wc.name.clear();
    for (const auto& [k, v] : c.userinfo)
      if (k == "name") wc.name = v;
    wc.ping_ms = c.ping_ms;
    wc.entered_at = c.entered_at;
    wc.numbers = {};
    if (i < 32 && wc.layout != h.layout[i]) wc.layout = in ? h.layout[i] : std::string();
    if (i < 32 && wc.inventory != h.inventory[i])
      wc.inventory = in ? h.inventory[i] : std::vector<int16_t>();
    const NativeEntityHead* e = in ? h.entity(i + 1) : nullptr;
    if (e == nullptr || e->client == nullptr) continue;
    const native::NativePlayer& ps = e->client->ps;
    PlayerNumbers& nb = wc.numbers;
    nb.health = ps.stats[1];
    nb.current_ammo = ps.stats[3];
    nb.armour = ps.stats[5];
    nb.frags = ps.stats[14];
    for (int k = 0; k < 32; k++) nb.stats[k] = ps.stats[k];
    nb.weapon_model = row_model(s, ps.gun_index);
    nb.weapon_frame = static_cast<float>(ps.gun_frame);
    nb.view_z = ps.view_offset[2];
    nb.view_set = true;
    for (int k = 0; k < 3; k++) {
      nb.view_offset[k] = ps.view_offset[k];
      nb.kick[k] = ps.kick_angles[k];
    }
    nb.fov = ps.fov;
  }
  // The QuakeWorld server's entities, for what reads them (the window's view, the bots).
  if (s.sv.edicts.size() < w.entities.size()) return;
  s.sv.time = h.clock;
  s.sv.num_edicts = count;
  for (size_t i = 0; i < w.entities.size(); i++) {
    const WorldEntity& row = w.entities[i];
    sh::Edict& d = s.sv.edicts[i];
    d.free = row.free;
    d.origin = row.origin;
    d.velocity = row.velocity;
    d.angles = row.angles;
    d.v_angle = row.view_angles;
    d.mins = row.mins;
    d.maxs = row.maxs;
    d.size = row.maxs - row.mins;
    d.absmin = row.absmin;
    d.absmax = row.absmax;
    d.flags = row.flags;
    d.health = row.health;
    d.modelindex = row.modelindex;
  }
  if (s.sv.clients.size() < size_t(sh::kMaxClients)) s.sv.clients.resize(sh::kMaxClients);
  for (int i = 0; i < sh::kMaxClients; i++) {
    s.sv.edicts[size_t(i) + 1].view_ofs = {0, 0, w.clients[i].numbers.view_z};
    sh::ClientSlot& slot = s.sv.clients[size_t(i)];
    slot.active = s.clients[i].active;
    slot.spectator = s.clients[i].spectator;
    slot.bot = s.clients[i].bot;
    slot.last_sequence = i < 32 ? h.last_sequence[i] : 0;
  }
}

// The frame's view: every entity a client is sent, each player's state, then every event
// cleared (an event goes out in one frame).
void take_view(NativeQ2& s) {
  NativeHost& h = *s.host;
  Q2Raw& raw = s.raw;
  raw.entities.assign(kQ2Entities, Q2EntityState{});
  raw.owners.assign(kQ2Entities, 0);
  q2_sight_reset(raw.sight, h.map.get());
  raw.sight.groups = h.map && !h.map->areas.empty() && !native_no_areas(h) ? h.area_groups
                                                                           : std::vector<int32_t>{};
  const bool proper = (native_features(h) & native::kGmfProperInUse) != 0;
  const int count = std::min(h.game->num_edicts, kQ2Entities);
  for (int n = 1; n < count; n++) {
    NativeEntityHead* e = h.entity(n);
    if ((e->svflags & native::kSvfNoClient) != 0) continue;
    if (proper && e->inuse == 0) continue;  // the game keeps it right: one not in use is gone
    if (e->s.model[0] == 0 && e->s.effects == 0 && e->s.sound == 0 && e->s.event == 0) continue;
    if (n <= h.max_clients && !h.begun[n - 1]) continue;
    e->s.number = n;
    raw.entities[size_t(n)] = wire_state(e->s);
    if (e->linkcount > 0)
      q2_sight_link(raw.sight, n, e->absmin, e->absmax);
    else
      q2_sight_link(raw.sight, n, e->s.origin, e->s.origin);
    raw.owners[size_t(n)] = static_cast<int16_t>(std::max(0, h.number_of(e->owner)));
  }
  for (int i = 0; i < h.max_clients && i < 32; i++) {
    const NativeEntityHead* e = h.entity(i + 1);
    raw.players[i] = e->client != nullptr ? wire_player(e->client->ps) : Q2PlayerState{};
  }
  // The events last one frame: the game clears them itself when it has its own end of frame.
  if (h.ex != nullptr && h.ex->prep_frame != nullptr)
    guarded(h, [&] { h.ex->prep_frame(); });
  else
    for (int n = 0; n < h.game->num_edicts; n++) h.entity(n)->s.event = 0;
  for (int i = 0; i < h.max_clients && i < sh::kMaxClients; i++) {
    const NativeEntityHead* e = h.entity(i + 1);
    if (e->client == nullptr) continue;
    s.world.clients[i].numbers.frags = e->client->ps.stats[14];  // the score's stat
  }
  fill_world(s);
}

// A game's per-client calls (the extended interface at version 3), as each slot's frame is
// written: whether the slot sees entity `n`, then the state it is to see in place of the real one.
bool per_client(void* ctx, int slot, int n, Q2EntityState& st) {
  NativeHost& h = *static_cast<NativeQ2*>(ctx)->host;
  if (h.dead || h.ex == nullptr || slot >= h.max_clients) return true;
  NativeEntityHead* viewer = h.entity(slot + 1);
  NativeEntityHead* e = h.entity(n);
  int keep = 1;
  if (h.ex->entity_visible_to_client != nullptr &&
      !guarded(h, [&] { keep = h.ex->entity_visible_to_client(viewer, e); }))
    return true;
  if (keep == 0) return false;
  if (h.ex->customize_entity_to_client == nullptr) return true;
  native::NativeCustomize rec{};
  int yes = 0;
  if (!guarded(h, [&] { yes = h.ex->customize_entity_to_client(viewer, e, &rec); }) || yes == 0)
    return true;
  if (rec.s.number != n) {  // a record of another entity: the real one is sent
    h.s->console.push_back("the game customized entity " + std::to_string(n) +
                           " for a client with entity " + std::to_string(rec.s.number) +
                           "'s state");
    return true;
  }
  st = wire_state(rec.s);
  return true;
}

// The level's baselines, once its entities have settled.
void take_baselines(NativeQ2& s) {
  NativeHost& h = *s.host;
  s.raw.baselines.assign(kQ2Entities, Q2EntityState{});
  const int count = std::min(h.game->num_edicts, kQ2Entities);
  for (int n = 1; n < count; n++) {
    NativeEntityHead* e = h.entity(n);
    if (e->inuse == 0) continue;
    if (e->s.model[0] == 0 && e->s.sound == 0 && e->s.effects == 0) continue;
    e->s.number = n;
    std::memcpy(e->s.old_origin, e->s.origin, sizeof e->s.old_origin);
    s.raw.baselines[size_t(n)] = wire_state(e->s);
    s.raw.baselines[size_t(n)].event = 0;
  }
}

std::string quotes_off(std::string w) {
  if (w.size() >= 2 && w.front() == '"' && w.back() == '"') w = w.substr(1, w.size() - 2);
  return w;
}

// The map a level string names (`eou1_.cin+*base2$start`: base2), or the text itself when it
// names none.
std::string bare_map(const std::string& level) {
  const LevelString ls = level_string(level);
  return ls.map.empty() ? level : ls.map;
}

// A level change asked for by `level` (its quotes off): the map to load and the whole string.
void ask_level(NativeQ2& s, const std::string& word, MapChange change) {
  s.host->level = quotes_off(word);
  s.pending_map = bare_map(s.host->level);
  s.pending_change = change;
}

// The text the library queued: a level change asked for, else a setting or a line kept.
void run_commands(NativeQ2& s) {
  NativeHost& h = *s.host;
  std::vector<std::string> lines;
  for (const std::string& text : h.commands) {
    size_t at = 0;
    while (at < text.size()) {
      size_t end = text.find_first_of("\n;", at);
      if (end == std::string::npos) end = text.size();
      lines.push_back(text.substr(at, end - at));
      at = end + 1;
    }
  }
  h.commands.clear();
  for (const std::string& line : lines) {
    native_split(h, line);
    if (h.argv.empty()) continue;
    const std::string& w = h.argv[0];
    if ((w == "gamemap" || w == "map") && h.argv.size() > 1) {
      ask_level(s, h.argv[1], w == "map" ? MapChange::new_game : MapChange::change_level);
    } else if (w == "menu_loadgame") {  // the dead of single player: the server's own player's menu
      NetEvent& ev = s.out.events.emplace_back();
      ev.kind = EventKind::stuffcmd;
      ev.to = 1u;
      ev.reliable = true;
      ev.time = h.clock;
      ev.text = "menu_loadgame\n";
      s.out.events_total++;
    } else if (h.argv.size() > 1 && h.find_setting(w) != nullptr) {
      native_set(h, *h.find_setting(w), h.argv[1], false);
    } else {
      s.console.push_back("the game's line, not run here: " + line);
    }
  }
}

void run_frame(NativeQ2& s) {
  NativeHost& h = *s.host;
  for (int i = 0; i < h.max_clients; i++) {
    NativeEntityHead* e = h.entity(i + 1);
    if (h.begun[i] && e->client != nullptr) e->client->ping = s.clients[i].ping_ms;  // measured
  }
  if (!guarded(h, [&] { h.game->run_frame(); })) return;
  run_commands(s);
  take_view(s);
  s.frame_number++;
}

// An address an IPv6-blind game can read: an IPv6 client's as 198.51.100.X:PORT, X the bytes of its
// /48 prefix folded by exclusive or.
std::string ipv4_for_game(const std::string& address) {
  ge::net::Address a;
  if (!ge::net::parse_address(address, a) || a.family != ge::net::Family::ipv6) return address;
  uint8_t x = 0;
  for (int k = 0; k < 6; k++) x ^= a.bytes[k];
  return "198.51.100." + std::to_string(x) + ":" + std::to_string(a.port);
}

// The user info a game's connect reads; with the extra user info (`wants` it), the connection's
// own keys apart in `extra` (challenge, ip, major, minor, netchan, packetlen, qport, zlib) and no
// `ip` added to the user info.
std::string userinfo_text(const NativeQ2& s, int slot, const std::string& name, int32_t wants,
                          std::string& extra) {
  std::string out;
  bool named = false;
  for (const auto& [k, v] : s.clients[slot].userinfo) {
    out += '\\';
    out += k;
    out += '\\';
    out += v;
    named = named || k == "name";
  }
  if (!named) out += "\\name\\" + name;
  // The keys a client of Quake II always sends, for a player whose client sent fewer (the
  // window's own): a game may read a missing one as set (a spectator).
  static constexpr const char* kDefaults[][2] = {{"skin", "male/grunt"}, {"hand", "0"},
                                                 {"msg", "1"},           {"rate", "25000"},
                                                 {"fov", "90"},          {"spectator", "0"}};
  for (const auto& [k, v] : kDefaults)
    if (out.find("\\" + std::string(k) + "\\") == std::string::npos)
      out.append("\\").append(k).append("\\").append(v);
  const std::string& address = s.clients[slot].address;
  const std::string ip = (wants & native::kGmfIpv6) != 0 ? address : ipv4_for_game(address);
  extra.clear();
  if ((wants & native::kGmfExtraUserInfo) != 0) {
    extra = "\\challenge\\0\\ip\\" + ip +
            "\\major\\34\\minor\\0\\netchan\\1\\packetlen\\1390\\qport\\0\\zlib\\0";
    return out;
  }
  if (out.find("\\ip\\") == std::string::npos) out += "\\ip\\" + ip;
  return out;
}

// A frame with nobody in the level (a level entered again runs on alone first).
void frame_alone(NativeQ2& s) {
  if (guarded(*s.host, [&] { s.host->game->run_frame(); })) s.frame_number++;
}

// --- the runner's calls ---

bool nv_start(void* r, const MapSpec& spec, std::string& error) {
  NativeQ2& s = *static_cast<NativeQ2*>(r);
  NativeHost& h = *s.host;
  if (h.dead) {
    error = "the game library stopped (" + h.error_text + "); start the server again";
    return false;
  }
  if (!spec.brushes && spec.models.empty()) {
    error = "maps/" + spec.name + ".bsp is not a map the game library can run (no world model)";
    return false;
  }
  // The level left kept or forgotten, a new game's settings, or a load's game read, before the
  // old level goes (q2_native_save.cpp).
  if (!h.load.empty()) {
    if (!native_load_begin(s, error)) return false;
  } else {
    if (bare_map(h.level) != spec.name) h.level = spec.name;  // a change the host made itself
    native_leave(s, s.pending_change);
    if (s.pending_change == MapChange::new_game && !native_new_game(s, error)) return false;
  }
  const LevelString level = level_string(h.level);
  h.tracer.reset();
  h.map.reset();
  h.portals.clear();
  h.area_groups.clear();
  h.hulls.clear();
  h.surfaces.clear();
  if (spec.brushes) {
    h.map = std::make_shared<br::Map>(*spec.brushes);
    br::build_phs(h.map.get());
    h.portals.assign(1024, 0);  // every portal shut
    br::flood_areas(*h.map, h.portals, &h.area_groups);
    h.tracer = std::make_unique<br::BrushTracer>(*h.map);
    h.surfaces.assign(h.map->textures.size(), native::NativeSurface{});
    for (size_t i = 0; i < h.surfaces.size(); i++)
      std::strncpy(h.surfaces[i].name, h.map->textures[i].name.c_str(), 15);
  } else {
    h.hulls = spec.models;  // a Quake map: its clip hulls (q2_native_host.hpp)
  }
  h.linked.assign(size_t(std::max(h.game->max_edicts, kQ2Entities)), 0);
  s.world.entities.assign(size_t(std::max(1, std::min(h.game->max_edicts, kQ2Entities))),
                          WorldEntity{});
  s.sv.set.max_edicts = std::max(s.sv.set.max_edicts, int(s.world.entities.size()) + 8);
  s.sv.edicts.assign(size_t(s.sv.set.max_edicts), sh::Edict{});
  s.map_name = spec.name;
  s.map_stopped = false;
  s.pending_map.clear();
  Q2Raw& raw = s.raw;
  raw.config.assign(kQ2ConfigStrings, std::string());
  for (int i = 0; i < 32; i++) {
    raw.reliable[i].clear();
    raw.datagram[i].clear();
  }
  raw.config[kCsName] = spec.name;
  const bool dm =
      h.find_setting("deathmatch") != nullptr && h.find_setting("deathmatch")->rec.value != 0;
  const NativeSetting* air = h.find_setting("sv_airaccelerate");
  h.air_accelerate = dm && air != nullptr ? air->rec.value : 0;
  raw.config[kCsAirAccel] = std::to_string(static_cast<int>(h.air_accelerate));
  raw.config[kCsMaxClients] = std::to_string(h.max_clients);
  raw.config[kCsChecksum] = std::to_string(s.map_checksum);
  raw.config[kCsModels + 1] = "maps/" + spec.name + ".bsp";
  for (size_t m = 1; m < h.model_count() && m + 1 < 256; m++)
    raw.config[size_t(kCsModels) + m + 1] = "*" + std::to_string(m);
  for (int i = 0; i < h.max_clients; i++) h.entity(i + 1)->s.number = i + 1;
  h.clock = 0;
  h.next_frame = kFrameSeconds;
  h.loading = true;
  std::string text = spec.brushes ? spec.brushes->entity_text : spec.entities;
  const std::string name = spec.name;
  const bool spawned =
      guarded(h, [&] { h.game->spawn_entities(name.c_str(), text.data(), level.spawn.c_str()); }) &&
      guarded(h, [&] { h.game->run_frame(); }) && guarded(h, [&] { h.game->run_frame(); });
  h.loading = false;
  if (!spawned) {
    error = s.stop_reason;
    return false;
  }
  if (raw.config[kCsChecksum] != std::to_string(s.map_checksum)) {
    error = "the game library changed the map's checksum (configstring 31): it cannot be hosted";
    h.dead = true;
    return false;
  }
  run_commands(s);
  s.pending_map.clear();
  take_baselines(s);
  take_view(s);
  native_set(h, native_setting(h, "mapname", "", native::kCvarNoSet), spec.name, true);
  // A level kept or loaded read back over the fresh one, then the autosave.
  if (!native_enter(s, error, &frame_alone)) return false;
  take_view(s);
  s.pending_change = MapChange::new_game;
  return true;
}

void nv_connect(void* r, int slot, const std::string& name, bool, bool, const float*) {
  NativeQ2& s = *static_cast<NativeQ2*>(r);
  NativeHost& h = *s.host;
  if (slot < 0 || slot >= h.max_clients) return;
  NativeEntityHead* e = h.entity(slot + 1);
  s.clients[slot].active = true;
  if (h.carried[slot]) {
    h.carried[slot] = false;
    h.begun[slot] = true;  // in the game as its begin runs: the game's prints then reach him
    guarded(h, [&] { h.game->client_begin(e); });
    return;
  }
  const int32_t wants = native_features(h);
  std::string extra;
  const std::string info = userinfo_text(s, slot, name, wants, extra);
  char buf[1024] = {};  // the user info, then the extra one after its zero
  std::strncpy(buf, info.c_str(), 511);
  std::strncpy(buf + std::strlen(buf) + 1, extra.c_str(), 511);
  int yes = 0;
  if (!guarded(h, [&] { yes = h.game->client_connect(e, buf); })) return;
  if (yes == 0) {
    std::string why;
    const char* rej = std::strstr(buf, "\\rejmsg\\");
    if (rej != nullptr) why = std::string(rej + 8).substr(0, std::string(rej + 8).find('\\'));
    native_print(h, slot, 2,
                 why.empty() ? "Connection refused.\n" : why + "\nConnection refused.\n");
    // Stuff text (11) and its line with the ending zero: the client leaves.
    static constexpr uint8_t kLeave[] = {11,  'd', 'i', 's', 'c',  'o', 'n',
                                         'n', 'e', 'c', 't', '\n', 0};
    s.raw.reliable[slot].insert(s.raw.reliable[slot].end(), std::begin(kLeave), std::end(kLeave));
    s.clients[slot].active = false;
    return;
  }
  h.connected[slot] = true;
  if (!guarded(h, [&] { h.game->client_userinfo_changed(e, buf); })) return;
  h.begun[slot] = true;
  guarded(h, [&] { h.game->client_begin(e); });
}

void nv_drop(void* r, int slot) {
  NativeQ2& s = *static_cast<NativeQ2*>(r);
  NativeHost& h = *s.host;
  if (slot < 0 || slot >= h.max_clients) return;
  s.clients[slot].active = false;
  if (h.carried[slot]) return;  // signing on again after a level change: still in the game
  const bool told = h.begun[slot] ||
                    (h.connected[slot] && (native_features(h) & native::kGmfAllDisconnects) != 0);
  if (told) guarded(h, [&] { h.game->client_disconnect(h.entity(slot + 1)); });
  h.begun[slot] = h.connected[slot] = false;
  s.raw.reliable[slot].clear();
  s.raw.datagram[slot].clear();
  if (slot < 32) {
    h.layout[slot].clear();
    h.inventory[slot].clear();
  }
}

void nv_frame(void* r, const sh::Packet* packets, int count) {
  NativeQ2& s = *static_cast<NativeQ2*>(r);
  NativeHost& h = *s.host;
  if (h.dead) return;
  for (int i = 0; i < count; i++) {
    const sh::Packet& p = packets[i];
    if (p.client < 0 || p.client >= h.max_clients || !h.begun[p.client]) continue;
    NativeEntityHead* e = h.entity(p.client + 1);
    const uint32_t gap = p.sequence - h.last_sequence[p.client];
    h.last_sequence[p.client] = p.sequence;
    for (int k = 0; k < 3; k++) {
      if ((k == 0 && gap < 3) || (k == 1 && gap < 2)) continue;  // only what a loss held back
      native::NativeCmd c = native_cmd(p.cmds[k]);
      if (!guarded(h, [&] { h.game->client_think(e, &c); })) return;
    }
  }
  run_commands(s);
  h.clock += s.sv.set.mintic > 0 ? s.sv.set.mintic : 0.013;
  if (h.clock + 1e-9 < h.next_frame) return;
  h.next_frame += kFrameSeconds;
  if (h.next_frame < h.clock) h.next_frame = h.clock + kFrameSeconds;  // fallen behind: catch up
  run_frame(s);
}

CommandResult nv_client_command(void* r, int slot, const std::string& line) {
  NativeQ2& s = *static_cast<NativeQ2*>(r);
  NativeHost& h = *s.host;
  if (slot < 0 || slot >= h.max_clients || !h.begun[slot]) return CommandResult::bad;
  native_split(h, line);
  if (h.argv.empty()) return CommandResult::bad;
  guarded(h, [&] { h.game->client_command(h.entity(slot + 1)); });
  run_commands(s);
  return CommandResult::mod;
}

bool nv_console(void* r, const std::string& line) {
  NativeQ2& s = *static_cast<NativeQ2*>(r);
  NativeHost& h = *s.host;
  native_split(h, line);
  if (h.argv.empty()) return false;
  const std::string w = h.argv[0];
  if (w == "sv") {
    guarded(h, [&] { h.game->server_command(); });
    run_commands(s);
    return true;
  }
  if ((w == "map" || w == "gamemap" || w == "changelevel") && h.argv.size() > 1) {
    ask_level(s, h.argv[1], w == "map" ? MapChange::new_game : MapChange::change_level);
    return true;
  }
  if (w == "restart") {
    s.pending_map = s.map_name;
    s.pending_change = MapChange::restart;
    return true;
  }
  if (w == "set" && h.argv.size() > 2) {  // `set <name> <value>`: made when the game has none
    const std::string name = h.argv[1], value = quotes_off(h.argv[2]);
    native_set(h, native_setting(h, name, value, 0), value, false);
    return true;
  }
  if (h.argv.size() > 1) {
    native_set(h, native_setting(h, w, h.argv[1], 0), h.argv[1], false);
    return true;
  }
  return h.find_setting(w) != nullptr;
}

void nv_user_info(void* r, int slot, const std::string& key, const std::string& value) {
  NativeQ2& s = *static_cast<NativeQ2*>(r);
  NativeHost& h = *s.host;
  if (slot < 0 || slot >= h.max_clients) return;
  auto& info = s.clients[slot].userinfo;
  bool found = false;
  for (auto& [k, v] : info)
    if (k == key) {
      v = value;
      found = true;
    }
  if (!found) info.emplace_back(key, value);
  if (!h.connected[slot]) return;
  std::string extra;
  const std::string text = userinfo_text(s, slot, value, native_features(h), extra);
  char buf[512] = {};
  std::strncpy(buf, text.c_str(), sizeof buf - 1);
  guarded(h, [&] { h.game->client_userinfo_changed(h.entity(slot + 1), buf); });
}

bool nv_pause(void*, int, bool) { return false; }

void nv_carry(void* r) {
  NativeQ2& s = *static_cast<NativeQ2*>(r);
  NativeHost& h = *s.host;
  for (int i = 0; i < h.max_clients; i++) h.carried[i] = h.begun[i];
}

void nv_destroy(void* r) { delete static_cast<NativeQ2*>(r); }

std::string nv_save_problem(const void* r) {
  return native_save_problem(*static_cast<const NativeQ2*>(r));
}

bool nv_save(const void* r, std::vector<uint8_t>& out, std::string& error, int64_t saved_unix) {
  // The game writes its files as it saves: the runner's state is the game's, not ours.
  return native_save(*const_cast<NativeQ2*>(static_cast<const NativeQ2*>(r)), out, error,
                     saved_unix);
}

std::vector<uint8_t> nv_take_autosave(void* r) {
  return std::exchange(static_cast<NativeQ2*>(r)->host->autosave, std::vector<uint8_t>());
}

uint32_t nv_last_sequence(const void* r, int slot) {
  const NativeHost& h = *static_cast<const NativeQ2*>(r)->host;
  return slot >= 0 && slot < 32 ? h.last_sequence[slot] : 0;
}

constexpr RunnerCalls kNativeCalls = {"native",
                                      &nv_start,
                                      &nv_connect,
                                      &nv_drop,
                                      &nv_frame,
                                      &nv_client_command,
                                      &nv_console,
                                      &nv_user_info,
                                      &nv_pause,
                                      &nv_carry,
                                      &nv_destroy,
                                      false,
                                      nullptr,
                                      false,
                                      false,
                                      nullptr,
                                      &nv_save_problem,
                                      &nv_save,
                                      &nv_take_autosave,
                                      &nv_last_sequence};

}  // namespace

NativeQ2::NativeQ2() = default;

NativeQ2::~NativeQ2() {
  if (!host) return;
  NativeHost& h = *host;
  if (!h.dead) guarded(h, [&] { h.game->shutdown(); });
  native_make_current(nullptr);
  native_forget(h);
  for (auto& [p, tag] : h.blocks) std::free(p);
  h.blocks.clear();
  native_close_files(h);
  h.library.close();
}

bool find_native_library(const std::filesystem::path& root, const std::string& game,
                         const std::string& base, std::filesystem::path& out) {
#if defined(_WIN32)
  static const char* const kNames[] = {"game.dll", "gamex86.dll"};
#elif defined(__APPLE__)
  static const char* const kNames[] = {"game.dylib"};
#else
  static const char* const kNames[] = {"game.so", "gamex86_64.so", "gamei386.so"};
#endif
  std::vector<std::string> folders;
  if (!game.empty() && game != base) folders.push_back(game);
  folders.push_back(base);
  std::error_code ec;
  for (const std::string& f : folders)
    for (const char* name : kNames) {
      const std::filesystem::path p = root / f / name;
      if (std::filesystem::is_regular_file(p, ec)) {
        out = p;
        return true;
      }
    }
  return false;
}

std::unique_ptr<NativeQ2> load_native_q2(const NativeQ2Options& opt, std::string& error) {
  auto s = std::make_unique<NativeQ2>();
  s->host = std::make_unique<NativeHost>();
  NativeHost& h = *s->host;
  h.s = s.get();
  h.max_clients = std::clamp(opt.max_clients, 1, sh::kMaxClients);
  s->sv.set.maxclients = h.max_clients;
  const std::string file = opt.library.string();
  std::string why;
  if (!ge::mod::open_library(opt.library, h.library, why)) {
    error = "the game library " + file + " did not load (" + why +
            "); it must be built for this platform";
    return nullptr;
  }
  auto entry = reinterpret_cast<native::NativeEntryFn>(h.library.symbol(native::kNativeEntry));
  if (entry == nullptr) {
    error = "the game library " + file + " has no entry point " + native::kNativeEntry +
            ": it is not a game library of Quake II";
    return nullptr;
  }
  native_fill_imports(h.imports);
  // The server's own settings, so the game finds them with the server's values.
  const std::pair<const char*, const char*> own[] = {
      {"dedicated", "1"},      {"cheats", "0"},
      {"deathmatch", "1"},     {"coop", "0"},
      {"singleplayer", "0"},   {"skill", "1"},
      {"dmflags", "16"},       {"fraglimit", "0"},
      {"timelimit", "0"},      {"hostname", "Shooter's Edge"},
      {"protocol", "34"},      {"timeout", "125"},
      {"zombietime", "2"},     {"sv_airaccelerate", "0"},
      {"sv_enforcetime", "0"}, {"paused", "0"},
      {"gamedir", ""},         {"mapname", ""},
      {"sv_noreload", "0"}};
  for (const auto& [name, value] : own) native_setting(h, name, value, 0);
  native_setting(h, "maxclients", std::to_string(h.max_clients), native::kCvarServerInfo);
  native_setting(h, "game", opt.game, native::kCvarServerInfo | native::kCvarLatch);
  native_setting(h, "sv_features", std::to_string(kNativeServerFeatures), native::kCvarNoSet);
  native_setting(h, "g_features", "0", native::kCvarNoSet);
  native_setting(h, "map_noareas", "0", 0);  // the engine's: every area joined while set
  // The files the extended interface reads: the library's folder, then the base folder beside it.
  h.write_dir = opt.library.parent_path();
  h.base_dir = h.write_dir.parent_path() / "baseq2";
  h.folders = {h.write_dir};
  if (h.base_dir != h.write_dir) h.folders.push_back(h.base_dir);
  for (const auto& [name, value] : opt.settings)
    native_set(h, native_setting(h, name, value, 0), value, true);
  native_make_current(&h);
  h.game = entry(&h.imports);
  if (h.game == nullptr || h.game->apiversion != native::kNativeVersion) {
    error = "the game library " + file + " is version " +
            (h.game != nullptr ? std::to_string(h.game->apiversion) : std::string("unknown")) +
            ", not " + std::to_string(native::kNativeVersion) + ": it is not one this server hosts";
    h.game = nullptr;
    return nullptr;
  }
  // The extended interface, when the library has one, before its start.
  if (auto entry_ex =
          reinterpret_cast<native::NativeEntryExFn>(h.library.symbol(native::kNativeEntryEx))) {
    native_fill_imports_ex(h.imports_ex);
    const native::NativeExportsEx* ex = entry_ex(&h.imports_ex);
    h.ex = ex != nullptr && ex->apiversion >= 1 ? ex : nullptr;
  }
  if (!guarded(h, [&] { h.game->init(); })) {
    error = "the game library " + file + " stopped in its start: " + h.error_text;
    return nullptr;
  }
  if (h.ex != nullptr && h.ex->apiversion >= 3 &&
      (h.ex->entity_visible_to_client != nullptr || h.ex->customize_entity_to_client != nullptr)) {
    s->raw.touch = per_client;
    s->raw.touch_ctx = s.get();
  }
  if (h.game->edicts == nullptr || h.game->edict_size < int32_t(sizeof(NativeEntityHead)) ||
      h.game->num_edicts <= h.max_clients) {
    error = "the game library " + file + " made no entities for " + std::to_string(h.max_clients) +
            " players";
    h.dead = true;
    return nullptr;
  }
  return s;
}

uint64_t native_q2_hash(const NativeQ2& s) {
  uint64_t h = 1469598103934665603ull;
  const auto mix = [&h](const void* p, size_t n) {
    const auto* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 1099511628211ull;
  };
  mix(&s.frame_number, sizeof s.frame_number);
  for (const Q2EntityState& e : s.raw.entities) {
    mix(e.origin, sizeof e.origin);
    mix(e.angles, sizeof e.angles);
    mix(&e.frame, sizeof e.frame);
    mix(e.model, sizeof e.model);
  }
  for (const Q2PlayerState& p : s.raw.players) {
    mix(p.origin, sizeof p.origin);
    mix(p.stats, sizeof p.stats);
  }
  return h;
}

RulesRunner native_q2_runner(std::unique_ptr<NativeQ2> s) {
  NativeQ2* p = s.release();
  return {&kNativeCalls, p, p};
}

NativeQ2* native_q2_of(const RulesRunner& r) {
  return r.calls == &kNativeCalls ? static_cast<NativeQ2*>(r.rules) : nullptr;
}

std::string native_q2_error(const NativeQ2& s) {
  return s.host != nullptr && s.host->dead ? s.host->error_text : std::string();
}

}  // namespace shooters_edge::server
