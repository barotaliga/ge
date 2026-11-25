// LAB `se-fastdl`: how long does a player wait for a map they lack, over the game's own channel and
// over HTTPS from the server's own web files (engine/server/web_files.hpp)? `--game=qw` (the
// default) or `q2`.
//
// THE MAP: `--map=<a .bsp>` (a real QuakeWorld map, such as aerowalk), else made-up bytes of
// `--size` kB (the smoke run's).
//
// OVER THE CHANNEL: the QuakeWorld server (qw_server.hpp) serves the map's bytes as a file a
// scripted client (qw_client.hpp) lacks and fetches before it plays, over engine/net's simulated
// LAN (`run_qw_trial`, one client); the wait is the time to play with the file less the time to
// play without it, on the session's own clock: the same to the bit on every machine. With
// `--game=q2` our Quake II server (q2_net_server.hpp) serves it to our scripted Quake II client
// (q2_client.hpp) on a hall of brushes, once asking the base protocol (34, a stock client's) and
// once the best the server offers (36, its downloads packed).
//
// OVER HTTPS: the server's web file server on loopback, TLS under the certificate it makes itself,
// and the client's web downloads (classic/content/web_downloads.hpp) trusting it by the digest the
// server tells; the file's SHA-256 checked before it is kept. Timed on the wall clock (measured,
// not in the digest), at the server's default caps (5000 kB a second a player, 20000 in all) and
// with no cap. The progress the console's bar draws is sampled as it goes.
//
// ge:summary Lab: a map's wait over the channel vs HTTPS from the server's own web files
// ge:keywords lab se-fastdl fast-downloads https web-files download-time udp-vs-https
// ge:keywords sv_http_port rate-cap progress q2-download q2-fastdl
// ge:skill net labs
// ge:test tests/net_file_server_test.cpp
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "classic/content/web_downloads.hpp"
#include "console/view.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/file_server.hpp"
#include "net/sim_network.hpp"
#include "net/tls_stream.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/net/qw_trial.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "ui/console.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace cn = shooters_edge::net;
namespace wd = ge::classic::content;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
using ge::core::num::to_fixed;

struct WebRun {
  bool ok = false;
  std::string error;
  double seconds = 0;
  std::vector<double> t, pct;  // the bar's progress over time
  std::string foot;            // the file the foot names at the first sample past 40%
  double share = -1;           // and its share
};

// One fetch of `map` as `maps/<name>.bsp` from a web file server on loopback with these caps.
WebRun fetch_over_https(const std::vector<uint8_t>& map, const std::string& file,
                        const std::string& folder, int64_t rate_kb, int64_t player_kb) {
  WebRun out;
  gn::FileServer server;
  gn::FileServeOptions o;
  o.rate = rate_kb * 1000;
  o.rate_person = player_kb * 1000;
  if (!gn::tls_self_signed("ge-server", o.cert_pem, o.key_pem)) {
    out.error = "no certificate could be made";
    return out;
  }
  if (!gn::file_server_open(
          server, gn::loopback_v4(0), o,
          [&map, &file, &folder](const std::string& path, std::vector<uint8_t>& bytes) {
            if (path != folder + "/maps/" + file) return false;
            bytes = map;
            return true;
          },
          out.error))
    return out;
  uint8_t pin[32];
  gn::tls_cert_sha256(o.cert_pem, pin);
  std::string hex;
  for (const uint8_t b : pin) {
    hex += "0123456789abcdef"[b >> 4];
    hex += "0123456789abcdef"[b & 15];
  }
  wd::WebConfig cfg;
  cfg.base_folder = folder;
  cfg.filelists = false;
  cfg.kinds = wd::quake_web_kinds();
  wd::WebDownloads h;
  // The address as a server with no settings but its port tells it, and its certificate's digest.
  wd::web_connect(h, cfg, "https://{server}:" + std::to_string(server.listener.local.port) + "/", 1,
                  folder == "qw" ? "127.0.0.1:27500" : "127.0.0.1:27910", hex);
  wd::web_queue(h, cfg, "maps/" + file);
  const auto t0 = std::chrono::steady_clock::now();
  const auto now = [&t0] {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  };
  wd::WebDone all;
  double last_sample = -1;
  while (wd::web_busy(h) && now() < 120) {
    const double t = now();
    gn::file_server_step(server, t);
    wd::WebDone d;
    wd::web_step(h, cfg, t, [](const std::string&, bool) { return true; }, d);
    for (auto& f : d.files) all.files.push_back(std::move(f));
    wd::WebProgress p;
    if (t - last_sample > 0.002 && wd::web_progress(h, p) && p.total > 0) {
      out.t.push_back(t);
      out.pct.push_back(100.0 * static_cast<double>(p.got) / static_cast<double>(p.total));
      if (out.share < 0 && out.pct.back() >= 40) {
        out.foot = p.path;
        out.share = out.pct.back() / 100;
      }
      last_sample = t;
    }
  }
  out.seconds = now();
  out.t.push_back(out.seconds);
  out.pct.push_back(100);
  gn::file_server_close(server);
  out.ok = all.files.size() == 1 && all.files[0].second == map;
  if (!out.ok) {
    out.error = "the map did not come whole:";
    for (const std::string& l : h.said) out.error += " " + l;
  }
  return out;
}

// A hall of brushes for Quake II's server to run while it serves the map's bytes as its file.
std::shared_ptr<const br::Map> q2_hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const auto box = [&w](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(-1024, -1024, -16, 1024, 1024, 0);
  box(-1040, -1024, -16, -1024, 1024, 256);
  box(1024, -1024, -16, 1040, 1024, 256);
  box(-1040, 1024, -16, 1040, 1040, 256);
  box(-1040, -1040, -16, 1040, -1024, 256);
  br::Entity spot;
  spot.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}};
  w.entities.push_back(spot);
  std::vector<uint8_t> bytes;
  std::string error;
  auto map = std::make_shared<br::Map>();
  if (!br::write_brush_world(w, &bytes, &error) || !br::read_bsp(bytes, map.get(), &error))
    return nullptr;
  return map;
}

// The simulated seconds until our Quake II client, asking `protocol`, plays on our server; with
// `map` it lacks the level's map and fetches those bytes first. -1 when it never plays.
double q2_wait(const std::vector<uint8_t>* map, int protocol, double most_s, std::string& error) {
  sv::RulesRunner rules = sv::q2_runner(sv::make_q2_server({}));
  rules.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = q2_hall();
  if (spec.brushes == nullptr || !rules.start(spec, error)) return -1;
  gn::LinkConditions link;
  gn::conditions_preset("lan", link);
  gn::SimNetwork net = gn::make_sim_network(7, link);
  gn::Transport st, ct;
  gn::open_sim_transport(st, net, gn::sim_address(0, cn::kPortQ2));
  gn::open_sim_transport(ct, net, gn::sim_address(1, 27901));
  cn::Q2NetConfig cfg;
  cfg.read_file = [map](const std::string& path, std::vector<uint8_t>* out) {
    if (map == nullptr || path != "maps/hall.bsp") return false;
    *out = *map;
    return true;
  };
  cn::Q2NetServer server;
  if (!cn::q2_net_start(server, rules, st, cfg, error)) return -1;
  cn::Q2ClientEnd c;
  cn::Q2ClientConfig cc;
  cc.name = "fetcher";
  cc.qport = 100;
  cc.fetch = map != nullptr;
  cc.protocol = protocol;
  cn::q2_client_start(c, ct, gn::sim_address(0, cn::kPortQ2), cc, 0);
  int64_t now = 0;
  while (c.stage != cn::ClientStage::active && static_cast<double>(now) / 1e6 < most_s) {
    now += 13000;
    gn::sim_advance(net, now);
    cn::q2_client_tick(c, now);
    cn::q2_net_frame(server, now);
  }
  if (c.stage != cn::ClientStage::active) {
    error = "the client did not reach play: " + c.refused;
    return -1;
  }
  if (map != nullptr && (c.downloaded.size() != 1 || c.downloaded["maps/hall.bsp"] != *map)) {
    error = "the map did not come whole over the channel";
    return -1;
  }
  return static_cast<double>(now) / 1e6;
}

void run(gl::Lab& lab) {
  std::vector<uint8_t> map;
  std::string name = "made-up bytes";
  if (const std::string& path = lab.arg("map"); !path.empty()) {
    std::ifstream f(path, std::ios::binary);
    map.assign(std::istreambuf_iterator<char>(f), {});
    if (map.empty()) {
      lab.fail("se-fastdl: the map " + path + " does not read",
               "next: pass --map=<a .bsp file of the game>");
      return;
    }
    name = path.substr(path.find_last_of("/\\") + 1);
  } else {
    map.resize(static_cast<size_t>(lab.arg_num("size") * 1000));
    for (size_t i = 0; i < map.size(); i++) map[i] = static_cast<uint8_t>((i * 131 + 7) >> 3);
  }
  const std::string file = lab.arg("map").empty() ? "lab.bsp" : name;
  const double kb = static_cast<double>(map.size()) / 1000;
  lab.metric("map-size", kb, "kB");

  const std::string game = lab.arg("game");
  if (game != "qw" && game != "q2") {
    lab.fail("se-fastdl: no game '" + game + "'", "next: pass --game=qw or --game=q2");
    return;
  }
  const std::string folder = game == "qw" ? "qw" : "baseq2";
  std::vector<std::pair<std::string, double>> channel;  // each kind of client's wait
  if (game == "q2") {
    // Over Quake II's channel: the base protocol (34) and the newer one (36), each the time to play
    // with the map to fetch less the time without it, on the simulated clock.
    for (const int protocol : {34, 0}) {
      std::string error;
      const double most = 20 + kb;
      const double without = q2_wait(nullptr, protocol, most, error);
      const double with = without < 0 ? -1 : q2_wait(&map, protocol, most, error);
      const std::string mode = protocol == 34 ? "protocol-34" : "protocol-36";
      if (with < 0) {
        std::string what = "se-fastdl: Quake II, ";
        what += mode;
        what += ": ";
        what += error;
        lab.fail(what, "next: make lab LAB=se-q2-net to see where the signon stops");
        return;
      }
      channel.emplace_back(mode, with - without);
      lab.metric("channel." + mode + ".seconds", with - without, "s");
    }
  } else {
    // Over the channel: the time to play with the file to fetch, less the time without it, for a
    // client of each kind: the extended protocol's (asking by lines, then by the protocol
    // extension's bits: chunked downloads) and the base protocol's (a stock client's pieces, paced
    // by its rate).
    cn::TrialOptions t;
    t.clients = 3;
    t.figures = true;
    t.preset = "lan";
    t.seconds = 8 + kb / 8;
    const cn::TrialReport without = cn::run_qw_trial(t);
    t.lit = map;
    const cn::TrialReport with = cn::run_qw_trial(t);
    for (size_t i = 0; i < with.clients.size() && i < without.clients.size(); i++) {
      const cn::TrialClient& c = with.clients[i];
      if (c.active_s < 0 || without.clients[i].active_s < 0) {
        lab.fail("se-fastdl: " + c.name + " (" + c.mode + ") did not reach play over the channel",
                 "next: make lab LAB=se-net to see where the signon stops");
        return;
      }
      const double s = c.active_s - without.clients[i].active_s;
      channel.emplace_back(c.mode, s);
      lab.metric("channel." + c.mode + ".seconds", s, "s");
      lab.metric("channel." + c.mode + ".files-equal", c.downloads, "count");
    }
  }
  double udp_s = 0;  // the slowest: the base protocol's, what a stock client gets
  for (const auto& [mode, s] : channel) udp_s = std::max(udp_s, s);

  // Over HTTPS: the server's default caps, then none.
  const WebRun capped = fetch_over_https(map, file, folder, 20000, 5000);
  const WebRun open = fetch_over_https(map, file, folder, 0, 0);
  for (const WebRun* r : {&capped, &open})
    if (!r->ok) {
      lab.fail("se-fastdl: over HTTPS: " + r->error,
               "next: run the net_file_server test; the lab needs a loopback TCP socket");
      return;
    }
  lab.metric("https-capped.seconds", capped.seconds, "s", gl::Det::measured);
  lab.metric("https-open.seconds", open.seconds, "s", gl::Det::measured);
  lab.metric("https-capped.kb-per-s", kb / capped.seconds, "kB/s", gl::Det::measured);
  lab.metric("https-open.kb-per-s", kb / open.seconds, "kB/s", gl::Det::measured);
  lab.metric("speedup-capped", udp_s / capped.seconds, "x", gl::Det::measured);

  gl::LabTable table{"the wait for " + name + " (" + to_fixed(kb, 0) + " kB)",
                     {"how", "seconds", "kB a second", "times faster than the base protocol"},
                     {}};
  for (const auto& [mode, sec] : channel)
    table.rows.push_back({"the game's channel, a client asking " + mode + " (simulated LAN)",
                          to_fixed(sec, 2), to_fixed(kb / sec, 0), to_fixed(udp_s / sec, 1)});
  table.rows.push_back({"HTTPS, the server's default caps (5000 kB/s a player)",
                        to_fixed(capped.seconds, 3), to_fixed(kb / capped.seconds, 0),
                        to_fixed(udp_s / capped.seconds, 0)});
  table.rows.push_back({"HTTPS, no cap (loopback)", to_fixed(open.seconds, 3),
                        to_fixed(kb / open.seconds, 0), to_fixed(udp_s / open.seconds, 0)});
  lab.table(table, gl::Det::measured);

  gl::LineChart chart;
  chart.frame.title = "The download bar over the HTTPS fetch of " + name;
  chart.frame.x_label = "t (s)";
  chart.frame.y_label = "the map (%)";
  chart.traces.push_back({"5000 kB/s a player", capped.t, capped.pct});
  chart.traces.push_back({"no cap", open.t, open.pct});
  const gl::Chart c = gl::line_chart(chart);
  lab.shot({"https-progress",
            c.title,
            "The share of the map the console's download bar shows over time, sampled from the "
            "client's web downloads as the server's web file server sends it over TLS.",
            "two lines rising from 0 to 100%; the capped one straight at the cap's slope, the "
            "open one far steeper",
            {{"map", name}}},
           gl::to_picture(c.plot), gl::Det::measured);

  // The console's foot as a player joining sees it mid-fetch: the file, the bar, the share.
  ge::console::Console console;
  ge::console::ConsoleView v(console);
  v.set_full(true);
  v.set_foot(capped.foot.empty() ? "maps/" + file : capped.foot, capped.share);
  ge::ui::Image img;
  img.width = 640, img.height = 400;
  img.rgba.assign(static_cast<size_t>(img.width) * img.height * 4, 0);
  for (size_t i = 3; i < img.rgba.size(); i += 4) img.rgba[i] = 255;
  ge::ui::DrawList dl;
  dl.begin(img.width, img.height);
  ge::ui::draw_console(dl, v);
  ge::ui::render(dl, img);
  gl::Picture bar{img.width, img.height, std::move(img.rgba)};
  lab.shot({"https-bar",
            "THE DOWNLOAD BAR OVER HTTPS",
            "The console while joining, its foot fed by the client's web downloads at the first "
            "sample past 40% of the capped fetch: the file's name, the charset's bar, the share.",
            "the console over the screen; on its foot the map's file, a bar 40-60% full and its "
            "percentage",
            {{"map", name}}},
           bar, gl::Det::measured);
}

const gl::LabSpec kSpec{
    "se-fastdl",
    gl::LabKind::bench,
    "game:shooters_edge",
    "engine/server/web_files.hpp and engine/net/file_server.hpp serving a map over HTTPS to "
    "plugins/classic/content/web_downloads.hpp, against QuakeWorld's and Quake II's channels' own "
    "download",
    "net",
    "How long does a player wait for a map they lack, over the game's channel and over HTTPS "
    "from the server's own web files?",
    {
        {"game", "qw", "the game whose channel the map also comes over: qw or q2"},
        {"map", "", "a .bsp of the game to send (\"\": made-up bytes of --size kB)"},
        {"size", "200", "the made-up map's kB"},
    },
    "--size=60",
    run,
};
GE_LAB(kSpec);

}  // namespace
