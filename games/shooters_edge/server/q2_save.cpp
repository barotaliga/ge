// Quake II's levels kept and its games saved (q2_save.hpp states the rules, the chunks and
// the contract; the facts are the clean-room note's, porting/shooters-edge/q2-campaign.md).
//
// Every part is walked by ONE template, run by the save's Writer to write and by its Reader to
// read, so a field saved is a field loaded. A kept level is the same walk's bytes held in memory.
// An index read is checked against what it indexes before the rules can use it.
//
// ge:summary Quake II's state walked once for a kept level, a save's writer and its reader
// ge:keywords q2_save q2_load q2_digest describe chunk kept-level revisit autosave
// ge:skill shooter save
// ge:test tests/shooters_edge_q2_save_test.cpp
#include "shooters_edge/server/q2_save.hpp"

#include <algorithm>
#include <utility>

#include "brush/map.hpp"
#include "save/file.hpp"
#include "save/io.hpp"
#include "save/xxh64.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::server {

namespace {

namespace save = ge::save;

constexpr uint32_t kGame = save::tag("SQGM");
constexpr uint32_t kLevel = save::tag("SQLV");
constexpr uint32_t kPlayers = save::tag("SQPL");
constexpr uint16_t kChunkVersion = 9;  // 2: a monster's duck; 3: a coop weapon taken; 4: a dropped
                                       // silencer's shots; 5: a corpse's box; 6: a monster's
                                       // liquid; 7: the trail, a monster's hunt and calls; 8: a
                                       // soldier's run-and-shoot and head shot; 9: coop power
                                       // cubes, the Makron's torso
constexpr const char* kSaveGameName = "shooters_edge_q2";  // the file's game (Meta::game)
constexpr uint32_t kSaveVersion = 1;
constexpr int kSettleFrames = 2;   // a map's frames before the kept state is read over it
constexpr int kAloneFrames = 100;  // a level entered again runs 10 s before the players come in
constexpr double kLoadMenuAfter = 1.0;  // s after dying a button asks for the load menu

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }
bool coop(const Q2Server& s) { return !deathmatch(s) && s.cvars.find("coop")->on(); }

// A count or index the reader takes only within least..most.
template <class Io>
bool within(Io& io, int64_t v, int64_t least, int64_t most, const char* what) {
  if (v >= least && v <= most) return true;
  io.fail("the save's " + std::string(what) + " is " + std::to_string(v) + ", outside " +
          std::to_string(least) + ".." + std::to_string(most) + ": the file is damaged");
  return false;
}

// A vector's length, the vector sized to it on reading.
template <class Io, class T>
void sized(Io& io, std::vector<T>& v) {
  uint32_t n = static_cast<uint32_t>(v.size());
  io.length(n, 1);
  if constexpr (Io::kReading) v.resize(n);
}

template <class Io>
void v3(Io& io, sh::Vec3f& v) {
  io(v.x);
  io(v.y);
  io(v.z);
}

template <class Io>
void entity(Io& io, Q2Entity& e) {
  io(e.respawn_at);
  io(e.think_at);
  io(e.touched_at);
  io(e.until);
  io(e.tick_at);
  for (std::string* t : {&e.classname, &e.targetname, &e.target, &e.model, &e.message,
                         &e.killtarget, &e.team, &e.pathtarget, &e.noise, &e.map})
    io(*t);
  for (int* n : {&e.modelindex, &e.spawnflags, &e.item, &e.block, &e.owner}) io(*n);
  for (float* f : {&e.speed, &e.accel, &e.decel, &e.wait, &e.lip, &e.delay, &e.random}) io(*f);
  for (int* n : {&e.dmg, &e.health, &e.max_health, &e.sounds, &e.state, &e.team_master, &e.think,
                 &e.activator, &e.frames})
    io(*n);
  for (float* f : {&e.rest, &e.now_speed, &e.next_speed, &e.top, &e.decel_span}) io(*f);
  for (int* n : {&e.arrive, &e.count, &e.frame, &e.needs, &e.aim}) io(*n);
  for (sh::Vec3f* v :
       {&e.origin, &e.angles, &e.mins, &e.maxs, &e.dir, &e.closed, &e.open, &e.velocity, &e.dest,
        &e.avelocity, &e.adest, &e.closed_angles, &e.open_angles, &e.axis})
    v3(io, *v);
  for (bool* b : {&e.hidden, &e.gone, &e.solid, &e.trigger, &e.moved, &e.last, &e.accelerated,
                  &e.turns, &e.box, &e.on})
    io(*b);
  io.since(3, e.coop_taken, false);
  io.since(5, e.corpse, false);
}

template <class Io>
void monster(Io& io, Q2Monster& m) {
  for (int* n : {&m.entity, &m.kind, &m.enemy}) io(*n);
  io(m.ideal_yaw);
  v3(io, m.velocity);
  io(m.on_ground);
  v3(io, m.last_sight);
  for (double* d : {&m.seen_at, &m.attack_finished, &m.busy_until, &m.fire_at, &m.burst_until})
    io(*d);
  io(m.attack);
  io(m.burst_frame);
  io(m.pain_at);
  io(m.pain_hold);
  io(m.gravity);
  io(m.corner);
  io(m.combat);
  for (bool* b : {&m.hold, &m.ambush, &m.waiting, &m.dead}) io(*b);
  io(m.item);
  io(m.loops);
  io(m.aimed);
  v3(io, m.aim_at);
  io(m.leaping);
  io(m.struck);
  io(m.screen);
  io(m.died_at);
  io(m.end_at);
  io(m.boom_step);
  io(m.patient);
  io(m.old_enemy);
  io(m.revive_at);
  io(m.claimed);
  io(m.spare);
  io(m.used_at);
  io.since(2, m.duck_frame, -1);
  io.since(2, m.duck_fresh, false);
  io.since(2, m.duck_shoot, false);
  io.since(2, m.ducked, false);
  io.since(2, m.duck_hold, 0.0);
  io.since(2, m.duck_pose, -1);
  SqMonsterPose& p = m.pose;
  io(p.act);
  io(p.span.first);
  io(p.span.last);
  io(p.frame);
  io(p.slot);
  io(p.cue_attack);
  io(p.cue_pain);
  v3(io, p.last);
  io.since(6, m.water_level, 0);
  io.since(6, m.water_type, 0);
  io.since(6, m.in_water, false);
  io.since(6, m.air_until, 12.0);
  io.since(6, m.liquid_hurt_at, 0.0);
  io.since(7, m.corner_wait, 0.0);
  if (io.version() >= 7) {  // a noise it goes to, and its hunt out of sight
    io(m.sound_target);
    v3(io, m.sound_at);
    io(m.sound_time);
    io(m.lost);
    io(m.on_trail);
    v3(io, m.goal);
    io(m.trail_time);
    io(m.search_time);
    io(m.idle_at);
    io(m.side_step);
    v3(io, m.real_goal);
  }
  io.since(8, m.run_shoot, -1);
  io.since(8, m.head_shot, false);
  io.since(8, m.death_run, -1);
  io.since(8, m.stomp, -1);
  io.since(8, m.flies_at, 0.0);
  io.since(8, m.slide, false);
  io.since(8, m.slide_left, false);
  io.since(8, m.pose.pain_run.first, int16_t{-1});
  io.since(8, m.pose.pain_run.last, int16_t{-1});
}

template <class Io>
void missile(Io& io, Q2Missile& m) {
  io(m.live);
  io(m.kind);
  io(m.owner);
  io(m.weapon);
  v3(io, m.origin);
  v3(io, m.velocity);
  io(m.dies_at);
  io(m.damage);
  io(m.radius_damage);
  io(m.radius);
  io(m.resting);
  io(m.from);
  io(m.burst);
  io(m.reach);
  io(m.fired_z);
}

template <class Io>
void drop(Io& io, Q2Drop& d) {
  io(d.live);
  io(d.item);
  io(d.count);
  io(d.owner);
  v3(io, d.origin);
  v3(io, d.velocity);
  io(d.takeable_at);
  io(d.dies_at);
  io(d.power_left);
  io(d.resting);
  io.since(4, d.shots, 0);
}

template <class Io>
void corpse(Io& io, Q2Body& b) {
  io(b.live);
  v3(io, b.origin);
  io(b.yaw);
  io(b.health);
  io(b.frame);
}

template <class Io>
void body(Io& io, SqBody& b) {
  io(b.health);
  io(b.max_health);
  io(b.counts);
  io(b.ammo);
  io(b.max_ammo);
  io(b.armour);
  io(b.armour_count);
  io(b.blaster);
  io(b.mega_until_drain);
  io(b.selected);
  io.since(9, b.cubes, 0);
}

template <class Io>
void move_state(Io& io, ge::classic::Q2State& m) {
  io(m.type);
  io(m.origin);
  io(m.velocity);
  io(m.flags);
  io(m.time);
  io(m.gravity);
  io(m.delta_angles);
}

template <class Io>
void view_state(Io& io, ge::classic::Q2ViewState& v) {
  for (float* f : {&v.bob_time, &v.bob_step, &v.kick_pitch, &v.kick_roll, &v.kick_left,
                   &v.fall_value, &v.fall_left})
    io(*f);
  io(v.blow_count);
  io(v.blow_push);
  v3(io, v.blow_way);
  v3(io, v.weapon_angles);
  v3(io, v.weapon_origin);
}

// SQLV: the level, without its players.
template <class Io>
void level(Io& io, Q2Server& s) {
  io(s.frame_number);
  io(s.level_time);
  io(s.host_time);
  io(s.pain_turn);
  io(s.death_turn);
  sized(io, s.entities);
  for (Q2Entity& e : s.entities) entity(io, e);
  const int64_t n = static_cast<int64_t>(s.entities.size());
  if constexpr (Io::kReading)
    for (const Q2Entity& e : s.entities)
      if (!within(io, e.owner, -1, n - 1, "an entity's owner") ||
          !within(io, e.team_master, -1, n - 1, "an entity's team") ||
          !within(io, e.aim, -1, n - 1, "a laser's aim") ||
          !within(io, e.item, -1, kSqItemCount - 1, "an entity's item") ||
          !within(io, e.needs, -1, kSqItemCount - 1, "a lock's item") ||
          !within(io, e.activator, -1, sh::kMaxClients - 1, "an entity's activator"))
        return;
  sized(io, s.monsters);
  for (Q2Monster& m : s.monsters) {
    monster(io, m);
    if constexpr (Io::kReading)
      if (!within(io, m.entity, 0, n - 1, "a monster's entity") ||
          !within(io, m.enemy, -1 - n, sh::kMaxClients - 1, "a monster's enemy") ||
          !within(io, m.old_enemy, -1 - n, sh::kMaxClients - 1, "a monster's enemy") ||
          !within(io, m.corner, -1, n - 1, "a monster's path") ||
          !within(io, m.combat, -1, n - 1, "a monster's combat point"))
        return;
  }
  if constexpr (Io::kReading)
    for (const Q2Monster& m : s.monsters)
      if (!within(io, m.patient, -1, int64_t(s.monsters.size()) - 1, "a medic's patient")) return;
  for (Q2Missile& m : s.missiles) missile(io, m);
  for (Q2Drop& d : s.drops) drop(io, d);
  for (Q2Body& b : s.bodies) corpse(io, b);
  io(s.next_body);
  if (!within(io, s.next_body, 0, kQ2Bodies - 1, "the next body")) return;
  io(s.intermission_at);
  io(s.next_map);
  io(s.exit_now);
  sized(io, s.delayed);
  for (Q2Server::Delayed& d : s.delayed) {
    io(d.entity);
    io(d.at);
    io(d.activator);
    io.since(5, d.burst, false);
    if (!within(io, d.entity, 0, n - 1, "a delayed use's entity")) return;
  }
  for (int* c : {&s.secrets, &s.secrets_found, &s.goals, &s.goals_found}) io(*c);
  uint32_t portals = static_cast<uint32_t>(s.portals.size());
  io(portals);
  if (portals != s.portals.size()) {
    io.fail("the save's level has " + std::to_string(portals) + " area portals, the map " +
            std::to_string(s.portals.size()) + ": it was saved on another version of the map");
    return;
  }
  io.raw(s.portals.data(), s.portals.size());
  io(s.monsters_total);
  io(s.monsters_killed);
  io(s.sight_entity);
  io(s.sight_frame);
  io(s.sight_client);
  for (std::string& style : s.world.lightstyles) io(style);
  if (io.version() >= 7) {  // the players' trail
    for (int i = 0; i < 8; i++) {
      v3(io, s.trail_at[i]);
      io(s.trail_time[i]);
      io(s.trail_yaw[i]);
    }
    io(s.trail_head);
  }
}

// A client's record (rules_runner.hpp).
template <class Io>
void client(Io& io, ClientInfo& c) {
  io(c.active);
  io(c.spectator);
  io(c.bot);
  io(c.userid);
  io(c.userinfo);
  io(c.spawn_parms);
  io(c.stuff);
  io(c.drop);
  io(c.address);
  io(c.ping_ms);
  io(c.entered_at);
  io(c.blow.hit);
  io(c.blow.armor);
  io(c.blow.blood);
  v3(io, c.blow.from);
}

template <class Io>
void player(Io& io, Q2Player& p) {
  move_state(io, p.move);
  io(p.last_sequence);
  io(p.alive);
  io(p.health);
  io(p.frags);
  io(p.spawned_at);
  io(p.died_at);
  io(p.buttons);
  io(p.cmd_angles);
  io(p.changed);
  io(p.forced);
  io(p.forced_angles);
  v3(io, p.view_angles);
  body(io, p.body);
  io(p.mega);
  io(p.drained_at);
  io(p.taken);
  v3(io, p.mins);
  v3(io, p.maxs);
  io(p.view_height);
  io(p.water_level);
  io(p.water_type);
  io(p.on_ground);
  SqHand& h = p.hand;
  io(h.weapon);
  io(h.pending);
  io(h.state);
  io(h.frame);
  io(h.click_at);
  io(h.held_shots);
  io(h.fuse_at);
  io(p.stepped);
  io(p.killer);
  for (double* d : {&p.quad_until, &p.invulnerable_until, &p.breather_until, &p.suit_until}) io(*d);
  io(p.silencer_shots);
  io(p.power_on);
  io(p.air_until);
  io(p.drown_at);
  io(p.drown_damage);
  io(p.old_fall);
  io(p.landed);
  io(p.frame_damage);
  io(p.pain_sound_at);
  io(p.old_water_level);
  io(p.breath_second);
  io(p.gibbed);
  io(p.ground);
  io(p.windfly_at);
  io(p.noise_frame);
  v3(io, p.noise_at);
  io(p.help_seen);
  io(p.help_unread);
  io(p.screen);
  io(p.layout);
  io(p.inven_open);
  io(p.inventory);
  io(p.carried);
  io(p.has_entered);
  io(p.midair.hits);
  io(p.midair.highest);
  io(p.midair.rise_sum);
  body(io, p.carry);
  body(io, p.entered);
  SqPose& q = p.pose;
  io(q.frame);
  io(q.end);
  io(q.priority);
  io(q.ducked);
  io(q.running);
  io(q.shot);
  io(q.threw);
  io(q.gesture);
  io(q.health);
  io(q.armour);
  io(q.weapon);
  io(p.picked);
  io(p.picked_at);
  io(p.hit_frame);
  io(p.hit_bits);
  view_state(io, p.view);
  v3(io, p.seen.offset);
  v3(io, p.seen.kick);
  v3(io, p.dead_angles);
  io(p.god);
  io(p.notarget);
  io(p.noclip);
  io(p.last_weapon);
  io(p.held_weapon);
  io(p.said_at);
  io(p.said_next);
  io(p.quiet_until);
  io(p.load_asked_at);
}

// SQPL: every slot's client and body.
template <class Io>
void players(Io& io, Q2Server& s) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    client(io, s.clients[i]);
    player(io, s.players[i]);
    if constexpr (Io::kReading)
      if (!within(io, s.players[i].ground, -1, int64_t(s.world.entities.size()) - 1,
                  "a player's ground") ||
          !within(io, s.players[i].mega, -1, int64_t(s.entities.size()) - 1, "a mega health"))
        return;
  }
}

// A slot as the game knows it between levels: who is in it and what he carries.
struct SlotRecord {
  bool active = false, spectator = false, bot = false;
  int userid = 0;
  std::vector<std::pair<std::string, std::string>> userinfo;
  bool carried = false;
  SqBody carry;
  int help_seen = 0, help_unread = 0;
};

// SQGM: the game across its levels.
struct GameRecord {
  std::string map;
  bool autosave = false;
  std::vector<std::pair<std::string, std::string>> settings;  // every cvar off its default
  std::string help[2];
  int help_changed = 0;
  std::string spawn_name;
  uint32_t cross_bits = 0;
  std::vector<std::pair<std::string, std::vector<uint8_t>>> kept;
  ge::core::Rng rng;
  int next_userid = 1;
  SlotRecord slots[sh::kMaxClients];
};

template <class Io>
void game(Io& io, GameRecord& g) {
  io(g.map);
  io(g.autosave);
  io(g.settings);
  io(g.help[0]);
  io(g.help[1]);
  io(g.help_changed);
  io(g.spawn_name);
  io(g.cross_bits);
  io(g.kept);
  io(g.rng);
  io(g.next_userid);
  for (SlotRecord& r : g.slots) {
    io(r.active);
    io(r.spectator);
    io(r.bot);
    io(r.userid);
    io(r.userinfo);
    io(r.carried);
    body(io, r.carry);
    io(r.help_seen);
    io(r.help_unread);
  }
}

GameRecord record_of(const Q2Server& s, bool autosave) {
  GameRecord g;
  g.map = s.map_name;
  g.autosave = autosave;
  for (const ge::console::Cvar* v : s.cvars.sorted())
    if (!v->is_default()) g.settings.emplace_back(v->name, v->value);
  g.help[0] = s.help[0];
  g.help[1] = s.help[1];
  g.help_changed = s.help_changed;
  g.spawn_name = s.spawn_name;
  g.cross_bits = s.cross_bits;
  g.kept = s.kept;
  g.rng = s.rng;
  g.next_userid = s.next_userid;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClientInfo& c = s.clients[i];
    const Q2Player& p = s.players[i];
    SlotRecord& r = g.slots[i];
    r.active = c.active;
    r.spectator = c.spectator;
    r.bot = c.bot;
    r.userid = c.userid;
    r.userinfo = c.userinfo;
    r.carried = p.carried;
    r.carry = p.carry;
    r.help_seen = p.help_seen;
    r.help_unread = p.help_unread;
  }
  return g;
}

std::vector<uint8_t> level_bytes(const Q2Server& s) {
  std::vector<uint8_t> out;
  save::Writer w(out, kChunkVersion);
  level(w, const_cast<Q2Server&>(s));  // the Writer reads only
  return out;
}

std::string title_of(const Q2Server& s) {
  if (s.map != nullptr && !s.map->entities.empty()) {
    const std::string m = s.map->entities[0].get_any_case("message");
    if (!m.empty()) return m;
  }
  return s.map_name;
}

// Writes the game into `out`: the level and the players unless it is an autosave.
bool write_save(const Q2Server& s, std::vector<uint8_t>& out, std::string& error,
                int64_t saved_unix, bool autosave) {
  save::Meta meta;
  meta.game = kSaveGameName;
  meta.game_version = kSaveVersion;
  meta.title = (autosave ? "ENTERING " : "") + title_of(s) + ", kills " +
               std::to_string(s.monsters_killed) + "/" + std::to_string(s.monsters_total);
  meta.place = s.map_name;
  meta.play_s = s.level_time;
  meta.saved_unix = saved_unix;
  meta.setup = "map " + s.map_name;
  save::FileWriter w(out, meta);
  GameRecord g = record_of(s, autosave);
  game(w.chunk(kGame, 0, kChunkVersion), g);
  Q2Server& live = const_cast<Q2Server&>(s);  // the Writer reads only
  if (!autosave) {
    level(w.chunk(kLevel, 0, kChunkVersion), live);
    players(w.chunk(kPlayers, 0, kChunkVersion), live);
  }
  std::string why;
  if (!w.finish(&why)) {
    error = "save: " + why;
    return false;
  }
  return true;
}

// The level's bytes read over the running map; false with `error` when they do not read.
bool read_level(Q2Server& s, const std::vector<uint8_t>& bytes, std::string& error) {
  save::Reader r(bytes.data(), bytes.size(), kChunkVersion);
  level(r, s);
  if (r.ok() && !r.at_end()) r.fail("the level has bytes left over once read");
  if (!r.ok()) {
    error = "level '" + s.map_name + "' kept at byte " + std::to_string(r.offset()) + ": " +
            r.problem();
    return false;
  }
  return true;
}

void ask_load_menu(Q2Server& s, int slot) {
  NetEvent ev;
  ev.kind = EventKind::stuffcmd;
  ev.to = 1u << slot;
  ev.text = "menu_loadgame\n";
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

}  // namespace

void q2_save_cvars(Q2Server& s) {
  s.cvars.add({.name = "sv_noreload",
               .fallback = "0",
               .flags = ge::console::kServer,
               .help = "Whether a level entered again starts afresh instead of as it was left.",
               .values = "0: as it was left (the default); 1: afresh"});
}

void q2_leave(Q2Server& s) {
  if (s.loading) return;  // the game loaded brings its own
  if (s.pending_change == MapChange::new_game) {
    s.kept.clear();
    return;
  }
  if (s.map == nullptr || s.pending_change != MapChange::change_level || deathmatch(s)) return;
  if (s.next_unit) {  // a new unit: its levels start afresh
    s.kept.clear();
    return;
  }
  std::vector<uint8_t> bytes = level_bytes(s);
  for (auto& [name, kept] : s.kept)
    if (name == s.map_name) {
      kept = std::move(bytes);
      return;
    }
  s.kept.emplace_back(s.map_name, std::move(bytes));
}

bool q2_enter(Q2Server& s, std::string& error) {
  const std::vector<uint8_t>* back = nullptr;
  if (s.loading && !s.load_level.empty()) back = &s.load_level;
  const bool revisit = s.loading || (s.pending_change == MapChange::change_level &&
                                     !s.cvars.find("sv_noreload")->on());
  if (back == nullptr && revisit && !deathmatch(s))
    for (const auto& [name, kept] : s.kept)
      if (name == s.map_name) back = &kept;
  if (back != nullptr) {
    q2_frames_alone(s, kSettleFrames);
    const bool kept = back != &s.load_level;
    if (!read_level(s, *back, error)) return false;
    if (kept) {  // its players are gone: nobody is hunted
      for (Q2Monster& m : s.monsters) m.enemy = m.old_enemy = -1;
      s.sight_client = -1;
    }
    map_revisit(s);
    q2_rebuild(s);
    if (!s.loading) q2_frames_alone(s, kAloneFrames);
  }
  s.autosave.clear();
  if (!deathmatch(s) && !s.loading && s.pending_change != MapChange::restart) {
    std::string why;
    if (!write_save(s, s.autosave, why, 0, true)) s.autosave.clear();
  }
  return true;
}

void q2_save_frame(Q2Server& s) {
  if (deathmatch(s) || coop(s)) return;
  for (int i = 0; i < sh::kMaxClients; i++) {
    Q2Player& p = s.players[i];
    if (!s.clients[i].active || s.clients[i].spectator || p.alive || p.buttons == 0) continue;
    if (s.level_time + 0.001 < p.died_at + kLoadMenuAfter || p.load_asked_at >= p.died_at) continue;
    p.load_asked_at = s.level_time;
    ask_load_menu(s, i);
  }
}

std::string q2_save_problem(const Q2Server& s) {
  if (s.map == nullptr) return "Can't save: no game is running.";
  if (deathmatch(s)) return "Can't save a deathmatch game.";
  if (s.intermission_at > 0 || !s.pending_map.empty()) return "Can't save now: the level is over.";
  if (!s.mods.tables.empty()) return "Can't save a game with mods running.";
  int in = 0;
  for (int i = 0; i < sh::kMaxClients; i++) in += s.clients[i].active ? 1 : 0;
  if (in == 0) return "Can't save: nobody is in the game.";
  if (!coop(s))
    for (int i = 0; i < sh::kMaxClients; i++)
      if (s.clients[i].active && !s.clients[i].spectator && !s.players[i].alive)
        return "Can't save while you are dead.";
  return "";
}

bool q2_save(const Q2Server& s, std::vector<uint8_t>& out, std::string& error, int64_t saved_unix) {
  error = q2_save_problem(s);
  if (!error.empty()) {
    error = "save: " + error;
    return false;
  }
  return write_save(s, out, error, saved_unix, false);
}

namespace {

// A chunk read where it belongs, to its end; false with `error` naming it.
template <class Walk>
bool read_chunk(const save::FileReader& f, uint32_t tag, std::string_view name, bool needed,
                std::string& error, Walk&& walk) {
  const save::ChunkInfo* info = f.find(tag);
  if (info == nullptr && !needed) return true;
  if (info == nullptr || info->version > kChunkVersion) {
    error = std::string(name) + ": chunk " + save::tag_name(tag) +
            (info == nullptr ? " missing" : " newer than this build reads") +
            ": it was saved by another version of the game";
    return false;
  }
  save::Reader r = f.reader(*info);
  walk(r);
  if (r.ok() && !r.at_end()) r.fail("the chunk has bytes left over once read");
  if (!r.ok()) {
    error = std::string(name) + ": chunk " + save::tag_name(tag) + " at byte " +
            std::to_string(r.offset()) + ": " + r.problem() +
            " (the file is damaged or from another version of the game)";
    return false;
  }
  return true;
}

bool open_save(save::FileReader& f, std::span<const uint8_t> bytes, std::string_view name,
               std::string& error) {
  if (!f.open(bytes, name, kSaveGameName)) {
    error = f.problem();
    return false;
  }
  if (f.meta().game_version > kSaveVersion) {
    error = std::string(name) + ": written by a newer game (save version " +
            std::to_string(f.meta().game_version) + ")";
    return false;
  }
  return true;
}

}  // namespace

std::string q2_save_map(std::span<const uint8_t> bytes, std::string_view name, std::string& error) {
  save::FileReader f;
  if (!open_save(f, bytes, name, error)) return "";
  GameRecord g;
  if (!read_chunk(f, kGame, name, true, error, [&](save::Reader& r) { game(r, g); })) return "";
  return g.map;
}

std::string q2_load_ask(Q2Server& s, std::span<const uint8_t> bytes, std::string_view name,
                        std::string& error) {
  save::FileReader f;
  if (!open_save(f, bytes, name, error)) return "";
  GameRecord g;
  if (!read_chunk(f, kGame, name, true, error, [&](save::Reader& r) { game(r, g); })) return "";
  for (const uint32_t tag : {kLevel, kPlayers})
    if (const save::ChunkInfo* info = f.find(tag);
        info != nullptr && info->version > kChunkVersion) {
      error = std::string(name) + ": chunk " + save::tag_name(tag) +
              " newer than this build reads: it was saved by another version of the game";
      return "";
    }
  s.load_file.assign(bytes.begin(), bytes.end());
  s.load_name = name;
  return g.map;
}

bool q2_load(const RulesRunner& r, const MapSpec& map, std::span<const uint8_t> bytes,
             std::string_view name, std::string& error) {
  Q2Server* s = q2_of(r);
  if (s == nullptr) {
    error = std::string(name) + ": only Quake II's rules load its saves";
    return false;
  }
  const std::string saved = q2_load_ask(*s, bytes, name, error);
  if (saved.empty()) return false;
  if (saved != map.name) {
    s->load_file.clear();
    error = std::string(name) + ": the save is of map " + saved + ", not " + map.name;
    return false;
  }
  return r.start(map, error);
}

namespace {

// The file a load asked for, opened again and its game read (it was checked when asked).
bool reopen(const Q2Server& s, save::FileReader& f, GameRecord& g, std::string& error) {
  return open_save(f, s.load_file, s.load_name, error) &&
         read_chunk(f, kGame, s.load_name, true, error, [&](save::Reader& r) { game(r, g); });
}

}  // namespace

bool q2_load_begin(Q2Server& s, std::string& error) {
  save::FileReader f;
  GameRecord g;
  if (!reopen(s, f, g, error)) return false;
  // The game playing (if any) let go: its settings back to their defaults, then the save's.
  for (ge::console::Cvar* v : s.cvars.sorted())
    if (!v->is_default()) s.cvars.reset(*v, ge::console::Source::local);
  for (const auto& [cvar, value] : g.settings) {
    std::string line = cvar;
    line += " \"";
    line += value;
    line += '"';
    q2_console(s, line);
  }
  s.help[0] = g.help[0];
  s.help[1] = g.help[1];
  s.help_changed = g.help_changed;
  s.cross_bits = g.cross_bits;
  s.kept = std::move(g.kept);
  s.next_spawn = g.spawn_name;
  s.next_unit = false;
  s.pending_change = MapChange::change_level;
  s.loading = true;
  s.load_over_game = s.map != nullptr;
  s.load_level.clear();
  if (const save::ChunkInfo* info = f.find(kLevel); info != nullptr) {
    const uint8_t* at = s.load_file.data() + info->offset;
    s.load_level.assign(at, at + info->raw);
  }
  return true;
}

bool q2_load_end(Q2Server& s, std::string& error) {
  save::FileReader f;
  GameRecord g;
  const std::string name = s.load_name;
  const bool ok = reopen(s, f, g, error);
  s.loading = false;
  s.load_level.clear();
  if (!ok) {
    s.load_file.clear();
    return false;
  }
  // Who plays now: a new game takes every saved player; a running one keeps those connected,
  // each in his saved body or coming in afresh.
  const bool running = s.load_over_game;
  bool connected[sh::kMaxClients] = {};
  std::string names[sh::kMaxClients];
  for (int i = 0; i < sh::kMaxClients; i++) {
    connected[i] = s.clients[i].active;
    for (const auto& [key, value] : s.clients[i].userinfo)
      if (key == "name") names[i] = value;
    s.clients[i] = ClientInfo{};
    s.players[i] = Q2Player{};
  }
  if (f.find(kPlayers) != nullptr) {
    if (!read_chunk(f, kPlayers, name, true, error, [&](save::Reader& rd) { players(rd, s); })) {
      s.load_file.clear();
      return false;
    }
  } else {  // an autosave: each comes in at the spawn point with what he carried
    for (int i = 0; i < sh::kMaxClients; i++) {
      const SlotRecord& rec = g.slots[i];
      if (!rec.active) continue;
      ClientInfo& c = s.clients[i];
      c.active = true;
      c.spectator = rec.spectator;
      c.bot = rec.bot;
      c.userid = rec.userid;
      c.userinfo = rec.userinfo;
      c.entered_at = s.level_time;
      Q2Player& p = s.players[i];
      p.carried = rec.carried;
      p.carry = rec.carry;
      p.help_seen = rec.help_seen;
      p.help_unread = rec.help_unread;
      q2_spawn(s, i);
    }
  }
  s.load_file.clear();
  if (running)
    for (int i = 0; i < sh::kMaxClients; i++) {
      if (!connected[i] && s.clients[i].active) {
        s.clients[i] = ClientInfo{};
        s.players[i] = Q2Player{};
      } else if (connected[i] && !s.clients[i].active) {
        q2_connect(s, i, names[i]);
      }
    }
  for (int i = 0; i < sh::kMaxClients; i++)  // a player connecting takes his body over
    if (s.clients[i].active) s.restored |= 1u << i;
  for (int i = 0; i < sh::kMaxClients; i++)  // what the user info sets beside the rules' record
    if (s.clients[i].active) {
      const auto info = s.clients[i].userinfo;
      for (const auto& [key, value] : info) q2_user_info(s, i, key, value);
    }
  s.rng = g.rng;
  s.next_userid = g.next_userid;
  q2_rebuild(s);
  return true;
}

uint64_t q2_digest(const Q2Server& s) {
  std::vector<uint8_t> bytes;
  Q2Server& live = const_cast<Q2Server&>(s);  // the Writer reads only
  save::Writer w(bytes, kChunkVersion);
  GameRecord g = record_of(s, false);
  game(w, g);
  level(w, live);
  players(w, live);
  return save::xxh64(bytes.data(), bytes.size(), rules_hash(s));
}

}  // namespace shooters_edge::server
