// The QC server's half of the rules' world: its lists copied at the map's start, its clients and
// every entity's row rewritten in place at the end of each frame.
//
// ge:summary The QC server filling the rules' world: lists at the start, rows every frame
// ge:keywords rules-world fill_world qc-server entity-rows player-numbers weapon-model
// ge:skill progs shooter
// ge:test tests/shooters_edge_rules_world_test.cpp
#include "shooters_edge/server/rules_world.hpp"

#include "progs/vm.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

namespace {

float field(const QcServer& s, int e, int offset) {
  if (offset < 0) return 0;
  return pg::as_float(*pg::entity_field(s.q.vm, e, offset));
}

// The model list's index of a text value (a weapon model's name); 0 when empty or not listed.
// `was` is last frame's answer, checked first, so a held weapon costs one compare a frame.
int model_index_of(const QcServer& s, int32_t text, int was) {
  const char* name = pg::vm_string(s.q.vm, text);
  if (name == nullptr || name[0] == 0) return 0;
  const auto at = static_cast<size_t>(was);
  if (was > 0 && at < s.models.size() && s.models[at] == name) return was;
  for (size_t i = 1; i < s.models.size(); i++)
    if (s.models[i] == name) return static_cast<int>(i);
  return 0;
}

// The user info's name, or nullptr.
const std::string* name_of(const ClientInfo& c) {
  for (const auto& [k, v] : c.userinfo)
    if (k == "name") return &v;
  return nullptr;
}

template <typename T>
void assign_if_changed(T& to, const T& from) {
  if (to != from) to = from;
}

void fill_numbers(const QcServer& s, int slot, WorldClient& c) {
  const int e = slot + 1;
  const auto& f = s.q.layout->f;
  const auto& w = s.world_fields;
  PlayerNumbers& n = c.numbers;
  n.health = static_cast<int32_t>(field(s, e, f.health));
  n.armour = static_cast<int32_t>(field(s, e, w.armorvalue));
  for (int k = 0; k < 4; k++) n.ammo[k] = static_cast<int32_t>(field(s, e, w.ammo[k]));
  n.current_ammo = static_cast<int32_t>(field(s, e, w.currentammo));
  n.weapon = static_cast<int32_t>(field(s, e, w.weapon));
  n.weapon_frame = static_cast<int>(field(s, e, w.weaponframe));
  n.items = static_cast<int32_t>(field(s, e, f.items));
  n.view_z = field(s, e, f.view_ofs + 2);
  n.frags = field(s, e, f.frags);
  n.weapon_model = model_index_of(
      s, static_cast<int32_t>(*pg::entity_field(s.q.vm, e, f.weaponmodel)), n.weapon_model);
}

}  // namespace

void fill_world(const QcServer& s, RulesWorld& w, bool full) {
  // Before the first map there is no program running and nothing to show.
  if (s.q.layout == nullptr || s.q.vm.globals.empty() || s.sv.edicts.empty()) return;
  if (full) {
    w.map_name = s.map_name;
    w.models = s.models;
    w.sounds = s.sounds;
    w.vwep = s.vwep;
    w.statics.clear();
    for (const StaticEntity& st : s.statics)
      w.statics.push_back({st.modelindex, static_cast<int>(st.frame), static_cast<int>(st.colormap),
                           static_cast<int>(st.skin), st.origin, st.angles,
                           st.extras.has_alpha ? st.extras.alpha : 1.0f});
    w.max_clients = s.sv.set.maxclients;
    w.weapon_frames = s.world_fields.weaponframe >= 0;
    w.map_title.clear();
    {
      const char* title = pg::vm_string(
          s.q.vm, static_cast<int32_t>(*pg::entity_field(s.q.vm, 0, s.q.layout->f.message)));
      if (title != nullptr) w.map_title = title;
    }
    w.entities.assign(static_cast<size_t>(s.sv.set.max_edicts), WorldEntity{});
    for (WorldClient& c : w.clients) c = WorldClient{};
  }
  for (int k = 0; k < kWorldLightStyles; k++) assign_if_changed(w.lightstyles[k], s.lightstyles[k]);
  assign_if_changed(w.serverinfo, s.serverinfo);
  w.time = s.sv.time;
  const int flags_at = s.q.layout->g.serverflags;
  w.serverflags =
      flags_at >= 0 ? pg::as_float(s.q.vm.globals[static_cast<size_t>(flags_at)]) : s.serverflags;
  w.paused = s.paused;

  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClientInfo& from = s.clients[i];
    WorldClient& c = w.clients[i];
    c.active = from.active;
    c.spectator = from.spectator;
    c.bot = from.bot;
    c.userid = from.userid;
    c.ping_ms = from.ping_ms;
    c.entered_at = from.entered_at;
    assign_if_changed(c.userinfo, from.userinfo);
    const std::string* name = name_of(from);
    if (name == nullptr)
      c.name.clear();
    else
      assign_if_changed(c.name, *name);
    if (!from.active)
      c.numbers = PlayerNumbers{};
    else
      fill_numbers(s, i, c);
  }

  const int n = s.sv.num_edicts;
  if (static_cast<size_t>(n) > w.entities.size()) w.entities.resize(static_cast<size_t>(n));
  w.entity_count = n;
  const auto& f = s.q.layout->f;
  for (int e = 0; e < n; e++) {
    const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
    WorldEntity& r = w.entities[static_cast<size_t>(e)];
    r.free = ed.free;
    r.origin = ed.origin;
    r.angles = ed.angles;
    r.velocity = ed.velocity;
    r.mins = ed.mins;
    r.maxs = ed.maxs;
    r.health = ed.health;
    r.modelindex = static_cast<int>(field(s, e, f.modelindex));
    r.frame = static_cast<int>(field(s, e, f.frame));
    r.colormap = static_cast<int>(field(s, e, f.colormap));
    r.skin = static_cast<int>(field(s, e, f.skin));
    r.effects = static_cast<int>(field(s, e, f.effects));
    const pg::EntityExtras& x = s.extras[static_cast<size_t>(e)];
    r.alpha = x.has_alpha ? x.alpha : 1.0f;
  }
}

}  // namespace shooters_edge::server
