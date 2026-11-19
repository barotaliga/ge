// The window joined to a server of the sequel (sequel_remote.hpp).
//
// ge:summary The window joined to a server of the sequel: start, step, hands, frames in between
// ge:keywords sequel-join remote-client lerp delta-angles use-weapon
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#include "shooters_edge/net/sequel_remote.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "classic/sequel_move.hpp"
#include "shooter/speedjump_assist.hpp"
#include "shooters_edge/net/sequel_client.hpp"
#include "shooters_edge/server/sequel_combat.hpp"

namespace shooters_edge::net {

namespace {

float angle16(int16_t a) { return static_cast<float>(a) * (360.0f / 65536.0f); }

// The player's settings as the sequel's user info: its figure and skin are one word
// ("male/grunt"); a classic skin alone names none of its figures, so the stock one.
SequelClientConfig sequel_config(Remote& r, const ClientConfig& cfg) {
  SequelClientConfig s;
  s.name = cfg.name;
  s.skin = !cfg.model.empty() ? cfg.model + "/" + cfg.skin : "male/grunt";
  if (cfg.skin.find('/') != std::string::npos) s.skin = cfg.skin;
  s.rate = cfg.rate;
  s.fov = cfg.fov;
  s.hand = cfg.hand;
  s.speedjump = cfg.speedjump;
  s.spectator = cfg.spectator;
  s.password = cfg.password;
  s.qport = cfg.qport;
  s.into = &r.demo;
  // The files the level names that the window lacks, fetched before the game begins and kept
  // in the archive the server says each came out of (the window's `keep_file`).
  s.fetch = true;
  s.fetch_level = true;
  s.ask_moves = true;  // the other players' moves, to run them forward (sequel_predict.hpp)
  s.read_file = [&r](const std::string& path) {
    return r.read_file ? r.read_file("", path) : std::vector<uint8_t>{};
  };
  s.keep_file = [&r](const std::string& path, const std::vector<uint8_t>& bytes,
                     const ge::vfs::ArchiveOrigin* origin) {
    if (r.keep_file) r.keep_file("", path, bytes, origin);
  };
  s.brain = [&r](int) {
    sh::MoveCmd m = r.hands;
    m.angles = r.view;
    m.impulse = 0;  // a weapon key went as `use` (below)
    // The easy jump (shooter/speedjump_assist.hpp) off the newest prediction, by the sequel's air
    // law at the air acceleration the server states: at the share our server told (svc 126),
    // else kStockShare on a stock server, which tells none.
    if (r.sequel != nullptr && r.sequel->cfg.speedjump && r.predicted) {
      const float told = r.demo.speedjump_share;
      const float share = told >= 0 ? told : kStockShare;
      // The aid reads the way in the world: the hands' angles plus the server's turn of him.
      sh::Vec3f turn;
      if (r.demo.last_frame >= 0) {
        const client::SequelView& v = r.demo.views[static_cast<size_t>(r.demo.last_frame & 15)];
        turn = {angle16(v.delta[0]), angle16(v.delta[1]), angle16(v.delta[2])};
      }
      m.angles = m.angles + turn;
      m = sh::assisted_command(m, r.ahead, share, r.speedjump_view, r.jump_aid,
                               sh::sequel_jump_rules(r.demo.state.air_accelerate));
      m.angles = m.angles - turn;
    }
    // The sequel's jump is an up move, never a button (the button's bit is its `use`).
    if ((m.buttons & sh::kButtonJump) != 0) {
      m.up = std::max<int16_t>(m.up, ge::classic::kSqJumpUp);
      m.buttons = static_cast<uint8_t>(m.buttons & ~sh::kButtonJump);
    }
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

}  // namespace

void sequel_remote_start(Remote& r, const ClientConfig& cfg) {
  r.sequel = std::make_unique<SequelClientEnd>();
  sequel_client_start(*r.sequel, *r.transport, r.server, sequel_config(r, cfg), 0);
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
  r.sequel_frames = 0;
  sequel_predict_map(r.sequel_ahead, nullptr);  // the level's, when the window loads it
}

RemoteStep sequel_remote_step(Remote& r, double wall_s, std::vector<std::string>& said) {
  SequelClientEnd& c = *r.sequel;
  client::Demo& d = r.demo;
  // A weapon key as the sequel's own command: a stock game takes `use`, never an impulse.
  const int key = r.hands.impulse;
  if (key >= 1 && key <= server::kSqWeaponCount && c.stage == ClientStage::active)
    sequel_client_command(c, std::string("use ") + server::kSqWeapons[key - 1].name);
  r.hands.impulse = 0;
  r.owed_s += wall_s;
  int ticks = 0;
  while (r.owed_s >= kTickUs * 1e-6) {
    r.owed_s -= kTickUs * 1e-6;
    if (ticks++ >= kMostTicks) continue;  // a stall: the time is dropped, not paid down
    r.now_us += kTickUs;
    sequel_client_tick(c, r.now_us);
  }
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
  c.news = SequelNews{};
  const double now_s = static_cast<double>(r.now_us) * 1e-6 + r.owed_s;
  const bool fresh = c.frames != r.sequel_frames;
  client::DemoState& st = d.state;
  if (fresh) {  // a frame came: the last two kept, as read, for drawing between them
    r.sequel_frames = c.frames;
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
    const client::SequelView& v = d.views[static_cast<size_t>(d.last_frame & 15)];
    turn = {angle16(v.delta[0]), angle16(v.delta[1]), angle16(v.delta[2])};
  }
  st.angles = r.view + turn;
  st.angles.y = st.angles.y + r.jump_aid.shown;  // and the easy jump's swing as shown
  // The player's own moves run ahead of the newest frame, over the movers where it put them
  // (sequel_predict.hpp); the eye stands over the prediction as the server's over its place.
  SequelPredict& ahead = r.sequel_ahead;
  if (fresh && d.last_frame >= 0) {
    const client::SequelView& v = d.views[static_cast<size_t>(d.last_frame & 15)];
    ahead.eye_up = st.eye - sh::Vec3f{static_cast<float>(v.origin[0]) * 0.125f,
                                      static_cast<float>(v.origin[1]) * 0.125f,
                                      static_cast<float>(v.origin[2]) * 0.125f};
  }
  sh::Vec3f drawn;
  const bool predicted = !r.nopred && sequel_predict_step(ahead, c, d, wall_s, drawn);
  if (!predicted) ahead.predicted = false;
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
  if (predicted) {
    if (st.player >= 0 && static_cast<size_t>(st.player) < st.players.size())
      st.players[static_cast<size_t>(st.player)].origin = drawn;
    st.eye = drawn + ahead.eye_up;
    r.ahead.velocity = ahead.velocity;  // what the view's bob and the camera read
    r.ahead.ground = ahead.on_ground ? 0 : -1;
    r.ahead.waterlevel = ahead.water_level;  // and the easy jump (the hands, above)
    r.ahead.type = sh::MoveType::normal;
  }
  r.predicted = predicted;
  // The other players run forward from the moves our server sent (none from a stock server).
  // A stock server's run on from its last two frames instead.
  r.others = r.predict_players ? sequel_predict_others(ahead, c, d, now_s, &r.others_msec) : 0;
  if (r.predict_players && c.moves_read == 0 && r.frame_old_s >= 0)
    r.others = sequel_run_on_others(ahead, c, d, r.players_old, r.players_new, r.frame_new_s,
                                    r.frame_new_s - r.frame_old_s, now_s, &r.others_msec);
  if (!r.predict_players) r.others_msec = 0;
  if (!fresh || st.map.empty()) return RemoteStep::waiting;
  if (st.levels != r.levels) {
    r.levels = st.levels;
    return RemoteStep::new_level;
  }
  return RemoteStep::frame;
}

}  // namespace shooters_edge::net
