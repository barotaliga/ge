// ge:summary A fighter's world rebuilt from what its classic client was sent, for any server
// ge:keywords client-mirror mirror_update put show item_box prediction water_level
// ge:skill bots net shooter
#include "shooters_edge/net/classic_mirror.hpp"

#include <algorithm>
#include <cstdlib>

#include "shooter/entity.hpp"
#include "shooter/pmove.hpp"
#include "shooter/predict.hpp"
#include "shooters_edge/net/classic_info.hpp"

namespace shooters_edge::net {

namespace {

// The protocol's stats, by number.
constexpr int kStatHealth = 0, kStatArmour = 4, kStatShells = 6, kStatNails = 7, kStatRockets = 8,
              kStatCells = 9, kStatActiveWeapon = 10, kStatItems = 15;
// The item bits that name the armour worn, and the share of a blow each takes.
constexpr int32_t kItArmour1 = 8192, kItArmour2 = 16384, kItArmour3 = 32768;

bool starts(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }

// The player slots: the server info's `maxclients`, else every number below the first entity the
// signon's baselines name (the players come first; a packet's entities are never players).
int maxclients_of(const ClassicClientEnd& c) {
  const int n = std::atoi(info_get(c.serverinfo, "maxclients").c_str());
  if (n >= 1 && n <= sh::kMaxClients) return n;
  for (const EntityState& b : c.baselines)
    if (b.number > 0 && b.modelindex > 0) return std::clamp(b.number - 1, 1, sh::kMaxClients);
  return sh::kMaxClients;
}

// The submodel the model list's entry `i` names: the list's first is the map itself (0), "*3" is
// 3; -1 for any other model.
int submodel_of(const std::string& name, size_t i) {
  if (i == 1) return 0;
  if (name.size() >= 2 && name[0] == '*') return std::atoi(name.c_str() + 1);
  return -1;
}

void put(ClientMirror& m, int e, const sh::Vec3f& origin, const sh::Vec3f& angles, int modelindex,
         float health) {
  sh::Server& sv = m.s.sv;
  sh::Edict& d = sv.edicts[static_cast<size_t>(e)];
  d.free = false;
  d.origin = origin;
  d.angles = angles;
  d.modelindex = modelindex;
  d.health = health;
  d.flags = 0;
  d.solid = sh::kSolidNot;
  const std::string& name = static_cast<size_t>(modelindex) < m.s.models.size()
                                ? m.s.models[static_cast<size_t>(modelindex)]
                                : std::string();
  if (e <= sv.set.maxclients) {
    d.mins = sh::kPlayerMins;
    d.maxs = sh::kPlayerMaxs;
  } else if (!name.empty() && name[0] == '*' &&
             static_cast<size_t>(modelindex) < sv.models.size()) {
    // A brush entity: its model's bounds, linked so a trace meets it as the server's would.
    d.solid = sh::kSolidBsp;
    sh::set_brush_model(sv, e, modelindex);
  } else {
    item_box(name, d.mins, d.maxs);
  }
  d.size = d.maxs - d.mins;
  d.absmin = d.origin + d.mins;
  d.absmax = d.origin + d.maxs;
  if (d.solid != sh::kSolidNot) {
    sh::link_edict(sv, e, false);
    m.linked.push_back(e);
  }
  if (e >= sv.num_edicts) sv.num_edicts = e + 1;
}

// The world entity row the rules show, off the edict.
void show(server::WorldEntity& w, const sh::Edict& d) {
  w.free = d.free;
  w.origin = d.origin;
  w.angles = d.angles;
  w.velocity = d.velocity;
  w.mins = d.mins;
  w.maxs = d.maxs;
  w.absmin = d.absmin;
  w.absmax = d.absmax;
  w.view_angles = d.v_angle;
  w.flags = d.flags;
  w.waterlevel = d.waterlevel;
  w.modelindex = d.modelindex;
  w.health = d.health;
}

// The brush models where the newest frame put them, for the prediction's world.
void movers_of(const ClientMirror& m, const ClientFrame& f, std::vector<sh::ClientModel>& out) {
  out.clear();
  for (const EntityState& e : f.ents) {
    if (e.modelindex <= 0 || static_cast<size_t>(e.modelindex) >= m.s.models.size()) continue;
    const std::string& name = m.s.models[static_cast<size_t>(e.modelindex)];
    if (name.size() < 2 || name[0] != '*') continue;
    const int k = std::atoi(name.c_str() + 1);
    if (k > 0 && static_cast<size_t>(k) < m.map_models.size())
      out.push_back({&m.map_models[static_cast<size_t>(k)], e.origin});
  }
}

}  // namespace

void item_box(const std::string& model, sh::Vec3f& mins, sh::Vec3f& maxs) {
  if (starts(model, "maps/b_")) {
    mins = {0, 0, 0};
    maxs = {32, 32, 56};
  } else if (model == "progs/quaddama.mdl" || model == "progs/invulner.mdl" ||
             model == "progs/invisibl.mdl" || model == "progs/suit.mdl") {
    mins = {-16, -16, -24};
    maxs = {16, 16, 32};
  } else {
    mins = {-16, -16, 0};
    maxs = {16, 16, 56};
  }
}

bool mirror_start(ClientMirror& m, const std::vector<sh::BrushModel>& models,
                  const ClassicClientEnd& c, std::string& error) {
  if (models.empty()) {
    error = "mirror_start: the map has no models; next: load the map the server named (" +
            (c.models.size() > 1 ? c.models[1] : std::string("none yet")) + ")";
    return false;
  }
  m.map_models = models;
  m.s.models = c.models;
  m.s.sounds = c.sounds;
  m.s.map_name = c.level;
  m.s.world.map_name = c.level;
  m.s.world.models = c.models;
  m.s.world.sounds = c.sounds;
  m.sound_names = c.sounds;
  // The brush models by the client's model list: the world at 1, `*k` at its index.
  std::vector<sh::BrushModel> by_index;
  for (size_t i = 1; i < c.models.size(); i++) {
    const int k = submodel_of(c.models[i], i);
    if (i == 1 && !starts(c.models[i], "maps/")) {
      error = "mirror_start: the model list's first model is '" + c.models[i] +
              "', not a map; next: wait for the signon's model list";
      return false;
    }
    if (k >= static_cast<int>(models.size())) {
      error = "mirror_start: the server names model " + c.models[i] + " but the map file has " +
              std::to_string(models.size() - 1) +
              " submodels; next: load the server's own copy of " + c.models[1];
      return false;
    }
    by_index.push_back(k >= 0 ? models[static_cast<size_t>(k)] : sh::BrushModel{});
  }
  if (by_index.empty()) {
    error = "mirror_start: no model list yet; next: start the mirror once the client is active";
    return false;
  }
  sh::ServerSettings set;
  set.maxclients = maxclients_of(c);
  set.max_edicts = sh::kMaxEdicts;
  set.move = client_move_settings(c);
  sh::create_server(m.s.sv, set, by_index, sh::Program{});
  m.s.world.max_clients = set.maxclients;
  m.s.world.entities.assign(m.s.sv.edicts.size(), server::WorldEntity{});
  m.linked.clear();
  return true;
}

void mirror_update(ClientMirror& m, const ClassicClientEnd& c) {
  sh::Server& sv = m.s.sv;
  m.updates++;
  sv.time = c.now;
  m.s.world.time = c.now;
  for (const int e : m.linked) sh::unlink_edict(sv, e);
  m.linked.clear();
  for (int e = 1; e < sv.num_edicts; e++) {
    sv.edicts[static_cast<size_t>(e)].free = true;
    sv.edicts[static_cast<size_t>(e)].modelindex = 0;
  }
  sv.num_edicts = sv.set.maxclients + 1;

  // The user infos and the frags.
  for (int i = 0; i < sv.set.maxclients; i++) {
    const std::string& info = c.userinfos[i];
    const bool spectator = !info_get(info, "*spectator").empty();
    sv.clients[static_cast<size_t>(i)].active = !info.empty() && !spectator;
    sv.clients[static_cast<size_t>(i)].spectator = spectator;
    server::WorldClient& w = m.s.world.clients[i];
    w.active = !info.empty();
    w.spectator = spectator;
    w.name = info_get(info, "name");
    w.numbers = {};
    w.numbers.frags = static_cast<float>(c.frags[i]);
    m.s.clients[i].active = w.active;
    m.s.clients[i].spectator = spectator;
  }

  // The newest frame's entities (and, with `baselines`, the rest at their baselines).
  const ClientFrame* f = nullptr;
  if (c.valid_frame >= 0) {
    const ClientFrame& vf = c.frames[c.valid_frame & 63];
    if (vf.valid && vf.sequence == static_cast<uint32_t>(c.valid_frame)) f = &vf;
  }
  m.entities_sent = 0;
  if (f != nullptr) {
    for (const EntityState& e : f->ents) {
      if (e.number <= sv.set.maxclients || e.number >= static_cast<int>(sv.edicts.size())) continue;
      put(m, e.number, e.origin, e.angles, e.modelindex, 0);
      m.entities_sent++;
    }
  }
  if (m.baselines) {
    for (const EntityState& b : c.baselines) {
      if (b.number <= sv.set.maxclients || b.number >= static_cast<int>(sv.edicts.size()) ||
          b.modelindex == 0 || !sv.edicts[static_cast<size_t>(b.number)].free)
        continue;
      put(m, b.number, b.origin, b.angles, b.modelindex, 0);
    }
  }

  // The players the newest packet carried: the client's own always is.
  const int me = c.playernum;
  const double newest = me >= 0 && c.player_seen[me] ? c.player_at[me] : -1;
  int player_model = 0;
  for (size_t i = 1; i < m.s.models.size(); i++)
    if (m.s.models[i] == "progs/player.mdl") player_model = static_cast<int>(i);
  m.players_sent = 0;
  for (int i = 0; i < sv.set.maxclients && newest >= 0; i++) {
    if (!c.player_seen[i] || c.player_at[i] < newest || !sv.clients[static_cast<size_t>(i)].active)
      continue;
    const PlayerState& p = c.players[i];
    const int model = p.modelindex != 0 ? p.modelindex : player_model;
    const float health =
        i == me ? static_cast<float>(c.stats[kStatHealth]) : (p.dead ? 0.f : 100.f);
    const sh::Vec3f view =
        i == me ? sh::Vec3f{c.set_angles[0], c.set_angles[1], c.set_angles[2]} : p.command.angles;
    put(m, i + 1, p.origin, {-view.x / 3, view.y, view.z}, model, health);
    sh::Edict& d = sv.edicts[static_cast<size_t>(i) + 1];
    d.v_angle = view;
    d.velocity = p.velocity;
    if (p.onground) d.flags |= sh::kFlagOnGround;
    m.players_sent++;
  }

  // The client's own player, run forward to now.
  if (me >= 0 && newest >= 0 && me < sv.set.maxclients) {
    sh::Edict& d = sv.edicts[static_cast<size_t>(me) + 1];
    d.maxspeed = client_move_settings(c).maxspeed;
    const uint32_t acked = c.ch.incoming_acknowledged;
    if (m.predict && f != nullptr && acked != 0 && !m.map_models.empty()) {
      sh::MoveCmd cmds[64];
      int count = 0;
      for (uint32_t q = acked + 1; q < c.ch.outgoing_sequence && count < 64; q++)
        if (c.sent_seq[q & 63] == q) cmds[count++] = c.sent[q & 63];
      std::vector<sh::ClientModel> movers;
      movers_of(m, *f, movers);
      std::vector<sh::Vec3f> others;
      for (int i = 0; i < sv.set.maxclients; i++)
        if (i != me && !sv.edicts[static_cast<size_t>(i) + 1].free && c.players[i].dead == false)
          others.push_back(sv.edicts[static_cast<size_t>(i) + 1].origin);
      sh::MoveWorld world;
      sh::client_world(m.map_models[0], movers.data(), static_cast<int>(movers.size()),
                       others.data(), static_cast<int>(others.size()), world);
      const sh::MoveState ahead = sh::predict_player(client_net_player(c.players[me]), cmds, count,
                                                     client_move_settings(c), world);
      d.origin = ahead.origin;
      d.velocity = ahead.velocity;
      // The depth at the predicted place, whether or not a command ran since the server's state
      // (the move sets it only as it runs one).
      d.waterlevel = sh::water_level(world, ahead.origin);
      d.flags = ahead.ground >= 0 ? (d.flags | sh::kFlagOnGround) : (d.flags & ~sh::kFlagOnGround);
      d.absmin = d.origin + d.mins;
      d.absmax = d.origin + d.maxs;
      m.predicted++;
    }
    // Its stats.
    server::PlayerNumbers& n = m.s.world.clients[me].numbers;
    n.health = static_cast<float>(c.stats[kStatHealth]);
    n.armour = static_cast<float>(c.stats[kStatArmour]);
    const int32_t items = c.stats[kStatItems];
    n.armour_type = (items & kItArmour3)   ? 0.8f
                    : (items & kItArmour2) ? 0.6f
                    : (items & kItArmour1) ? 0.3f
                                           : 0.f;
    n.ammo[0] = static_cast<float>(c.stats[kStatShells]);
    n.ammo[1] = static_cast<float>(c.stats[kStatNails]);
    n.ammo[2] = static_cast<float>(c.stats[kStatRockets]);
    n.ammo[3] = static_cast<float>(c.stats[kStatCells]);
    n.items = static_cast<float>(items);
    n.weapon = static_cast<float>(c.stats[kStatActiveWeapon]);
  }

  // The sounds this tick's packets started, each where the packet put it.
  sv.sound_count = 0;
  for (const ClientSound& h : c.heard) {
    if (sv.sound_count >= sh::kMaxSounds) break;
    sh::SoundEvent& se = sv.sounds[sv.sound_count++];
    se.ent = h.ent;
    se.channel = h.channel;
    se.sample = h.sound >= 0 && static_cast<size_t>(h.sound) < m.sound_names.size()
                    ? m.sound_names[static_cast<size_t>(h.sound)].c_str()
                    : "";
    se.volume = h.volume;
    se.attenuation = h.attenuation;
    se.placed = true;
    se.at = h.origin;
    if (h.ent >= sv.num_edicts && h.ent < static_cast<int>(sv.edicts.size()))
      sv.num_edicts = h.ent + 1;
  }
  m.sounds_sent = sv.sound_count;

  m.s.world.entity_count = sv.num_edicts;
  for (int e = 0; e < sv.num_edicts; e++)
    show(m.s.world.entities[static_cast<size_t>(e)], sv.edicts[static_cast<size_t>(e)]);
}

}  // namespace shooters_edge::net
