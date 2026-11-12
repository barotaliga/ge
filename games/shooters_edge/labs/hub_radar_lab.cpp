// LAB `hub-radar`: A BOT MATCH ON THE HUB'S MAP (docs/hub.md § The live map, engine/hub/radar.hpp).
// A team match of bots on the fixture halls (net/classic_team.hpp) is recorded as a multiview
// recording, packed into a replay file version 2 and given its radar track and the level's outline
// (client/demo_radar.hpp); the hub's archive takes it, and `/radar/{sha256}` draws it. It answers
// whether every player is on the map four times a second, how little the track adds to the file,
// and whether the map reads: the match drawn frame by frame (the still view a reader steps through)
// and the animated minute on a phone.
//
// The track, the page and their sizes are deterministic; the pictures are a browser's (headless
// Chromium, GE_CHROMIUM or the container's), so they are measured.
//
// ge:summary Lab: a bots' match's radar track drawn as the hub's map, frame by frame, in Chromium
// ge:keywords lab hub-radar radar map svg bots team match frame-by-frame outline phone chromium
// ge:skill net replay labs
// ge:test tests/hub_radar_test.cpp tests/shooters_edge_demo_radar_test.cpp
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "hub/archive.hpp"
#include "hub/pages.hpp"
#include "hub/radar.hpp"
#include "lab/lab.hpp"
#include "lab/labs/hub_browser.hpp"
#include "replay/radar.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/demo_radar.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_team.hpp"

namespace {

namespace gl = ge::lab;
namespace gh = ge::hub;
namespace gr = ge::replay;
namespace cl = shooters_edge::client;
namespace cn = shooters_edge::net;
namespace fs = std::filesystem;

gh::HubSpec spec() {
  gh::HubSpec s;
  s.game = "Shooter's Edge";
  s.id = "shooters_edge";
  s.modes = {{"4on4", 9}, {"2on2", 8}, {"1on1", 7}, {"ffa", 3}};
  s.score = "frags";
  return s;
}

void run(gl::Lab& lab) {
  const double minutes = lab.arg_num("minutes");
  std::string error;
  std::vector<uint8_t> halls;
  if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &halls, &error)) {
    lab.fail("hub-radar: the fixture halls: " + error, "run tests/brush_write_test.cpp");
    return;
  }
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("hub-radar: the baseline mod: " + error,
             "run tests/shooters_edge_baseline_mod_test.cpp");
    return;
  }
  cn::TeamMatchOptions o;
  o.map = halls;
  o.map_name = "halls";
  o.entities = cn::kTeamHallsEntities;
  o.progs = &progs;
  o.frames = static_cast<int>(std::lround(minutes * 60 * 1000 / shooters_edge::bots::kPilotMsec));
  const cn::TeamMatchReport match = cn::run_team_match(o);
  if (!match.ok) {
    lab.fail("hub-radar: the match: " + match.error, "run tests/shooters_edge_team_test.cpp");
    return;
  }

  // Packed, then the radar and the outline added.
  std::vector<uint8_t> ged, with;
  if (!cl::pack_demo(match.mvd, ged, error) || !cl::add_radar(match.mvd, ged, with, error, halls)) {
    lab.fail("hub-radar: " + error, "run tests/shooters_edge_demo_radar_test.cpp");
    return;
  }
  gr::Radar r;
  if (!gr::read_radar_file(with, r, error) || !r.any) {
    lab.fail("hub-radar: the radar does not read back: " + error,
             "run tests/replay_radar_test.cpp");
    return;
  }
  const double seconds =
      r.frames.empty()
          ? 0
          : static_cast<double>(r.frames.back().time_ms - r.frames.front().time_ms) / 1000.0;
  size_t dots = 0, full = 0;
  for (const gr::RadarFrame& f : r.frames) {
    dots += f.dots.size();
    full += f.dots.size() == r.roster.size() ? 1 : 0;
  }
  lab.metric("match_s", seconds, "s");
  lab.metric("players", static_cast<double>(r.roster.size()), "players");
  lab.metric("frames", static_cast<double>(r.frames.size()), "frames");
  lab.metric("frames_per_s", seconds > 0 ? static_cast<double>(r.frames.size() - 1) / seconds : 0,
             "Hz");
  lab.metric("frames_all_on_map",
             r.frames.empty()
                 ? 0
                 : 100.0 * static_cast<double>(full) / static_cast<double>(r.frames.size()),
             "%");
  lab.metric("dots", static_cast<double>(dots), "dots");
  lab.metric("outline_lines", static_cast<double>(r.outline.size()), "lines");
  lab.metric("file_kb", static_cast<double>(ged.size()) / 1024.0, "KiB");
  lab.metric("radar_kb", static_cast<double>(with.size() - ged.size()) / 1024.0, "KiB");
  lab.metric(
      "radar_share",
      100.0 * static_cast<double>(with.size() - ged.size()) / static_cast<double>(ged.size()),
      "% of the file");
  if (r.roster.size() != 8) {
    lab.fail("the radar's roster has " + std::to_string(r.roster.size()) + " of the 8 bots",
             "next: check demo_radar's names against the recording's");
  }
  if (r.outline.empty()) {
    lab.fail("the halls gave no outline",
             "next: check map_outline's slice height (client/demo_radar.cpp)");
  }
  std::vector<uint8_t> back;
  if (!cl::unpack_demo(with, back, error) || back != match.mvd) {
    lab.fail("the file with its radar no longer unpacks to the recording",
             "next: make stream_add_track keep every other track's bytes");
  }

  // The hub's archive takes it; its pages.
  std::error_code ec;
  const fs::path dir = fs::temp_directory_path(ec) / "ge-hub-radar-lab";
  fs::remove_all(dir, ec);
  gh::Archive a;
  gh::DemoRow row;
  if (!gh::archive_open(a, dir.string(), error) ||
      gh::archive_add(a, with, 1794460000, {}, error, &row) != gh::AddResult::added) {
    lab.fail("hub-radar: the archive: " + error, "run tests/hub_archive_test.cpp");
    return;
  }
  gh::Answer page;
  gh::radar_answer(&a, spec(), "/radar/" + row.sha, page);
  lab.metric("page_kb", static_cast<double>(page.body.size()) / 1024.0, "KiB");
  if (page.code != 200)
    lab.fail("/radar/ answered " + std::to_string(page.code),
             "next: read radar_answer in engine/hub/radar.cpp");
  lab.attach("radar.html", {page.body.begin(), page.body.end()});
  gh::Site site;
  site.archive = &a;
  const std::string demos = gh::demos_page(site, spec(), "/demos");
  if (demos.find("/radar/" + row.sha) == std::string::npos) {
    lab.fail("the Demos page has no Map link for a demo with radar",
             "next: read demos_page in engine/hub/pages.cpp");
  }

  const std::string browser = lab.arg("browser") == "0" ? "" : gl::hub_browser::find_browser();
  if (browser.empty()) {
    lab.note("no headless Chromium (GE_CHROMIUM, /opt/pw-browsers/chromium): pages not drawn");
    fs::remove_all(dir, ec);
    return;
  }
  const fs::path shots = fs::temp_directory_path(ec) / "ge-hub-radar-shots";
  fs::create_directories(shots, ec);
  int drawn = 0;
  const int steps = static_cast<int>(lab.arg_num("steps"));
  for (int i = 0; i < steps; i++) {
    const double at = seconds * (i + 0.5) / steps;
    char id[32], when[32];
    std::snprintf(id, sizeof id, "at-%03d", static_cast<int>(at));
    std::snprintf(when, sizeof when, "%d:%02d", static_cast<int>(at) / 60,
                  static_cast<int>(at) % 60);
    gh::Answer still;
    gh::radar_answer(&a, spec(),
                     "/radar/" + row.sha + "?at=" + std::to_string(static_cast<int>(at)), still);
    const std::optional<gl::Picture> pic =
        gl::hub_browser::hub_draw(browser, shots, id, still.body, 900, 1180, false);
    if (!pic) continue;
    drawn++;
    lab.shot({id,
              std::string("THE MAP AT ") + when,
              "The match still at this moment (`?at=`): the eight bots on the halls' outline, "
              "the faint lines where each went over the whole match.",
              "red and blue dots with their names, a wedge for the way each faces; a dead bot "
              "faint; the dots inside the outline's rooms.",
              {{"at_s", std::to_string(static_cast<int>(at))}}},
             *pic, gl::Det::measured);
  }
  if (const std::optional<gl::Picture> pic =
          gl::hub_browser::hub_draw(browser, shots, "phone", page.body, 390, 1100, true)) {
    drawn++;
    lab.shot({"phone",
              "THE ANIMATED MINUTE ON A PHONE, DARK",
              "GET /radar/{sha256} at a phone's width in the dark scheme: the first minute "
              "animated (the screenshot catches its first moment), the minute links below.",
              "the map as wide as the screen, the legend wrapping, nothing sideways.",
              {}},
             *pic, gl::Det::measured);
  }
  lab.metric("pages_drawn", drawn, "pictures", gl::Det::measured);
  fs::remove_all(shots, ec);
  fs::remove_all(dir, ec);
}

const gl::LabSpec kSpec{
    "hub-radar",
    gl::LabKind::picture,
    "game:shooters_edge",
    "engine/hub/radar (the hub's map), engine/replay/radar (the track) and "
    "client/demo_radar.hpp (a recording's radar)",
    "net",
    "Does a bots' match's radar put every player on the hub's map four times a second for a "
    "small share of the file, and does the map read frame by frame and on a phone?",
    {
        {"minutes", "2", "the bots' match's length"},
        {"steps", "6", "the still frames drawn, evenly through the match"},
        {"browser", "1", "draw the pages with a headless Chromium when one is found (0: never)"},
    },
    "--minutes=0.25 --browser=0",
    run,
};
GE_LAB(kSpec);

}  // namespace
