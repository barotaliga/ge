// The listen server (listen.hpp).
//
// ge:summary The listen server: the mod server and its local player, maps loaded and changed
// ge:keywords listen-server listen_frame listen_advance load_named_map baseline_progs forced-view
// ge:skill shooter progs
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/listen.hpp"
#include "prof/zones.hpp"
#include "shooters_edge/client/q2_look.hpp"
#include "shooters_edge/server/q2_map.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <utility>
#include <vector>

#include "classic/q2_fx_switches.hpp"
#include "core/num.hpp"
#include "progs/layout.hpp"
#include "progs/vm.hpp"
#include "server/net_restart.hpp"
#include "shooter/client.hpp"
#include "shooters_edge/client/q2_flash_fx.hpp"
#include "shooters_edge/client/q2_model_fx.hpp"
#include "shooters_edge/client/q2_temp_fx.hpp"
#include "shooters_edge/server/announce.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/campaign_save.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/q1_save.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_native.hpp"
#include "shooters_edge/server/q2_save.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace sb = ge::shooterbots;

namespace shooters_edge::client {

namespace pg = ge::progs;

// A line of the window's own (an answer to the player's words): ended, so the next line printed
// (a server's "bot1 entered the game") is not joined onto it.
static void say(Listen& l, std::string text) {
  if (text.empty() || text.back() != '\n') text += '\n';
  l.prints.push_back(std::move(text));
}

void listen_print(Listen& l, int level, const std::string& text) {
  l.print_levels.resize(l.prints.size(), 2);
  l.print_levels.push_back(level);
  l.prints.push_back(text);
}

namespace {

// The message bytes of the baseline mod's temporary entities (rules/common_decl.hpp's
// SVC_TEMPENTITY and its TE_ kinds, the multiplayer protocol's layout: a gunshot and blood carry a
// count).
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
  // Quake II's look for its hits and blasts: its game, or its weapons fired whole by ours.
  const bool q2_look = listen_is_q2(l) || listen_q2_arms(l);
  // Quake II's gun flashes (q2_flash_fx.hpp): a player's shot is its sounds and the flash at his
  // gun (his own: the sounds alone); a monster's flash line plays its gun's sound beside the look.
  const bool player_flash = ev.text == "q2_flash";
  const bool monster_flash = ev.kind == sv::EventKind::trail_effect && ev.values[3] > 0 &&
                             ev.text.starts_with("te_monster_flash.");
  if ((player_flash || monster_flash) && l.sounds.size() < kMostListenSounds) {
    const int value = static_cast<int>(player_flash ? ev.values[0] : ev.values[3]);
    const bool own = player_flash && ev.ent == 1;
    std::vector<DemoEffect> drawn;
    const auto pick = static_cast<uint32_t>(ev.ent * 7919 + static_cast<int>(ev.time * 10) * 31);
    q2_flash_fx(value, monster_flash, ev.ent, ev.origin, own ? l.view : sh::Vec3f{}, own, pick,
                l.sounds, drawn);
    if (player_flash) {
      for (const DemoEffect& d : drawn) {
        Effect e;
        e.kind = EffectKind::named_trail;
        e.code = -1;
        e.name = d.name;
        e.entity = d.entity;
        e.at = d.at;
        e.end = d.end;
        e.flash = d.flash;
        l.effects.push_back(std::move(e));
      }
      return;
    }
  }
  Effect e;
  e.code = -1;
  e.name = ev.text;
  if (monster_flash) {  // its gun's spent cases (shells.hpp)
    e.flash = static_cast<int>(ev.values[3]);
    e.flash_monster = true;
  }
  // The sound the Quake II client plays itself for one of the packs' temporary entities (the
  // server sends none for them: q2_temp_fx.hpp), heard where it happened.
  const auto pack_sound = [&](int code, const sh::Vec3f& at, int entity = -1, int channel = 0) {
    const Q2TempSound ts = q2_temp_sound(code, 0);
    if (ts.name == nullptr || !listen_is_q2(l) || l.sounds.size() >= kMostListenSounds) return;
    DemoSound o;
    o.entity = entity;
    o.channel = channel;
    o.name = ts.name;
    o.attenuation = ts.attenuation;
    o.origin = at;
    l.sounds.push_back(o);
  };
  if (ev.kind == sv::EventKind::trail_effect && ev.text == "te_lightning") {
    // A tesla's bolt: from the tesla (`level`) to what it strikes (`ent`), one bolt a pair, its
    // crackle on the tesla's weapon channel each time it is sent.
    e.kind = EffectKind::named_trail;
    e.entity = ev.level;
    e.colour = ev.ent;
    e.at = ev.origin;
    e.end = {ev.values[0], ev.values[1], ev.values[2]};
    l.effects.push_back(std::move(e));
    if (listen_is_q2(l) && l.sounds.size() < kMostListenSounds) {
      DemoSound o;
      o.entity = ev.level;
      o.channel = 1;
      o.name = "weapons/tesla.wav";
      o.origin = ev.origin;
      l.sounds.push_back(o);
    }
    return;
  }
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
  // Quake II's names for effects Quake has: its sparks off a wall are a gunshot's hit,
  // its blasts an explosion; the BFG's burst is an explosion and its own green burst too.
  const std::string_view name = ev.text;
  if ((name == "te_teleport" || name == "te_boss_teleport") && listen_is_q2(l)) {  // its own burst
    e.kind = EffectKind::named;
    e.name = name == "te_boss_teleport" ? "sq_boss_teleport" : "sq_teleport";
    e.at = e.end = ev.origin;
    l.effects.push_back(std::move(e));
    return;
  }
  // Quake II's own specks: a laser's sparks in its colour; a splash in its kind's (none, sparks,
  // blue water, brown mud, green slime, lava, blood).
  if (listen_is_q2(l) && (name == "te_laser_sparks" || name.starts_with("te_splash") ||
                          name == "te_welding_sparks" || name == "te_tunnel_sparks")) {
    constexpr std::pair<std::string_view, int> kSplash[] = {
        {"te_splash", 0x00},      {"te_splash_sparks", 0xe0}, {"te_splash_water", 0xb0},
        {"te_splash_mud", 0x50},  {"te_splash_slime", 0xd0},  {"te_splash_lava", 0xe0},
        {"te_splash_blood", 0xe8}};
    e.kind = EffectKind::named;
    e.name = name == "te_laser_sparks"     ? "sq_laser_sparks"
             : name == "te_welding_sparks" ? "sq_welding"
             : name == "te_tunnel_sparks"  ? "sq_tunnel_sparks"
                                           : "sq_splash";
    e.at = e.end = ev.origin;
    e.way = {ev.values[0], ev.values[1], ev.values[2]};
    e.count = n >= 1 ? count : 8;  // none asked: the wire's 8
    e.colour = ev.level;
    for (const auto& [splash, c] : kSplash)
      if (name == splash) e.colour = c;
    l.effects.push_back(std::move(e));
    return;
  }
  const bool blast = name == "te_bfg_bigexplosion" || name == "te_bfg_explosion" ||
                     name == "te_rocket_explosion" || name == "te_grenade_explosion" ||
                     name == "te_explosion1" || name == "te_explosion2" ||
                     name == "te_plasma_explosion" || name == "te_plain_explosion" ||
                     name == "te_tracker_explosion" || name == "te_nukeblast" ||
                     name == "te_explosion1_big" || name == "te_explosion1_np";
  if (blast) {
    l.explosions.push_back(ev.origin);
    if (const int code = q2_blast_code(name); code > 0 && q2_look) {
      l.explosion_codes.resize(l.explosions.size());  // its ball, light and specks
      l.explosion_codes.back() = static_cast<uint8_t>(code);
      if (code >= 35) pack_sound(code, ev.origin);  // Ground Zero's: the client's own sound
    }
  }
  if (blast && name != "te_bfg_bigexplosion") return;
  const bool sparks = name == "te_sparks" || name == "te_bullet_sparks" ||
                      name == "te_screen_sparks" || name == "te_shield_sparks" ||
                      name == "te_laser_sparks" || name == "te_shotgun";
  if (uint8_t kind = 0; sv::qw_temp_entity(sparks                    ? "te_gunshot"
                                           : name == "te_greenblood" ? "te_blood"
                                                                     : ev.text,
                                           kind)) {
    if (kind == kExplosion || kind == 4) {
      l.explosions.push_back(ev.origin);
      if (q2_look) {  // our rules' plain blast in Quake II: a rocket's
        l.explosion_codes.resize(l.explosions.size());
        l.explosion_codes.back() = 7;
      }
      return;
    }
    std::vector<uint8_t> b = {kTempEntity, kind};
    if (kind == 2 || kind == 12) b.push_back(static_cast<uint8_t>(count));
    for (float c : {ev.origin.x, ev.origin.y, ev.origin.z})
      sv::encode(b, sv::WriteKind::coord, c, "");
    if (Effect classic; arena_effect(b, classic)) {
      if (const Q2Puff* puff = q2_look ? q2_puff(name) : nullptr) {  // Quake II's specks
        classic.q2_specks = puff->count;
        classic.colour = puff->colour;
        classic.code = puff->code;  // its smoke and flash (q2_model_fx.hpp) by the kind byte
        classic.way = {ev.values[0], ev.values[1], ev.values[2]};
      }
      l.effects.push_back(classic);
    }
    return;
  }
  e.kind = EffectKind::named;
  e.at = e.end = ev.origin;
  e.way = {ev.values[0], ev.values[1], ev.values[2]};
  e.count = std::min(count, 64);
  if (const int code = q2_pack_te_code(name); code > 0) {  // Ground Zero's, by its kind byte
    e.code = code;
    pack_sound(code, ev.origin);
  }
  l.effects.push_back(std::move(e));
}

// The program's own state behind the rules (the reads the rules' world does not carry yet: a
// field by name, the program's statistics); null for another program form.
const sv::QcServer* program(const Listen& l) { return l.rules ? sv::qc_of(*l.rules) : nullptr; }

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
  l.explosion_codes.clear();
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
    if (prints && ev.kind == sv::EventKind::print && (ev.to & 1u) != 0)
      listen_print(l, ev.level, ev.text);
    if (ev.kind == sv::EventKind::centerprint && (ev.to & 1u) != 0) l.centres.push_back(ev.text);
    if (ev.kind == sv::EventKind::stuffcmd && (ev.to & 1u) != 0) {
      if (ev.text == "menu_loadgame\n")
        l.load_menu_asked = true;
      else if (ev.text == "bf\n")  // the pickup flash (the program's on every pickup)
        ++l.bonuses;
      else
        l.stuffed.push_back(ev.text);
    }
    if (ev.kind == sv::EventKind::aim_bend && (ev.to & 1u) != 0)
      sh::aim_turn_bent(l.aim_turn, ev.ent);
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
  // Quake II's world, once a rules frame: an item back throws its glow, a teleporter's pad its
  // rising specks (Quake II's own arrivals, effects.hpp), as a joined client draws them.
  const sv::Q2Server* sq = l.rules != nullptr ? sv::q2_of(*l.rules) : nullptr;
  if (sq == nullptr || sq->frame_number == l.q2_fx_frame) return;
  l.q2_fx_frame = sq->frame_number;
  for (int n = 1; n < s.world.entity_count && size_t(n) < s.world.entities.size(); n++) {
    const sh::WorldEntity& w = s.world.entities[size_t(n)];
    if (w.free) continue;
    const char* name = w.event == 1                                 ? "sq_item_back"
                       : (w.effects & sv::kSqTeleporterEffect) != 0 ? "sq_fountain"
                                                                    : nullptr;
    if (name == nullptr) continue;
    Effect e;
    e.code = -1;
    e.kind = EffectKind::named;
    e.name = name;
    e.at = e.end = w.origin;
    l.effects.push_back(std::move(e));
  }
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
void host_print(void* ctx, int level, const std::string& text) {
  listen_print(*static_cast<Listen*>(ctx), level, text);
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
  if (l.accounts) ge::server::server_accounts_close(*l.accounts);
  l.accounts.reset();
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
int64_t wall_unix_us() {
  return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count());
}

int64_t unix_now(const Listen& l) {
  return l.opt.unix_us != nullptr ? l.opt.unix_us() : wall_unix_us();
}

// The logins on a game others join (`opt.login`): `c` takes the accounts they ask, the window's
// own (opened into `l.accounts`, its count printed) or a master's socket. False with `error`.
bool open_logins(Listen& l, ge::server::ServerConfig& c, std::string& error) {
  c.login = l.opt.login;
  c.unix_us = l.opt.unix_us != nullptr ? l.opt.unix_us : wall_unix_us;
  if (l.opt.accounts != nullptr) {
    c.accounts = l.opt.accounts;
    return true;
  }
  if (c.login.login == 0 && c.login.master.empty()) return true;
  l.accounts = std::make_unique<ge::server::ServerAccounts>();
  std::string said;
  if (!ge::server::server_accounts_open(*l.accounts, c, l.opt.accounts_dir, c.unix_us, said,
                                        error)) {
    l.accounts.reset();
    return false;
  }
  if (l.accounts->hosted)
    say(l, "This game keeps its own player accounts (" +
               std::to_string(l.accounts->host.svc.store.accounts.size()) + " registered).");
  return true;
}

bool start_map(Listen& l, std::string& error) {
  close_match(l);
  if (l.opt.game == nullptr) {
    error = "the game is built wrong: it has no server game for the match";
    return false;
  }
  auto h = std::make_unique<ListenHost>();
  h->progs = l.progs;
  h->module = l.opt.module != nullptr && !l.opt.module->empty() ? l.opt.module : nullptr;
  if (l.opt.native_library != nullptr) h->native_library = *l.opt.native_library;
  h->map = l.map;
  h->files = l.opt.files;
  h->load = std::move(l.load);  // a Quake II's save the first map loads
  h->load_name = l.load_name;
  h->lines = l.opt.item_names;
  h->worn = l.opt.arsenal;
  h->worn_over = l.opt.over_mod;
  h->worn_info = l.opt.remix_info;
  // A mod's module sets its own weapons and physics: classic remix's lines for them
  // (`worn_lines`: cr_items, cr_physics) are left out, the mod's boundaries standing, but for
  // those `cr_over_mods` sets over them (`over_mod`, below).
  if (h->module == nullptr && h->native_library.empty())
    h->lines.insert(h->lines.end(), l.opt.arsenal.begin(), l.opt.arsenal.end());
  h->lines.push_back("sv_rjump 1");  // the window's own server offers the rocket jump helpers
  h->lines.push_back("sv_rrjump 1");
  h->lines.insert(h->lines.end(), l.opt.console.begin(), l.opt.console.end());
  if (h->module != nullptr || !h->native_library.empty())  // `cr_over_mods`: over the mod's own
    h->lines.insert(h->lines.end(), l.opt.over_mod.begin(), l.opt.over_mod.end());
  h->lines.insert(h->lines.end(), l.opt.remix_info.begin(), l.opt.remix_info.end());
  // A lobby is a match's: a single-player or coop game (`deathmatch 0`) starts with the map, its
  // monsters in it, whatever `sv_lobby` says.
  const bool lobby = l.opt.lobby && !wants_campaign(l.opt);
  h->lobby = lobby;
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
  c.lobby = lobby ? 1 : 0;
  c.port = l.opt.port;
  if (l.opt.voice) c.rules.emplace("sv_voip", "1");
  c.host = h.get();
  auto port = std::make_unique<ge::net::Transport>();
  if (l.opt.port > 0 && l.opt.transport == nullptr) {
    const auto at = static_cast<uint16_t>(l.opt.port);
    if (!ge::net::open_udp_transport(*port, ge::net::ipv4(0, 0, 0, 0, at), &error)) {
      error = "the match's port " + std::to_string(l.opt.port) + " did not open: " + error +
              ". Pick another port, or 0 to play alone.";
      return false;
    }
  }
  if ((l.opt.port > 0 || l.opt.transport != nullptr) && !open_logins(l, c, error)) {
    if (l.opt.transport == nullptr) ge::net::close_transport(*port);
    return false;
  }
  auto server = std::make_unique<ge::server::Server>();
  l.host = std::move(h);
  l.port = std::move(port);
  ge::net::Transport* socket = l.opt.transport != nullptr ? l.opt.transport : l.port.get();
  if (!ge::server::server_open(*server, *l.opt.game, c, socket, nullptr, error)) {
    error = "the map " + l.map->name + " did not load: " + error;
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
      error = "the game is built wrong: it has no files to read maps/" + name + ".bsp from";
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
    if (!error.empty()) say(l, error);
  }
  entered(l);
  // The console's banner for the level: a rule and its title (the window keeps a line that starts
  // with a blank one out of the HUD's feed).
  say(l, "\n" + std::string(38, '-') + "\n\n" + map_title(l) + "\n");
}

bool coop_on(const Listen& l) {
  const ge::console::Cvar* v = l.server ? l.server->cvars.find("coop") : nullptr;
  return v != nullptr && v->on();
}

// A companion seated now (in a campaign; a single-player one turns cooperative for it); false with
// the line printed when it cannot be.
bool seat_companion(Listen& l, sb::AimSkill skill, bool helper = false) {
  const bool turned = !coop_on(l);
  if (turned) {
    sv::campaign_set_coop(*l.campaign_host, true);
    if (!coop_on(l)) {
      say(l, "This game has no room for a companion.");
      return false;
    }
  }
  if (!l.party) {
    l.party = std::make_unique<Party>();
    l.party->seed = l.opt.seed;
  }
  std::string error;
  const int slot = party_add(*l.party, *l.campaign_host, l.map->spec, skill, error, helper);
  if (slot < 0) {
    if (turned) sv::campaign_set_coop(*l.campaign_host, false);
    say(l, "addbot: " + error);
    return false;
  }
  const std::string& name = l.party->mates[static_cast<size_t>(slot)]->name;
  if (l.door != nullptr && !l.door_calls->seat(l.door, slot, name, true, error)) {
    party_remove(*l.party, *l.campaign_host);
    if (turned) sv::campaign_set_coop(*l.campaign_host, false);
    say(l, "addbot: " + error);
    return false;
  }
  if (turned) {
    l.companion_coop = true;
    say(l, "The game is cooperative while you have company.");
  }
  say(l, name + " (" + sb::aim_skill_name(skill) + (helper ? ", helper" : "") + ") joins the game");
  return true;
}

// A companion let go (`removebot`); the last one leaving a game that turned cooperative for the
// companions makes it single player again.
void let_companion_go(Listen& l) {
  int last = 0;
  for (int i = 1; l.party && i < sh::kMaxClients; i++)
    if (l.party->mates[static_cast<size_t>(i)]) last = i;
  if (!l.party || !party_remove(*l.party, *l.campaign_host)) {
    say(l, "removebot: no companion is seated");
    return;
  }
  if (l.door != nullptr) l.door_calls->unseat(l.door, last);
  if (l.companion_coop && party_size(*l.party) == 0) {
    sv::campaign_set_coop(*l.campaign_host, false);
    l.companion_coop = false;
    say(l, "You play alone again.");
  }
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
    co.wide_models = true;  // a game's own maps: the bounds its program sizes its movers by
  }
  co.registered = opt.registered;
  co.episodes = opt.episodes;
  co.settings = opt.arsenal;
  co.settings.push_back("sv_rjump 1");  // the rocket jump helpers, as on the window's match
  co.settings.push_back("sv_rrjump 1");
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
      e.colour = static_cast<int>(ev.values[0]);
      e.kind = ev.text == "explosion"  ? EffectKind::explosion
               : ev.text == "particle" ? EffectKind::burst
                                       : EffectKind::blood;
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
      if (l.effects[i].kind == EffectKind::explosion && l.effects[i].code != 12)  // 12: its own
        l.explosions.push_back(l.effects[i].at);
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
  if (l.accounts) ge::server::server_accounts_close(*l.accounts);
  l.accounts.reset();
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
      say(l, "coop: the port " + std::to_string(o.port) + " did not open: " + error +
                 ". Pick another sv_port, or 0 to play alone.");
      return;
    }
  }
  l.port = std::move(port);
  ge::net::Transport& t = o.transport != nullptr ? *o.transport : *l.port;
  l.door_calls = o.door;
  l.door =
      o.door->open(*l.campaign_host, t, o.hostname, o.seed, kPartyMost + 1, door_tap, &l, error);
  if (l.door == nullptr) {
    say(l, "coop: others cannot join: " + error);
    return;
  }
  // The logins on the coop game, as on a match.
  ge::server::ServerConfig c;
  if (l.door_calls->logins != nullptr &&
      (!open_logins(l, c, error) || ((c.accounts != nullptr || c.account_link != nullptr) &&
                                     !l.door_calls->logins(l.door, c, error))))
    say(l, "coop: logins are off: " + error);
  l.net_s = 0;
  bool seated = l.door_calls->seat(l.door, 0, o.player, false, error);
  for (int i = 1; seated && l.party && i < sh::kMaxClients; i++)
    if (const auto& m = l.party->mates[static_cast<size_t>(i)])
      seated = l.door_calls->seat(l.door, i, m->name, true, error);
  if (!seated) {
    say(l, "coop: others cannot join: " + error);
    close_door(l);
    return;
  }
  say(l, "coop: others may join on port " + std::to_string(o.port) + "\n");
}

}  // namespace

bool baseline_progs(pg::Progs& out, std::string& error) { return sv::baseline_mod(out, error); }

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
            " or its packs. Check the name and the game folder (\"map arena\" is built in).";
    return nullptr;
  }
  return map_from_bytes(bytes, name, error);
}

bool listen_start(Listen& l, const pg::Progs& progs, std::unique_ptr<LoadedMap> map,
                  const ListenOptions& opt, std::string& error) {
  if (single_player(progs)) return listen_campaign(l, progs, std::move(map), opt, error);
  l.party.reset();
  l.companion_coop = false;
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

void listen_farewell(Listen& l, const std::string& reason, bool restart) {
  if (!l.match || l.match->game == nullptr || l.match->game->quit == nullptr) return;
  if (l.match->state != nullptr) l.match->game->quit(l.match->state, reason, restart);
}

bool listen_net_restart(Listen& l, int& port, std::string& said) {
  if (!l.match || !l.port || l.opt.transport != nullptr || l.port->local.port == 0) {
    said = "No game is hosted for others here, so there is no connection to open again.\n";
    return false;
  }
  const int good = l.port->local.port;
  std::string ip;
  if (!ge::server::net_restart(*l.port, ip, port, "", good, said)) return false;
  const ge::server::ServerGame* game = l.match->game;
  if (game != nullptr && game->reopened != nullptr && l.match->state != nullptr)
    game->reopened(l.match->state, *l.port);
  return true;
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
      say(l, "coop: others cannot join: " + error);
      close_door(l);
    }
    campaign_entered(l);
    l.map_changed = false;
    return true;
  }
  // No match yet, or one of the other kind (a deathmatch's lobby while `deathmatch 0` now asks
  // for single player or coop, or the other way), or one on the remix's old items, files or
  // moves (`cr_items` changed since it started): the match started afresh on the map.
  if (!l.match || l.host->lobby != (l.opt.lobby && !wants_campaign(l.opt)) ||
      l.host->worn != l.opt.arsenal || l.host->worn_over != l.opt.over_mod ||
      l.host->worn_info != l.opt.remix_info || l.host->files != l.opt.files) {
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
  l.companion_coop = false;
  campaign_entered(l);
  for (int k = 0; k < opt.companions && k < kPartyMost; k++)
    if (!seat_companion(l, opt.companion_skill)) break;
  open_door(l);
  l.map_changed = false;
  return true;
}

namespace {

// The table of the rules running, or nullptr.
const sv::RunnerCalls* runner_calls(const Listen& l) {
  return l.rules != nullptr ? l.rules->calls : nullptr;
}

}  // namespace

std::string listen_save_problem(const Listen& l) {
  if (const sv::RunnerCalls* c = runner_calls(l); c != nullptr && c->save_problem != nullptr)
    return c->save_problem(l.rules->rules);
  if (!l.campaign_host || l.server == nullptr) return "Only a single-player game can be saved.";
  if (l.campaign.end != LevelEnd::none) return "Can't save now: the level is over.";
  if (l.companion_coop && l.party && party_size(*l.party) > 0)
    return "Can't save while a companion is with you. Send it away with removebot, then save.";
  return sv::campaign_save_problem(*l.campaign_host);
}

bool listen_save(const Listen& l, std::vector<uint8_t>& out, std::string& error,
                 int64_t saved_unix) {
  error = listen_save_problem(l);
  if (!error.empty()) {
    error = "save: " + error;
    return false;
  }
  if (const sv::RunnerCalls* c = runner_calls(l); c != nullptr && c->save != nullptr)
    return c->save(l.rules->rules, out, error, saved_unix);
  return sv::campaign_save(*l.campaign_host, out, error, saved_unix);
}

bool listen_load_q2(Listen& l, const ListenOptions& opt, std::span<const uint8_t> bytes,
                    std::string_view name, std::string& error) {
  std::string map_name = sv::q2_save_map(bytes, name, error);
  if (map_name.empty()) {  // a save of a mod's game library's game
    std::string why;
    map_name = sv::native_q2_save_map(bytes, name, why);
    if (map_name.empty()) return false;
    error.clear();
  }
  if (opt.files == nullptr) {
    error = "the game is built wrong: it has no files to read maps/" + map_name + ".bsp from";
    return false;
  }
  std::unique_ptr<LoadedMap> map = load_named_map(*opt.files, map_name, error);
  if (!map) return false;
  if (l.match && l.host && listen_is_q2(l)) {  // onto Quake II's game playing
    l.host->load.assign(bytes.begin(), bytes.end());
    l.host->load_name = name;
    const bool ok = listen_change(l, std::move(map), error);
    l.host->load.clear();
    if (!ok) return false;
  } else {  // a new game on the save's map
    l.party.reset();
    l.companion_coop = false;
    close_match(l);
    l.campaign_host.reset();
    l.opt = opt;
    l.map = std::move(map);
    l.load.assign(bytes.begin(), bytes.end());
    l.load_name = name;
    const bool ok = start_map(l, error);
    l.load.clear();
    if (!ok) return false;
  }
  l.map_changed = true;  // the window draws the save's map
  if (const sv::RunnerCalls* c = runner_calls(l); c != nullptr && c->last_sequence != nullptr)
    l.sequence = std::max(l.sequence, c->last_sequence(l.rules->rules, 0));  // his commands go on
  return true;
}

std::vector<uint8_t> listen_take_autosave(Listen& l) {
  const sv::RunnerCalls* c = runner_calls(l);
  if (c == nullptr || c->take_autosave == nullptr) return {};
  return c->take_autosave(l.rules->rules);
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
      error = "the game is built wrong: it has no files to read maps/" + name + ".bsp from";
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
  l.companion_coop = false;
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
  return sv::q1_save_text(*l.campaign_host, out, error);
}

bool listen_load_text(Listen& l, const pg::Progs& progs, const ListenOptions& opt,
                      std::string_view text, std::string_view name, std::string& error,
                      std::unique_ptr<LoadedMap> map) {
  if (!load_game(l, progs, opt, std::move(map),
                 [&](sv::Campaign& c, const sv::CampaignOptions& co) {
                   return sv::q1_load_text(c, progs, co, text, name, opt.player, error);
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
  if (name.ends_with(".spr")) {  // a sprite: half its widest frame across, its tallest up and down
    if (bytes.size() < 24 || std::memcmp(bytes.data(), "IDSP", 4) != 0) return false;
    const auto word = [&bytes](size_t at) {
      return static_cast<int32_t>(static_cast<uint32_t>(bytes[at]) | uint32_t{bytes[at + 1]} << 8 |
                                  uint32_t{bytes[at + 2]} << 16 | uint32_t{bytes[at + 3]} << 24);
    };
    const float across = static_cast<float>(word(16)) / 2, up = static_cast<float>(word(20)) / 2;
    mins = {-across, -across, -up};
    maxs = {across, across, up};
    return true;
  }
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

constexpr int kMuzzleFlashBit = 2;  // the entity's muzzle-flash effect bit

void listen_frame(Listen& l, sh::MoveCmd cmd) {
  GE_ZONE("listen.frame");
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
    // The move's air law: the classic one, unless rules that keep their own players say theirs.
    sh::SpeedJumpRules air = sh::kClassicJumpRules;
    if (l.rules != nullptr && l.rules->calls != nullptr && l.rules->calls->aid != nullptr)
      l.rules->calls->aid(l.rules->rules, 0, s, air);
    // His own share when the server granted him one (`*speedjump`, net/qw_admin.hpp).
    const std::string granted = sv::info_value(l.server->clients[0].userinfo, "*speedjump");
    const float share = granted.empty() ? l.server->sv.set.move.speedjump
                                        : static_cast<float>(std::atof(granted.c_str()));
    cmd = sh::assisted_command(cmd, s, share, l.speedjump_view, l.jump_aid, air);
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
      if (l.accounts) ge::server::server_accounts_update(*l.accounts, unix_now(l));
      if (!l.door_calls->frame(l.door, sent, count, now_us, l.failed)) {
        close_door(l);
        l.rules = nullptr;
        l.server = nullptr;
        return;
      }
      if (l.campaign_host->history.size() != l.maps_started) campaign_entered(l);
    } else {
      // A gun's flash lasts the one frame it was fired in: the program sets the bit as the
      // monster shoots and leaves it for the server to clear once the frame is out, so it is
      // cleared here before the next (else a soldier would seem to fire on every frame after).
      if (l.server != nullptr && l.rules != nullptr && sv::qc_of(*l.rules) != nullptr)
        for (int e = 1; e < l.server->sv.num_edicts; e++) {
          if (l.server->sv.edicts[size_t(e)].free) continue;
          const int fx = static_cast<int>(field_named(l, e, "effects"));
          if ((fx & kMuzzleFlashBit) != 0)
            set_field_named(l, e, "effects", static_cast<float>(fx & ~kMuzzleFlashBit));
        }
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
    if (l.accounts) {
      ge::server::server_accounts_update(*l.accounts, unix_now(l));
      ge::server::server_accounts_reports(*l.accounts, *l.match, unix_now(l));
    }
    if (catch_up(l)) entered(l);  // the lobby's restart into the match: a new server
  }
  sv::RulesCommon& s = *l.server;
  l.frames++;
  if (l.campaign.turned) {
    l.view = {l.campaign.angles.x, l.campaign.angles.y, 0};
    l.campaign.turned = false;
  }
  // The mod's forced view (a spawn, a teleport) leaves the player's angles off the ones sent: by
  // more than Quake II's rules round a sent angle to (65536ths of a turn, kept within -180..180),
  // so a view they only rounded stays the player's own (and his easy jump's aim with it).
  const sh::Edict& me = s.sv.edicts[1];
  const auto moved = [](float a, float b) {
    double d = static_cast<double>(a) - b;
    while (d > 180) d -= 360;
    while (d < -180) d += 360;
    return d > 0.01 || d < -0.01;
  };
  l.forced = s.clients[0].active &&
             (moved(me.v_angle.x, cmd.angles.x) || moved(me.v_angle.y, cmd.angles.y));
  if (l.forced) l.view = {me.v_angle.x, me.v_angle.y, 0};
}

bool listen_lobby(Listen& l, std::string_view line) {
  const std::string_view word = line.substr(0, line.find(' '));
  const bool bot_word = word == "addbot" || word == "removebot";
  if (bot_word && l.campaign_host && l.server != nullptr) {  // a campaign: the coop party
    if (word == "removebot") {
      let_companion_go(l);
      return true;
    }
    const size_t sp = line.find(' ');
    const std::string arg = sp == std::string_view::npos ? "" : std::string(line.substr(sp + 1));
    sb::AimSkill skill = l.opt.companion_skill;
    bool helper = false;
    std::string bad;
    if (!companion_words(arg, skill, helper, bad)) {
      say(l, "addbot: unknown word \"" + bad +
                 "\" (use novice, average or expert, and escort or helper)");
      return true;
    }
    seat_companion(l, skill, helper);
    return true;
  }
  // A vote on the settings (engine/server/vote.hpp): the server's own words, with or without a
  // lobby.
  if ((word == "vote" || word == "yes" || word == "no") && !l.campaign_host && l.host &&
      l.host->words != nullptr && l.server != nullptr) {
    l.host->ctx = &l;
    const bool taken = l.host->words(l.host->game, std::string(line));
    catch_up(l);
    return taken;
  }
  // Quake II's own single-player or coop game: its companions (client/q2_party.hpp), which
  // the game seats on its rules.
  if (bot_word && !l.room && l.rules != nullptr && sv::q2_of(*l.rules) != nullptr &&
      l.host != nullptr && l.host->words != nullptr && l.host->game != nullptr) {
    l.host->ctx = &l;
    const bool taken = l.host->words(l.host->game, std::string(line));
    catch_up(l);
    if (taken) return true;
  }
  // A deathmatch with no lobby: its first `addbot` opens the room the bots are seated in.
  if (word == "addbot" && !l.room && l.server != nullptr && l.host != nullptr &&
      l.host->words != nullptr && l.host->game != nullptr) {
    l.host->ctx = &l;
    const bool taken = l.host->words(l.host->game, std::string(line));
    catch_up(l);
    if (taken) return true;
  }
  if (!l.room || l.server == nullptr) {
    if (bot_word || word == "ready" || word == "break" || word == "notready" ||
        word == "toggleready" || word == "team" || word == "start" || word == "lobby" ||
        word == "warmup" || word == "sitout" || word == "togglewarmup") {
      say(l, std::string(word) + ": there is no lobby. Lobbies run in deathmatch with sv_lobby 1.");
      return true;
    }
    return false;
  }
  if (word == "addbot" && l.host->lobby && l.room->lobby.phase == sv::LobbyPhase::match) {
    say(l, "addbot: the match has started. Add bots during the warmup.");
    return true;
  }
  l.host->ctx = &l;
  const bool taken = l.host->words(l.host->game, std::string(line));
  catch_up(l);
  return taken;
}

bool listen_is_q2(const Listen& l) { return l.rules != nullptr && sv::q2_of(*l.rules) != nullptr; }

bool listen_q2_arms(const Listen& l) {
  if (l.rules == nullptr || listen_is_q2(l)) return false;
  for (const std::string& line : l.opt.arsenal)
    if (line.starts_with("set arsenal_weapon") && line.find("q2_") != std::string::npos)
      return true;
  return false;
}

bool listen_own_screens(const Listen& l) {
  return l.rules != nullptr && l.rules->calls != nullptr && l.rules->calls->own_screens;
}

bool listen_say(Listen& l, const std::string& line) {
  if (l.rules == nullptr || l.server == nullptr) return false;
  l.rules->client_command(0, line);
  return true;
}

bool listen_rules_word(Listen& l, const std::string& line) {
  if (l.rules == nullptr || l.server == nullptr || l.rules->calls == nullptr ||
      !l.rules->calls->own_words)
    return false;
  return l.rules->client_command(0, line) != sv::CommandResult::bad;
}

int listen_players(const Listen& l) {
  if (l.server == nullptr) return 0;
  int n = 0;
  for (const sv::ClientInfo& c : l.server->clients) n += c.active && !c.spectator && !c.bot ? 1 : 0;
  return n;
}

bool listen_campaign_word(Listen& l, const std::string& line) {
  if (l.rules == nullptr || l.server == nullptr) return false;
  const std::string word = line.substr(0, line.find_first_of(" \t"));
  // Quake's `tell` and `ping` always go to the server, a listen server too.
  if (l.server->sv.set.single_player_clock && (word == "tell" || word == "ping")) {
    l.rules->client_command(0, line);
    return true;
  }
  if (l.campaign_host == nullptr) return false;
  for (const char* w : {"god", "notarget", "noclip", "fly", "give", "kill", "pause"})
    if (word == w) {
      l.rules->client_command(0, line);
      return true;
    }
  if (word == "restart" || word == "changelevel") {
    l.rules->console(line);
    return true;
  }
  return false;
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

bool listen_command(Listen& l, const std::string& line) {
  if (!l.match || !l.host || l.host->command == nullptr || l.host->game == nullptr) return false;
  l.host->command(l.host->game, line);
  catch_up(l);
  return true;
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
  GE_ZONE("listen");  // the server frames due, the bots' thinking in them
  const double tic = listen_tic(l);
  l.owed += wall_s;
  int n = 0;
  while (l.owed >= tic && n < kMostFrames && l.pending_map.empty() && l.cinematic.empty() &&
         l.server != nullptr) {
    l.owed -= tic;
    listen_frame(l, cmd);
    n++;
    if (sv::Q2Server* sq = l.rules != nullptr ? sv::q2_of(*l.rules) : nullptr)
      l.cinematic = std::exchange(sq->cinematic, {});  // the world waits while it plays
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
  p.noclip = me.movetype == sh::kMovetypeNoclip;
  p.view_ofs = me.view_ofs;
  if (const sv::PlayerNumbers& n = l.server->world.clients[0].numbers; n.view_set) {
    p.rules_view = true;
    p.view_ofs = {n.view_offset[0], n.view_offset[1], n.view_offset[2]};
    p.kick = {n.kick[0], n.kick[1], n.kick[2]};
    p.fov = n.fov;
  }
  if (const sv::Q2Server* sq = l.rules != nullptr ? sv::q2_of(*l.rules) : nullptr) {
    p.rules_frame = sq->frame_number;
    p.rules_age = sq->host_time - sq->level_time;
    p.pushed = sq->players[0].pushed;
  }
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

bool aim_turn_points(const Listen& l, int target, sh::Vec3f& eye, sh::Vec3f& middle) {
  const sv::RulesWorld& w = l.server->world;
  if (w.entities.size() < 2) return false;
  const sv::WorldEntity& me = w.entities[1];
  const float view_z = w.clients[0].numbers.view_z;
  eye = me.origin + sh::Vec3f{0, 0, view_z != 0 ? view_z : 22};
  if (target < 1 || static_cast<size_t>(target) >= w.entities.size()) return false;
  const sv::WorldEntity& t = w.entities[static_cast<size_t>(target)];
  if (t.free || t.health <= 0) return false;
  middle = (t.absmin + t.absmax) * 0.5f;
  return true;
}

void listen_area_bits(const Listen& l, std::vector<uint8_t>& out) {
  out.clear();
  const sv::Q2Server* sq = l.rules != nullptr ? sv::q2_of(*l.rules) : nullptr;
  if (sq == nullptr || l.server == nullptr || l.server->world.entities.size() < 2) return;
  const sv::RulesWorld& w = l.server->world;
  const float view_z = w.clients[0].numbers.view_z;
  sv::map_area_bits(*sq, w.entities[1].origin + sh::Vec3f{0, 0, view_z != 0 ? view_z : 22}, out);
}

void missiles_of(const Listen& l, std::vector<Missile>& out) {
  out.clear();
  // The shots in flight as the rules' world names them, whatever runs the rules (a player's or a
  // monster's alike).
  if (l.rules == nullptr || !*l.rules) return;
  const std::vector<sv::WorldEntity>& rows = (*l.rules)->world.entities;
  for (size_t e = 0; e < rows.size(); e++) {
    const sv::WorldEntity& r = rows[e];
    if (r.free) continue;
    Missile o;
    if (r.shot == sv::WorldShot::none) {  // Quake II's marks by their bits (a game library's)
      if (!listen_is_q2(l)) continue;
      const Q2Look look = q2_look(static_cast<uint32_t>(r.effects), 0);
      const Q2Look::Missile k = look.missile;
      if (k < Q2Look::Missile::flag1 && !look.spinning) continue;
      o.entity = static_cast<int>(e);
      o.origin = r.origin;
      if (k >= Q2Look::Missile::flag1) o.q2_mark = static_cast<uint8_t>(k);
      o.q2_spin = look.spinning;
      o.yaw = r.angles.y;
      out.push_back(o);
      continue;
    }
    o.entity = static_cast<int>(e);
    o.origin = r.origin;
    o.rocket = r.shot == sv::WorldShot::rocket || r.shot == sv::WorldShot::grenade;
    o.grenade = r.shot == sv::WorldShot::grenade;
    o.bolt = r.shot == sv::WorldShot::bolt;
    o.bfg = r.shot == sv::WorldShot::ball;
    o.streak = o.bolt && (r.effects & 8) != 0 && (listen_is_q2(l) || listen_q2_arms(l));
    out.push_back(o);
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

bool vector_field_named(const Listen& l, int entity, const char* name, float out[3]) {
  const pg::Def* d = pg::find_field(*l.progs, name);
  if (d == nullptr) return false;
  const sv::QcServer* q = program(l);
  if (q == nullptr) return false;
  const uint32_t* v = pg::entity_field(q->q.vm, entity, d->offset);
  for (int k = 0; k < 3; k++) out[k] = pg::as_float(v[k]);
  return true;
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
