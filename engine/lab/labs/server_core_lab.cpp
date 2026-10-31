// LAB `server-core`: one game's server on the engine's server core (engine/server), run on the
// server trial (server/trial.hpp) once under each condition preset: dedicated and offline, as a
// listen server stepped by a 144 Hz client loop, and on the simulated network with a LAN browser
// probing every second and players joining the session one a second. It answers whether the three
// ways give one digest, whether the browser sees the server by its name with its seats as they
// fill, and how long each player takes to be seated. The browser's last sighting is drawn as a
// game's menu would draw it (engine/ui), and the seats seen over time as a chart. Everything runs
// on a virtual clock off one seed.
//
// ge:summary Lab: the server core: dedicated, listen and networked runs, the LAN browser, seats
// ge:keywords lab server server-core dedicated-server listen-server lan-browser seats join
// ge:keywords digest picture preset
// ge:skill net labs
// ge:test tests/server_core_test.cpp
#include <cstdio>
#include <string>
#include <vector>

#include "core/num.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "server/trial.hpp"
#include "ui/draw.hpp"
#include "ui/font.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gs = ge::server;
namespace gu = ge::ui;

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

std::string hex(uint64_t v) {
  char out[20];
  std::snprintf(out, sizeof out, "%016llx", static_cast<unsigned long long>(v));
  return out;
}

// THE BROWSER: each preset's last sighting as a row of a game's FIND A GAME page.
gl::Picture browser_picture(const std::vector<std::pair<std::string, gs::ServerTrial>>& runs,
                            const std::string& description) {
  constexpr int kW = 1280, kH = 560;
  gu::Image img = gu::make_image(kW, kH, {0.07f, 0.09f, 0.13f, 1});
  gu::DrawList dl;
  dl.begin(kW, kH);
  const gu::Font& bold = gu::ui_font(gu::Weight::semibold);
  const gu::Font& body = gu::ui_font(gu::Weight::medium);
  const gu::Color ink{0.93f, 0.95f, 0.98f, 1}, dim{0.55f, 0.62f, 0.72f, 1};
  const gu::Color accent{0.36f, 0.70f, 1.0f, 1}, warn{1.0f, 0.72f, 0.30f, 1};
  const float x0 = 32, y0 = 32;
  dl.rect({x0, y0, kW - 64, kH - 64}, {0.12f, 0.15f, 0.21f, 1}, 14);
  dl.text(bold, 30, x0 + 24, y0 + 46, "FIND A GAME", ink);
  dl.text(body, 20, x0 + 24, y0 + 74, "on the server's LAN: one probe a second, the last answer",
          dim);
  const float cols[5] = {x0 + 24, x0 + 420, x0 + 580, x0 + 720, x0 + 900};
  const char* heads[5] = {"NAME", "PLAYERS", "LOCKED", "SEEN AT", "NETWORK"};
  for (int c = 0; c < 5; c++) dl.text(bold, 17, cols[c], y0 + 116, heads[c], dim);
  dl.rect({x0 + 16, y0 + 126, kW - 96, 2}, {0.25f, 0.30f, 0.40f, 1});
  float y = y0 + 166;
  for (size_t i = 0; i < runs.size(); i++) {
    const gs::ServerTrial& t = runs[i].second;
    if (i % 2 == 0) dl.rect({x0 + 16, y - 30, kW - 96, 64}, {0.15f, 0.19f, 0.26f, 1}, 6);
    if (t.sightings.empty()) {
      dl.text(body, 22, cols[0], y, "(not found)", warn);
    } else {
      const gs::TrialSighting& s = t.sightings.back();
      dl.text(bold, 24, cols[0], y, s.name, accent);
      dl.text(body, 16, cols[0], y + 22, description, dim);
      dl.text(body, 22, cols[1], y,
              std::to_string(s.seated) + " / " + std::to_string(s.max_players), ink);
      dl.text(body, 22, cols[2], y, s.locked ? "yes" : "no", s.locked ? warn : dim);
      char at[24];
      std::snprintf(at, sizeof at, "%.2f s", s.at_s);
      dl.text(body, 22, cols[3], y, at, ink);
    }
    dl.text(body, 22, cols[4], y, runs[i].first, ink);
    y += 72;
  }
  gu::render(dl, img);
  gl::Picture pic;
  pic.width = img.width;
  pic.height = img.height;
  pic.rgba = std::move(img.rgba);
  return pic;
}

void run(gl::Lab& lab) {
  gs::TrialConfig base;
  base.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  base.server.hostname = "Ring Night";
  base.server.description = "the sample game's ring, four walkers";
  base.server.bots = 4;
  base.server.maxclients = 8;
  base.server.seed = base.seed;
  base.server.seconds = lab.arg_num("seconds");
  base.clients = static_cast<int>(lab.arg_num("clients"));

  std::vector<std::pair<std::string, gs::ServerTrial>> runs;
  gl::LabTable ways{"the three ways to run it: the game's digest",
                    {"preset", "frames", "dedicated", "listen (144 Hz loop)", "networked", "agree"},
                    {}};
  gl::LabTable seen{"the LAN browser and the joins",
                    {"preset", "sightings", "seats first..last", "LAN answers", "players seated",
                     "slowest seat (ms)", "trial digest"},
                    {}};
  gl::LineChart line;
  line.frame = {"SEATS ON THE SERVER, EACH PRESET", "seed " + lab.arg("seed"), "t (s)", "seated"};
  line.spikes = false;
  int disagree = 0;
  for (const std::string& name : split(lab.arg("presets"))) {
    gs::TrialConfig c = base;
    if (!ge::net::conditions_preset(name, c.link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      continue;
    }
    gs::ServerTrial t = gs::run_server_trial(c);
    if (!t.error.empty()) lab.fail(name + ": " + t.error, "skill: net.");
    disagree += t.ok ? 0 : 1;
    ways.rows.push_back({name, std::to_string(t.frames), hex(t.dedicated), hex(t.listen),
                         hex(t.networked), t.ok ? "yes" : "NO"});
    double slowest = 0;
    int seated = 0;
    for (double s : t.seated_at_s) {
      if (s >= 0) seated++;
      slowest = s > slowest ? s : slowest;
    }
    const std::string seats = t.sightings.empty()
                                  ? "-"
                                  : std::to_string(t.sightings.front().seated) + ".." +
                                        std::to_string(t.sightings.back().seated);
    seen.rows.push_back(
        {name, std::to_string(t.sightings.size()), seats, std::to_string(t.lan_answers),
         std::to_string(seated) + "/" + std::to_string(t.seated_at_s.size()),
         std::to_string(static_cast<int>(ge::core::num::round(slowest * 1000))), hex(t.digest)});
    gl::Trace tr{name, t.seats_t, t.seats_n};
    line.traces.push_back(std::move(tr));
    lab.metric(name + ".sightings", static_cast<double>(t.sightings.size()), "");
    lab.metric(name + ".seated", seated, "players");
    runs.emplace_back(name, std::move(t));
  }
  lab.metric("disagreeing_runs", disagree, "presets");
  lab.table(std::move(ways));
  lab.table(std::move(seen));
  if (!runs.empty()) lab.note("banner: " + runs.front().second.banner);
  if (!runs.empty()) lab.note("last status: " + runs.front().second.status);
  if (runs.empty()) return;
  lab.shot({"browser",
            "THE SERVER IN A LAN BROWSER, EACH PRESET",
            "A dedicated server of the sample game, named by sv_hostname, as a LAN browser finds "
            "it: one row a condition preset, its last probe's answer. Four bots seat from the "
            "start and a player joins a second.",
            "the name and description as set; the seats at bots plus every player; nothing "
            "missing under a bad network.",
            {{"presets", lab.arg("presets")}, {"seed", lab.arg("seed")}}},
           browser_picture(runs, base.server.description));
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"seats",
            chart.title,
            "The seats the server counts over the run (what its LAN advert says): the four bots "
            "from the start, a step up for each player the session seats.",
            "one staircase for every preset, the bad network's steps a little later.",
            {{"presets", lab.arg("presets")}, {"seed", lab.arg("seed")}}},
           gl::to_picture(chart.plot));
}

const gl::LabSpec kSpec{
    "server-core",
    gl::LabKind::sim,
    "engine",
    "engine/server (the server core, the trial, the sample game)",
    "net",
    "Does one game's server give one digest dedicated, as a listen server and on the network? "
    "Does a LAN browser see it by its name with its seats as players join, and is every player "
    "seated?",
    {
        {"presets", "lan,wifi,bad", "condition presets, comma-separated"},
        {"seconds", "8", "server time each run, s"},
        {"clients", "3", "players joining the session, one a second"},
        {"seed", "1", "the network's and the game's stream"},
    },
    "--presets=lan,bad --seconds=4",
    run,
};
GE_LAB(kSpec);

}  // namespace
