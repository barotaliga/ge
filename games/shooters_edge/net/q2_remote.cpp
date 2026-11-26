// The window joined to a server of Quake II (q2_remote.hpp).
//
// ge:summary The window joined to a server of Quake II: start, step, hands, frames in between
// ge:keywords q2-join remote-client lerp delta-angles use-weapon
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_test.cpp
#include "shooters_edge/net/q2_remote.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <sstream>
#include <utility>

#include "classic/q2_move.hpp"
#include "shooter/speedjump_assist.hpp"
#include "shooters_edge/client/cvar_layers.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/qw_remote_rulesets.hpp"
#include "shooters_edge/server/q2_combat.hpp"

namespace shooters_edge::net {

namespace {

constexpr uint8_t kQ2ButtonUse = 2;  // Quake II's use button in a move's buttons byte

float angle16(int16_t a) { return static_cast<float>(a) * (360.0f / 65536.0f); }

// Quake II's figure and skin as one word ("male/grunt"): a skin with a slash as it is, else the
// model's folder and the skin; a classic skin alone names none of its figures, so the stock one.
std::string q2_skin_word(const std::string& model, const std::string& skin) {
  if (skin.find('/') != std::string::npos) return skin;
  return !model.empty() ? model + "/" + skin : "male/grunt";
}

// The player's settings as Quake II's user info.
Q2ClientConfig q2_config(Remote& r, const ClientConfig& cfg) {
  Q2ClientConfig s;
  s.name = cfg.name;
  s.skin = q2_skin_word(cfg.model, cfg.skin);
  s.stuff = &r.stuff;
  s.msg = cfg.msg;
  s.rate = cfg.rate;
  s.fov = cfg.fov;
  s.hand = cfg.hand;
  s.gender = cfg.gender_auto ? client::auto_gender(s.skin) : cfg.gender;
  s.speedjump = cfg.speedjump;
  s.ruleset = cfg.ruleset;
  s.ask_login = true;   // our server's logins' key, to log in by an account file
  s.ask_feel = true;    // the feel our server runs the move on, to predict the same
  s.ask_frames = true;  // every frame our server's rules run (`sv_fps`), not only the tenths
  s.voice = cfg.voice;  // cl_voip: voice on our server, as Quake's join asks for it
  s.live_mic = cfg.live_mic;
  s.voice_to = cfg.voice_to;
  s.spectator = cfg.spectator;
  s.password = cfg.password;
  s.qport = cfg.qport;
  s.timeout = cfg.timeout;  // cl_timeout (0: never)
  s.trace = cfg.trace;      // showpackets, showdrop
  s.into = &r.demo;
  // The files the level names that the window lacks, fetched before the game begins and kept
  // in the archive the server says each came out of (the window's `keep_file`).
  s.fetch = true;
  s.fetch_level = true;
  s.allow = cfg.q2_downloads;    // allow_download and its kinds
  s.http = cfg.web;              // cl_http_*: the files over HTTP first
  s.nodelta = cfg.q2_nodelta;    // cl_nodelta
  s.protocol = cfg.q2_protocol;  // cl_protocol and the later protocols' settings
  s.msglen = cfg.q2_msglen;
  s.chantype = cfg.q2_chantype;
  s.batch = cfg.q2_batch;
  s.packetdup = cfg.q2_packetdup;
  s.maxpackets = cfg.q2_maxpackets;
  s.instantpacket = cfg.q2_instantpacket;
  s.ask_moves = true;    // the other players' moves, to run them forward (q2_predict.hpp)
  s.ask_back = true;     // the items coming back, drawn ahead of time (ce_respawns)
  s.ask_hits = true;     // the damage dealt, the hit marker's (cl_hit_markers)
  s.ask_looks = true;    // Quake's looks of Quake's weapons (classic remix)
  s.ask_sheet = true;    // the settings sheet, the server settings page's
  s.ask_arsenal = true;  // the weapons held and the ammunition, a weapon change predicted
  s.read_file = [&r](const std::string& path) {
    return r.read_file ? r.read_file("", path) : std::vector<uint8_t>{};
  };
  s.setting = r.setting;  // a server's setting bans ask the console's (server/settingban.hpp)
  s.keep_file = [&r](const std::string& path, const std::vector<uint8_t>& bytes,
                     const ge::vfs::ArchiveOrigin* origin) {
    if (r.keep_file) r.keep_file("", path, bytes, origin);
  };
  s.held = r.held;  // a download cut short resumed
  s.brain = [&r](int) {
    sh::MoveCmd m = r.hands;
    m.angles = r.view;
    m.impulse = 0;  // a weapon key went as `use` (below)
    // The easy jump (shooter/speedjump_assist.hpp) off the newest prediction, by Quake II's air
    // law at the air acceleration the server states: at the share our server told (svc 126),
    // else kStockShare on a stock server, which tells none.
    if (r.q2 != nullptr && r.q2->cfg.speedjump && r.predicted) {
      const float told = r.demo.speedjump_share;
      const float share = told >= 0 ? told : kStockShare;
      // The aid reads the way in the world: the hands' angles plus the server's turn of him.
      sh::Vec3f turn;
      if (r.demo.last_frame >= 0) {
        const client::Q2View& v = r.demo.views[static_cast<size_t>(r.demo.last_frame & 15)];
        turn = {angle16(v.delta[0]), angle16(v.delta[1]), angle16(v.delta[2])};
      }
      m.angles = m.angles + turn;
      m = sh::assisted_command(m, r.ahead, share, r.speedjump_view, r.jump_aid,
                               sh::q2_jump_rules(q2_feel_of(r.demo).air_accelerate));
      m.angles = m.angles - turn;
    }
    // Quake II's jump is an up move, never a button (the button's bit is its `use`).
    if ((m.buttons & sh::kButtonJump) != 0) {
      m.up = std::max<int16_t>(m.up, ge::classic::kSqJumpUp);
      m.buttons = static_cast<uint8_t>(m.buttons & ~sh::kButtonJump);
    }
    if ((m.buttons & sh::kButtonUse) != 0)  // +use is Quake II's second button
      m.buttons = static_cast<uint8_t>((m.buttons & ~sh::kButtonUse) | kQ2ButtonUse);
    return m;
  };
  return s;
}

// `old` and `now` drawn between at `t` (0 the old, 1 the new): those present in both with the same
// model, moved less than kLerpMost.
int lerp_list(std::vector<client::DemoEntity>& drawn, const std::vector<client::DemoEntity>& old,
              const std::vector<client::DemoEntity>& now, float t) {
  int n = 0;
  const size_t count = std::min({drawn.size(), old.size(), now.size()});
  for (size_t i = 0; i < count; i++) {
    const client::DemoEntity& a = old[i];
    const client::DemoEntity& b = now[i];
    if (!a.present || !b.present || a.model != b.model || !drawn[i].present) continue;
    const sh::Vec3f d = b.origin - a.origin;
    if (sh::length(d) > kLerpMost || (d.x == 0 && d.y == 0 && d.z == 0)) continue;
    drawn[i].origin = a.origin + d * t;
    n++;
  }
  return n;
}

// THE PLAYER'S OWN MISSILES THROWN AHEAD (client/own_missiles.hpp, `cl_predict_projectiles`): each
// bolt, grenade, hand grenade, rocket or BFG ball the weapon pass fired leaves from the player's
// drawn place along the view his move sent (the server's turn added), the side by his `hand`; then
// every stand-in flies to `now_s` through the level's brushes under the player's gravity, is handed
// to the server's copy on its path and is drawn into the frame. The server moves nothing ahead, so
// its copy is a round trip late (`ping` the round trip itself).
void q2_own_missiles(Remote& r, client::DemoState& st, double now_s, const sh::Vec3f& turn,
                     const sh::Vec3f& eye_up) {
  const Q2ClientEnd& c = *r.q2;
  if (r.missiles_level != st.levels) {
    client::own_missiles_clear(r.missiles);
    r.missiles_level = st.levels;
  }
  const int me = st.player;
  const bool placed = me >= 0 && static_cast<size_t>(me) < st.players.size();
  const auto ping = static_cast<int>(std::lround(c.latency * 1000));
  for (const Q2Weapon::Fired& f : r.q2_weapon.fired) {
    if (!placed) break;
    client::OwnShot shot;
    switch (static_cast<server::SqWeapon>(f.weapon)) {
      case server::SqWeapon::blaster:
      case server::SqWeapon::hyperblaster:
        shot.kind = client::OwnMissileKind::q2_bolt;
        break;
      case server::SqWeapon::grenade_launcher:
        shot.kind = client::OwnMissileKind::q2_grenade;
        break;
      case server::SqWeapon::rocket_launcher:
        shot.kind = client::OwnMissileKind::q2_rocket;
        break;
      case server::SqWeapon::bfg:
        shot.kind = client::OwnMissileKind::q2_bfg;
        break;
      case server::SqWeapon::hand_grenade:  // thrown, not gone off in the hand
        if (f.speed <= 0) continue;
        shot.kind = client::OwnMissileKind::q2_hand_grenade;
        shot.speed = f.speed;
        break;
      default:
        continue;
    }
    const auto shot_kind = static_cast<size_t>(server::kSqWeapons[f.weapon].shot);
    shot.effects = server::kSqShotEffects[shot_kind];
    if (static_cast<server::SqWeapon>(f.weapon) == server::SqWeapon::hyperblaster)
      shot.effects = f.frame == 6 || f.frame == 9 ? 64 : 0;  // its 7th and 10th frames glow
    shot.origin = st.players[static_cast<size_t>(me)].origin;
    shot.view_height = eye_up.z;
    shot.angles = c.sent[f.move & (kSqSentRing - 1)].angles + turn;
    shot.side = c.cfg.hand == 1 ? -1 : c.cfg.hand == 2 ? 0 : 1;
    shot.age_s = f.age_s;
    client::own_missile_fire(r.missiles, shot, now_s, c.latency, ping, shot.origin);
  }
  const int gravity = r.demo.last_frame >= 0
                          ? r.demo.views[static_cast<size_t>(r.demo.last_frame & 15)].gravity
                          : 800;
  client::own_missiles_step_brushes(r.missiles, r.q2_ahead.tracer.get(),
                                    static_cast<float>(gravity), now_s);
  // The copies hidden last step shown as the newest frame has them before they are matched again.
  const auto restore = [&](int n) {
    if (n > 0 && static_cast<size_t>(n) < r.ents_new.size() &&
        static_cast<size_t>(n) < st.entities.size())
      st.entities[static_cast<size_t>(n)].present = r.ents_new[static_cast<size_t>(n)].present;
  };
  for (const client::OwnMissile& o : r.missiles.flying) restore(o.adopted);
  for (const int n : r.missiles.hidden) restore(n);
  client::own_missiles_handover(r.missiles, st);
  client::own_missiles_draw(r.missiles, st, now_s);
}

}  // namespace

bool q2_user_info_moved(Q2ClientConfig& q,
                        const std::function<std::string(const std::string&)>& setting) {
  Q2ClientConfig was = q;
  const auto text = [&setting](const char* name) { return setting(name); };
  if (const std::string n = text("name"); !n.empty()) q.name = n;
  if (const std::string s = text("skin"); !s.empty()) q.skin = q2_skin_word(text("model"), s);
  if (const std::string h = text("hand"); !h.empty()) q.hand = std::atoi(h.c_str());
  if (const std::string f = text("cl_fov"); !f.empty()) {
    const double fov = std::atof(f.c_str());
    q.fov = fov > 0 ? static_cast<int>(std::lround(fov)) : 90;
  }
  if (const std::string m = text("msg"); !m.empty()) q.msg = std::atoi(m.c_str());
  if (const std::string g = text("gender"); !g.empty())
    q.gender = text("gender_auto") == "1" ? client::auto_gender(q.skin) : g;
  if (const std::string rate = text("rate"); std::atoi(rate.c_str()) > 0)
    q.rate = std::atoi(rate.c_str());
  q.password = text("password");
  return q.name != was.name || q.skin != was.skin || q.hand != was.hand || q.fov != was.fov ||
         q.msg != was.msg || q.gender != was.gender || q.rate != was.rate ||
         q.password != was.password;
}

void q2_remote_start(Remote& r, const ClientConfig& cfg) {
  r.q2 = std::make_unique<Q2ClientEnd>();
  if (!r.stuff_file.empty()) {  // the whitelist read again at each join
    std::ifstream in(r.stuff_file, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    r.stuff.patterns = ge::classic::read_stuff_whitelist(text.str());
  }
  q2_client_start(*r.q2, *r.transport, r.server, q2_config(r, cfg), 0);
  remote_record_restart(r);  // the level kept for `record` (qw_match_record.hpp)
  r.active = true;
  r.now_us = 0;
  r.owed_s = 0;
  r.levels = 0;
  r.said = ClientStage::challenge;
  r.why.clear();
  r.world = nullptr;
  r.predicted = false;
  r.jump_aid = {};
  r.lerped = 0;
  r.ents_old.clear();
  r.ents_new.clear();
  r.players_old.clear();
  r.players_new.clear();
  r.frame_old_s = r.frame_new_s = -1;
  r.q2_frames = 0;
  q2_predict_map(r.q2_ahead, nullptr);  // the level's, when the window loads it
  r.q2_weapon = Q2Weapon{};
}

int q2_move_ms(int rate) {
  const int held = rate <= 0 ? kQ2MostMoves : std::clamp(rate, kQ2LeastMoves, kQ2MostMoves);
  return 1000 / held;
}

RemoteStep q2_remote_step(Remote& r, double wall_s, std::vector<std::string>& said) {
  Q2ClientEnd& c = *r.q2;
  client::Demo& d = r.demo;
  // A weapon key as Quake II's own command: a stock game takes `use`, never an impulse.
  const int key = r.hands.impulse;
  if (key >= 1 && key <= server::kSqWeaponCount && c.stage == ClientStage::active)
    q2_client_command(c, std::string("use ") + server::kSqWeapons[key - 1].name);
  r.hands.impulse = 0;
  if (c.cfg.live_mic) {  // the voice keys, as on Quake's server (qw_remote.cpp)
    c.mic_talking = remote_talking(r);
    if (c.mic_talking && r.talk != nullptr && ge::console::talk_voice(*r.talk).on)
      q2_voice_target(c,
                      std::string(ge::console::chat_to_word(ge::console::talk_voice(*r.talk).to)));
  }
  r.owed_s += wall_s;
  int ticks = 0;
  const int tick_us = q2_move_ms(r.q2_maxfps) * 1000;  // its own cl_maxfps, live
  c.cfg.shownet = r.q2_shownet;                        // cl_shownet, live
  r.trace.sent = r.q2_showpackets;                     // cl_showpackets, live
  while (r.owed_s >= tick_us * 1e-6) {
    r.owed_s -= tick_us * 1e-6;
    if (ticks++ >= kMostTicks) continue;  // a stall: the time is dropped, not paid down
    r.now_us += tick_us;
    q2_client_tick(c, r.now_us);
  }
  for (std::string& line : std::exchange(c.ignored, {})) said.push_back(std::move(line));
  for (std::string& line : std::exchange(c.to_run, {})) r.stuffed_run.push_back(std::move(line));
  for (std::string& line : std::exchange(c.http.said, {})) said.push_back(std::move(line));
  if (c.stage != r.said) {
    r.said = c.stage;
    std::string line = remote_stage_words(r.said) + r.server_name;
    if (r.said == ClientStage::dropped && !c.refused.empty()) line += ": " + c.refused;
    said.push_back(line);
  }
  if (c.stage == ClientStage::dropped) {
    r.why = c.refused.empty() ? "the connection was lost" : c.refused;
    remote_disconnect(r);
    return RemoteStep::dropped;
  }
  // What the payloads since the last frame carried, for the window to hear and show.
  d.state.prints = std::move(c.news.prints);
  d.state.effects = std::move(c.news.effects);
  d.state.started = std::move(c.news.started);
  d.state.stopped = std::move(c.news.stopped);
  d.state.centre = std::move(c.news.centre);
  d.state.voice = std::move(c.news.voice);
  c.news = Q2News{};
  const double now_s = static_cast<double>(r.now_us) * 1e-6 + r.owed_s;
  // The ignore lists, the team tags and the regex triggers (chat_ignore.hpp).
  r.ignore.chat.relay = false;                  // a game joined: Quake II's message triggers run
  remote_sheet_step(r, d.state.prints, now_s);  // `info` asked for the settings page
  remote_fair_step(r, d.state.prints, now_s);   // f_ruleset answered (qw_remote_rulesets.hpp)
  client::drop_ignored(r.ignore, d.state, d.state.prints, now_s, &d.state.centre);
  // The cheat lock: in a game of more than one player slot no cheat holds, whatever the local
  // sv_cheats says (let go by remote_disconnect).
  if (r.ignore.con != nullptr)
    r.ignore.con->cvars().hold_cheats(c.stage == ClientStage::active && d.max_clients > 1);
  const bool fresh = c.frames != r.q2_frames;
  client::DemoState& st = d.state;
  if (fresh) {  // a frame came: the last two kept, as read, for drawing between them
    r.q2_frames = c.frames;
    r.ents_old.swap(r.ents_new);
    r.ents_new = st.entities;
    r.players_old.swap(r.players_new);
    r.players_new = st.players;
    r.frame_old_s = r.frame_new_s;
    r.frame_new_s = now_s;
    if (st.levels != r.levels) {  // a new level: nothing to draw between
      r.ents_old.clear();
      r.players_old.clear();
      r.frame_old_s = -1;
    }
  }
  // The drawn view: the hands' angles and the turn the server holds for the player.
  sh::Vec3f turn;
  if (d.last_frame >= 0) {
    const client::Q2View& v = d.views[static_cast<size_t>(d.last_frame & 15)];
    turn = {angle16(v.delta[0]), angle16(v.delta[1]), angle16(v.delta[2])};
  }
  st.angles = r.view + turn;
  st.angles.y = st.angles.y + r.jump_aid.shown;  // and the easy jump's swing as shown
  // The player's own moves run ahead of the newest frame, over the movers where it put them
  // (q2_predict.hpp); the eye stands over the prediction as the server's over its place.
  Q2Predict& ahead = r.q2_ahead;
  if (fresh && d.last_frame >= 0) {
    const client::Q2View& v = d.views[static_cast<size_t>(d.last_frame & 15)];
    ahead.eye_up = st.eye - sh::Vec3f{static_cast<float>(v.origin[0]) * 0.125f,
                                      static_cast<float>(v.origin[1]) * 0.125f,
                                      static_cast<float>(v.origin[2]) * 0.125f};
  }
  sh::Vec3f drawn;
  ahead.teleports = r.predict_teleports;  // cl_predict_teleport
  ahead.pushes = r.predict_pushes;        // cl_predict_push
  ahead.movers = r.predict_movers;        // cl_predict_movers
  const bool predicted = !r.nopred && q2_predict_step(ahead, c, d, wall_s, drawn);
  if (!predicted) ahead.predicted = false;
  // A teleport the prediction foresaw turns the view with it (the server's own delta angles,
  // classic/teleporters.hpp), not a round trip later.
  if (predicted && ahead.teleported) {
    turn = {angle16(ahead.delta_angles[0]), angle16(ahead.delta_angles[1]),
            angle16(ahead.delta_angles[2])};
    st.angles = r.view + turn;
    st.angles.y = st.angles.y + r.jump_aid.shown;
  }
  if (ahead.misses != ahead.misses_told) {  // cl_showmiss
    if (ahead.show_misses && r.trace.print)
      r.trace.print("prediction miss on " + std::to_string(ahead.miss_frame) + ": " +
                    std::to_string(std::lround(ahead.miss_units)));
    ahead.misses_told = ahead.misses;
  }
  // Everything between the last two frames, one frame behind the newest.
  r.lerped = 0;
  if (!r.nolerp && r.frame_old_s >= 0 && r.frame_new_s > r.frame_old_s) {
    const double f = (now_s - r.frame_new_s) / (r.frame_new_s - r.frame_old_s);
    const auto t = static_cast<float>(std::clamp(f, 0.0, 1.0));
    const sh::Vec3f eye_up = st.player >= 0 && static_cast<size_t>(st.player) < st.players.size()
                                 ? st.eye - st.players[static_cast<size_t>(st.player)].origin
                                 : sh::Vec3f{};
    r.lerped = lerp_list(st.entities, r.ents_old, r.ents_new, t) +
               lerp_list(st.players, r.players_old, r.players_new, t);
    if (st.player >= 0 && static_cast<size_t>(st.player) < st.players.size() && !st.intermission)
      st.eye = st.players[static_cast<size_t>(st.player)].origin + eye_up;
  }
  q2_draw_movers_ahead(ahead, st);  // cl_predict_movers
  if (predicted) {
    if (st.player >= 0 && static_cast<size_t>(st.player) < st.players.size())
      st.players[static_cast<size_t>(st.player)].origin = drawn;
    st.eye = drawn + ahead.eye_up + sh::Vec3f{0, 0, ahead.eye_shift};
    r.ahead.velocity = ahead.velocity;  // what the view's bob and the camera read
    r.ahead.ground = ahead.on_ground ? 0 : -1;
    r.ahead.waterlevel = ahead.water_level;  // and the easy jump (the hands, above)
    r.ahead.type = sh::MoveType::normal;
  }
  r.predicted = predicted;
  // The held weapon run ahead of the newest frame (q2_weapon.hpp): its frame, shots and ammo.
  if (st.player >= 0 && static_cast<size_t>(st.player) < st.players.size())
    q2_weapon_predict(r.q2_weapon, r.weapon, r.nopred, c, d,
                      st.players[static_cast<size_t>(st.player)].origin);
  q2_own_missiles(r, st, now_s, turn, ahead.eye_up);
  // The other players run forward from the moves our server sent (none from a stock server).
  // A stock server's run on from its last two frames instead.
  r.others = r.predict_players ? q2_predict_others(ahead, c, d, now_s, &r.others_msec) : 0;
  if (r.predict_players && c.moves_read == 0 && r.frame_old_s >= 0)
    r.others = q2_run_on_others(ahead, c, d, r.players_old, r.players_new, r.frame_new_s,
                                r.frame_new_s - r.frame_old_s, now_s, &r.others_msec);
  if (!r.predict_players) r.others_msec = 0;
  if (!fresh || st.map.empty()) return RemoteStep::waiting;
  if (st.levels != r.levels) {
    r.levels = st.levels;
    return RemoteStep::new_level;
  }
  return RemoteStep::frame;
}

void add_q2_download_command(ge::console::Console& con, Remote& r) {
  con.add_command("download", "download <file>: a file asked of the server you are on",
                  [&r](ge::console::Console& c, const ge::console::Args& a) {
                    if (!r.active || r.watch || !r.q2 || r.q2->stage != ClientStage::active) {
                      c.print("Must be connected to a server.");
                      return;
                    }
                    const ge::console::Cvar* allow = c.cvars().find("allow_download");
                    if (allow != nullptr && allow->value == "-1") {
                      c.print("Downloads are permanently disabled.");
                      return;
                    }
                    if (a.size() != 2) {
                      c.print("Usage: download <filename>");
                      return;
                    }
                    const std::string why = q2_download(*r.q2, a[1]);
                    if (!why.empty()) c.print("Couldn't download " + a[1] + ": " + why);
                  });
}

void add_q2_skins_command(ge::console::Console& con, Remote& r) {
  // Every game's (QuakeWorld's and Quake II's word alike): the level read again, so every figure
  // and skin fetched since is drawn; each player's look listed as it is now.
  con.add_command("skins", "skins: every player's model and skin read again",
                  [&r](ge::console::Console& c, const ge::console::Args&) {
                    const client::DemoState& st = r.demo.state;
                    if (!r.active || r.watch || st.map.empty()) {
                      c.print("Must be in a level to load skins.");
                      return;
                    }
                    r.kept = true;  // the level read again: every figure and skin drawn anew
                    for (size_t i = 0; i < st.names.size(); i++) {
                      const std::string& look = r.q2 ? st.looks[i] : st.skins[i];
                      if (st.names[i].empty() && look.empty()) continue;
                      if (r.q2 && look.empty()) continue;
                      c.print("client " + std::to_string(i) + ": " + st.names[i] + "\\" +
                              (look.empty() ? std::string("base") : look) + " --> " +
                              (look.empty() ? std::string("base") : look));
                    }
                  });
}

}  // namespace shooters_edge::net
