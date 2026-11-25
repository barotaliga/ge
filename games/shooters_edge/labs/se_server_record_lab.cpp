// LAB `se-server-record`: SHOOTER'S EDGE'S DEDICATED SERVER RECORDED BY THE SERVER CORE, PLAYED
// BACK FROM EVERY PLAYER'S VIEW AND RELAYED WITH A DELAY (engine/server/record.hpp, playback.hpp;
// games/shooters_edge/net/qw_game_tv.hpp; docs/servers.md § Recording). Quake
// serves a match of bots on the arena over the simulated network with the core's recorder on. Every
// slot's view at every frame is kept off the live server; the recording is then played back and
// each view held to it, frame by frame and after a seek into every second. The same recording goes
// through a source's TV node holding it back by the delay, a relay, and viewers on both, under
// every network preset: each viewer's copy must be whole, play back to the server's last views,
// and reach nobody before the delay.
//
// The pictures: every player's path over the arena as the playback gave it, and the lag of each
// block to each viewer per preset.
//
// ge:summary Lab: the classic server's recording played back from every view, relayed with a delay
// ge:keywords lab se-server-record server-record recording playback player-view seek relay delay
// ge:keywords classic-game multiview preset tv
// ge:skill replay net labs
// ge:test tests/shooters_edge_qw_record_server_test.cpp
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "classic/q2_move.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "lab/stats.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "server/playback.hpp"
#include "server/record.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_game.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "tv/trial.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace sv = ge::server;
namespace tv = ge::tv;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace cn = shooters_edge::net;

using Views = std::vector<std::vector<uint8_t>>;  // a frame's view of every slot

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    const size_t comma = s.find(',', at);
    const std::string part =
        s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!part.empty()) out.push_back(part);
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

// A view's place on the plan: after the 1 of a slot in play, the origin as three shorts in eighths
// of a unit (the classic's `mv_view` puts the model's frame first: `at` 2; Quake II's
// `q2_player_view` none: `at` 1). False for a slot nobody plays.
bool place(const std::vector<uint8_t>& v, size_t at, double& x, double& y) {
  if (v.size() < at + 4 || v[0] != 1) return false;
  const auto coord = [&](size_t i) {
    return static_cast<int16_t>(static_cast<uint16_t>(v[i] | (v[i + 1] << 8))) / 8.0;
  };
  x = coord(at);
  y = coord(at + 2);
  return true;
}

gl::Rgba colour_of(int n) {
  static const gl::Rgba kColours[] = {{230, 90, 70},   {80, 170, 230},  {240, 190, 60},
                                      {110, 200, 120}, {190, 120, 220}, {240, 140, 190},
                                      {120, 220, 210}, {200, 200, 200}};
  return kColours[static_cast<size_t>(n) % (sizeof kColours / sizeof kColours[0])];
}

std::string upper(std::string s) {
  for (char& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  return s;
}

// A walled hall of Quake II's map family, written as <root>/baseq2/maps/hall.bsp; the root, or
// "" with `error`.
std::string hall_folder(std::string& error) {
  const auto box = [](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
  };
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  br::Entity a, b;
  a.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "0"}};
  b.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "400 400 24"}, {"angle", "180"}};
  w.entities = {a, b};
  std::vector<uint8_t> bytes;
  if (!br::write_brush_world(w, &bytes, &error)) return "";
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "ge_se_server_record_lab";
  std::filesystem::create_directories(root / "baseq2" / "maps");
  std::ofstream(root / "baseq2" / "maps" / "hall.bsp", std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  return root.string();
}

// Quake II's two clients: one runs and turns, one strafes side to side looking up and down.
sh::MoveCmd runner(int tick) {
  sh::MoveCmd m;
  m.forward = 300;
  m.angles[1] = static_cast<float>((tick * 2) % 360);
  return m;
}

sh::MoveCmd dodger(int tick) {
  sh::MoveCmd m;
  m.forward = 150;
  m.side = (tick / 40) % 2 == 0 ? 300 : -300;
  m.angles[0] = static_cast<float>((tick % 60) - 30);
  m.angles[1] = static_cast<float>((tick / 3) % 360);
  return m;
}

// Every frame's views off a recording's playback, to its end. False with `error`.
bool play_all(const sv::ServerGame& g, const std::vector<uint8_t>& bytes, int slots,
              std::vector<Views>& out, std::string& error) {
  sv::Playback pb;
  if (!sv::playback_open(pb, g, bytes, error)) return false;
  const auto row = [&] {
    Views r(static_cast<size_t>(slots));
    for (int p = 0; p < slots; p++) sv::playback_view(pb, p, r[static_cast<size_t>(p)]);
    out.push_back(std::move(r));
  };
  row();
  while (sv::playback_step(pb, error)) row();
  return error.empty();
}

void run_game(gl::Lab& lab, const std::string& game) {
  const bool q2 = game == "q2";
  const int slots = static_cast<int>(lab.arg_num("slots"));
  const int seconds = static_cast<int>(lab.arg_num("seconds"));
  const uint32_t seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const std::string where = q2 ? "the hall" : "the arena";
  const size_t origin_at = q2 ? 1 : 2;  // where a view's origin starts (`place`)
  std::string error;

  // THE MATCH, SERVED AND RECORDED: every slot's view at every frame kept off the live server.
  // Quake plays its bots on the arena; Quake II's server has none, so two clients
  // of Quake II play a walled hall.
  gn::LinkConditions lan;
  gn::SimNetwork net = gn::make_sim_network(seed, lan);
  gn::Transport port;
  const uint16_t server_port = q2 ? cn::kPortQ2 : 27500;
  gn::open_sim_transport(port, net, gn::sim_address(0, server_port));
  sv::ServerConfig c;
  c.hostname = "Arena Night";
  c.map = q2 ? "hall" : "arena";
  c.maxclients = slots;
  c.bots = q2 ? 0 : static_cast<int>(lab.arg_num("bots"));
  c.seed = seed;
  if (q2) c.root = hall_folder(error);
  const sv::ServerGame& g = q2 ? cn::q2_game() : cn::qw_game();
  sv::Server s;
  if (!error.empty() || !sv::server_open(s, g, c, &port, nullptr, error)) {
    lab.fail("the " + game + " server did not open: " + error,
             "next: run make test T=shooters_edge_" + game + "_record_server; skill: net.");
    return;
  }
  std::vector<std::unique_ptr<gn::Transport>> ports;
  std::vector<std::unique_ptr<cn::Q2ClientEnd>> clients;
  if (q2)
    for (int n = 0; n < 2; n++) {
      ports.push_back(std::make_unique<gn::Transport>());
      gn::open_sim_transport(*ports.back(), net, gn::sim_address(uint32_t(n + 1), 27901));
      clients.push_back(std::make_unique<cn::Q2ClientEnd>());
      cn::Q2ClientConfig cc;
      cc.name = n == 0 ? "runner" : "dodger";
      cc.brain = n == 0 ? runner : dodger;
      cc.qport = static_cast<uint16_t>(100 + n);
      cn::q2_client_start(*clients.back(), *ports.back(), gn::sim_address(0, server_port), cc,
                          static_cast<double>(s.now_us) / 1e6);
    }
  const auto step = [&] {
    gn::sim_advance(net, s.now_us);
    for (auto& cl : clients) cn::q2_client_tick(*cl, s.now_us);
    sv::server_frame(s);
  };
  const int64_t per_s = std::llround(1.0 / s.frame_s);
  if (q2)
    for (int64_t f = 0; f < 2 * per_s; f++) step();  // the clients join and play
  sv::Recording rec;
  rec.keyframe_seconds = static_cast<int>(lab.arg_num("keyframe-seconds"));
  if (!sv::record_begin(rec, s, error)) {
    lab.fail("the recording did not start: " + error,
             "next: run make test T=shooters_edge_" + game + "_record_server; skill: replay.");
    return;
  }
  std::vector<Views> live;
  const auto keep = [&] {
    Views row(static_cast<size_t>(slots));
    for (int p = 0; p < slots; p++) g.player_view(s.state, p, row[static_cast<size_t>(p)]);
    live.push_back(std::move(row));
  };
  keep();
  const int64_t frames = std::llround(seconds / s.frame_s);
  for (int64_t f = 0; f < frames; f++) {
    step();
    sv::record_frame(rec, s);
    keep();
  }
  sv::record_finish(rec, s);
  sv::server_close(s);
  lab.metric(game + ".recording.bytes", static_cast<double>(rec.bytes.size()), "bytes");
  lab.metric(game + ".recording.bytes-per-s", static_cast<double>(rec.bytes.size()) / seconds,
             "bytes/s");
  lab.metric(game + ".recording.keyframes", static_cast<double>(rec.keyframes), "count");
  lab.metric(game + ".recording.keyframe-bytes", static_cast<double>(rec.keyframe_bytes), "bytes");

  // PLAYED BACK FROM EVERY VIEW: frame by frame, then a seek into every second.
  std::vector<Views> played;
  if (!play_all(g, rec.bytes, slots, played, error)) {
    lab.fail("the recording did not play back: " + error,
             "next: run make test T=shooters_edge_" + game + "_record_server; skill: replay.");
    return;
  }
  int64_t checked = 0, wrong = 0, in_play = 0;
  for (size_t f = 0; f < played.size() && f < live.size(); f++)
    for (int p = 0; p < slots; p++) {
      checked++;
      in_play += live[f][static_cast<size_t>(p)].size() > 1 ? 1 : 0;
      wrong += played[f][static_cast<size_t>(p)] != live[f][static_cast<size_t>(p)] ? 1 : 0;
    }
  wrong += played.size() == live.size() ? 0 : 1;
  sv::Playback pb;
  int64_t seeks = 0, seek_wrong = 0;
  if (sv::playback_open(pb, g, rec.bytes, error)) {
    for (int64_t at = per_s / 2 + 1; at <= frames; at += per_s) {
      if (!sv::playback_seek(pb, at, error)) break;
      seeks++;
      for (int p = 0; p < slots; p++) {
        std::vector<uint8_t> v;
        sv::playback_view(pb, p, v);
        seek_wrong += v != live[static_cast<size_t>(at)][static_cast<size_t>(p)] ? 1 : 0;
      }
    }
  }
  lab.metric(game + ".playback.views-checked", static_cast<double>(checked), "count");
  lab.metric(game + ".playback.views-in-play", static_cast<double>(in_play), "count");
  lab.metric(game + ".playback.views-wrong", static_cast<double>(wrong), "count");
  lab.metric(game + ".playback.seeks", static_cast<double>(seeks), "count");
  lab.metric(game + ".playback.seek-views-wrong", static_cast<double>(seek_wrong), "count");

  // THE PATHS PICTURED: every player's place over the match off the playback, on the plan.
  {
    double lo_x = 1e9, hi_x = -1e9, lo_y = 1e9, hi_y = -1e9, x = 0, y = 0;
    for (const Views& row : played)
      for (const auto& v : row)
        if (place(v, origin_at, x, y)) {
          lo_x = std::min(lo_x, x);
          hi_x = std::max(hi_x, x);
          lo_y = std::min(lo_y, y);
          hi_y = std::max(hi_y, y);
        }
    const int size = 520, pad = 24, top = 26;
    const double span = std::max({hi_x - lo_x, hi_y - lo_y, 1.0});
    gl::Picture pic = gl::create_picture(size + 2 * pad + 150, size + top + pad, {18, 20, 26});
    gl::fill_rect(pic, pad, top, size, size, {30, 33, 42});
    const auto px = [&](double vx) {
      return pad + static_cast<int>((vx - lo_x) / span * (size - 4));
    };
    const auto py = [&](double vy) {
      return top + size - 4 - static_cast<int>((vy - lo_y) / span * (size - 4));
    };
    for (int p = 0; p < slots; p++) {
      bool any = false;
      for (const Views& row : played)
        if (place(row[static_cast<size_t>(p)], origin_at, x, y)) {
          gl::fill_rect(pic, px(x), py(y), 3, 3, colour_of(p));
          any = true;
        }
      if (any)
        gl::draw_text(pic, size + 2 * pad, top + 14 * p, "slot " + std::to_string(p + 1),
                      colour_of(p));
    }
    gl::draw_text(pic, pad, 8, where + " from above, " + fmt(span, 0) + " units across",
                  {150, 155, 170});
    lab.shot({"paths-" + game,
              "EVERY PATH IN THE " + upper(game) + " SERVER'S RECORDING",
              "The server's recording played back once: each player's place at every frame as the "
              "playback gave it, a colour per slot, on " +
                  where +
                  " seen from above. Every view "
                  "equals the one the live server gave at the same frame.",
              q2 ? "the runner's circles and the dodger's zigzag across the hall"
                 : "the bots' runs between the items; a slot that stops is a player who died "
                   "and waits",
              {{"game", game}, {"seconds", lab.arg("seconds")}, {"seed", lab.arg("seed")}}},
             pic);
  }

  // RELAYED WITH THE DELAY: the recording broadcast at the match's speed under every preset.
  tv::TrialConfig base;
  base.blocks = seconds;
  base.block_ms = 1000;
  base.viewers_direct = static_cast<int>(lab.arg_num("direct"));
  base.viewers_relayed = static_cast<int>(lab.arg_num("relayed"));
  base.delay_ms = static_cast<int64_t>(lab.arg_num("delay-ms"));
  base.seed = seed;
  base.source = &rec.bytes;
  base.keep_streams = true;
  gl::LabTable table{"each preset: the recording through a source with the delay, a relay, viewers",
                     {"preset", "viewers whole", "at the server's last views", "lag min ms",
                      "lag p50 ms", "lag p95 ms", "lag max ms", "none before the delay"},
                     {}};
  std::vector<gl::Sample> lags;
  int good = 0;
  const std::vector<std::string> presets = split(lab.arg("presets"));
  for (const std::string& name : presets) {
    tv::TrialConfig tc = base;
    if (!ge::net::conditions_preset(name, tc.link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      return;
    }
    const tv::TrialResult r = tv::run_tv_trial(tc);
    int whole = 0, ended = 0;
    std::vector<double> all;
    for (const tv::TrialViewer& v : r.viewers) {
      whole += v.whole ? 1 : 0;
      all.insert(all.end(), v.lag_ms.begin(), v.lag_ms.end());
      std::vector<Views> seen;
      std::string e;
      if (v.whole && play_all(g, v.stream, slots, seen, e) && !seen.empty())
        ended += seen.back() == live.back() ? 1 : 0;
    }
    const gl::Summary st = gl::summarize(all);
    const bool held = all.empty() || st.min >= static_cast<double>(base.delay_ms);
    const int n = static_cast<int>(r.viewers.size());
    table.rows.push_back({name, std::to_string(whole) + " of " + std::to_string(n),
                          std::to_string(ended) + " of " + std::to_string(n), fmt(st.min, 0),
                          fmt(st.p50, 0), fmt(st.p95, 0), fmt(st.max, 0), held ? "yes" : "no"});
    lags.push_back({name, all});
    good += whole == n && ended == n && held ? 1 : 0;
    std::string key = game;
    key += '.';
    key += name;
    lab.metric(key + ".viewers-whole", whole, "count");
    lab.metric(key + ".at-last-views", ended, "count");
    lab.metric(key + ".lag-min-ms", st.min, "ms");
    lab.metric(key + ".lag-p50-ms", st.p50, "ms");
  }
  lab.table(table);
  lab.metric(game + ".presets-good", good, "count");
  const gl::ChartFrame frame{
      "HOW LATE EACH BLOCK OF THE " + upper(game) + " RECORDING REACHED VIEWERS",
      "held back " + lab.arg("delay-ms") + " ms at the source; a relay; " +
          std::to_string(base.viewers_direct + base.viewers_relayed) + " viewers; seed " +
          lab.arg("seed"),
      "", "lag after the server wrote it (ms)"};
  lab.shot({"lag-" + game,
            frame.title,
            "The " + game +
                " server's recording broadcast at the match's own speed through a source's "
                "TV node that holds every block back by the delay, then a relay; a box per network "
                "preset of when each viewer had each block whole.",
            "every box at or above the delay; the boxes rising with the link's loss",
            {{"game", game}, {"presets", lab.arg("presets")}, {"delay-ms", lab.arg("delay-ms")}}},
           gl::to_picture(gl::box_chart(frame, lags).plot));

  if (in_play == 0)
    lab.fail(game + ": no slot was in play in any frame: nobody joined",
             "next: run make test T=shooters_edge_qw_game; skill: net.");
  if (wrong != 0 || seek_wrong != 0)
    lab.fail(
        std::to_string(wrong + seek_wrong) + " views played back differ from the live server's",
        "next: run make test T=shooters_edge_qw_record_server and compare "
        "mv_player_view with mvd_player_view; skill: replay.");
  if (good != static_cast<int>(presets.size()))
    lab.fail(
        "under a preset a viewer was not whole, did not end at the server's views, or got a "
        "block before the delay",
        "next: read the table's row and run that preset in tests/tv_net_test.cpp; skill: "
        "replay.");
}

void run(gl::Lab& lab) {
  for (const std::string& game : split(lab.arg("games"))) {
    if (game != "q1" && game != "q2") {
      lab.fail("unknown game '" + game + "'", "next: pass --games=q1,q2.");
      return;
    }
    run_game(lab, game);
  }
}

const gl::LabSpec kSpec{
    "se-server-record",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/qw_game_tv: the classic server's match recorded by the core, "
    "played back from every view, relayed with a delay",
    "replay",
    "Does a match Shooter's Edge's server recorded play back as every player saw it, from the "
    "start and after a seek, and do watchers on a relay get it whole with the delay under every "
    "network preset?",
    {
        {"games", "q1,q2",
         "the games served: q1 (bots on the arena), Quake II (two "
         "clients in a hall)"},
        {"bots", "4", "Quake's bots"},
        {"slots", "8", "the server's player slots (sv_maxclients), every one's view checked"},
        {"seconds", "20", "the match's length"},
        {"keyframe-seconds", "5", "a keyframe every this many seconds of the recording"},
        {"presets", "lan,broadband,wifi,mobile,bad,awful", "condition presets, comma-separated"},
        {"delay-ms", "2000", "the source node's delay"},
        {"direct", "2", "viewers on the source's node"},
        {"relayed", "3", "viewers under the relay"},
        {"seed", "5", "the match's, the network's and the nodes' stream"},
    },
    "--seconds=6 --presets=lan,bad",
    run,
};
GE_LAB(kSpec);

}  // namespace
