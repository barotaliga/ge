// LAB `se-q1-speedruns`: how do Quake's fastest players clear each map, at 100% and at any %?
// Reads a folder of recorded single-player runs (`.dem`, or packed `.dz`: open_demo unpacks it),
// each one map from its start to its exit, and draws what the campaign bot's planner can learn
// from them: the route (the eye every half second), where each kill and each secret counted, the
// boosts (the eye rising faster than any jump or stair can lift it: a rocket or grenade jump, a
// push), and the time to the exit (the intermission's start on the demo's clock).
//
// A file's name says its run when it is `<skill><kind>_<map>_<anything>`: skill E (easy) or N
// (nightmare), kind H (100%: every kill and secret) or R (any %, the exit as soon as it is open).
// `--only` keeps the kinds asked for (default H: the 100% runs the bot is measured against).
//
// The table, a run a row: the map, the kind, the time to the exit, the level's kills and secrets as
// the run's own counters left them, the boosts. Pictures: each run on the live map's plate. The
// attachment `routes.tsv`: every run's track, kills, secrets and boosts as rows (the planner's
// data: `run map kind t x y z what`).
//
// ge:summary Lab: recorded fast runs of Quake's maps read: route, kills, secrets, boosts, exit time
// ge:keywords lab se-q1-speedruns speedrun demo route-data 100-percent any-percent boosts
// ge:keywords rocket-jump exit-time campaign planner dzip
// ge:skill shooter bots labs
// ge:test tests/shooters_edge_companion_test.cpp
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "classicenhance/source.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/livemap/route.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace ce = ge::classicenhance;
namespace lv = shooters_edge::livemap;
namespace cl = shooters_edge::client;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;
using P3 = std::array<float, 3>;

constexpr double kTrackStep_s = 0.5;  // the route's point spacing on the demo's clock
constexpr double kBoostWindow_s = 0.3;
constexpr float kBoostRise = 150;  // units in the window: a jump lifts ~45, stairs at a run ~95
constexpr float kTeleport = 400;   // a step this long between frames is a teleporter

struct Run {
  std::string file, map, kind, skill;
  double exit_s = -1;  // the intermission's start, demo clock from the first frame (-1: none)
  int kills = -1, monsters = -1, secrets = -1, total_secrets = -1;
  std::vector<P3> track, kill_at, secret_at, boosts;
  std::vector<double> kill_t, secret_t, boost_t;
  std::string error;
};

P3 at(const cl::DemoState& s) { return {s.eye.x, s.eye.y, s.eye.z}; }

// `name` as `<skill><kind>_<map>_...` (EH_e1m1_048.dem); empty fields when it is not.
void name_parts(const std::string& stem, Run& r) {
  const size_t a = stem.find('_');
  if (a == 2 && (stem[0] == 'E' || stem[0] == 'N') && (stem[1] == 'H' || stem[1] == 'R')) {
    r.skill = stem[0] == 'E' ? "easy" : "nightmare";
    r.kind = stem[1] == 'H' ? "100%" : "any%";
  }
}

Run read_run(const std::filesystem::path& path) {
  Run r;
  r.file = path.filename().string();
  name_parts(path.stem().string(), r);
  std::vector<uint8_t> bytes;
  if (FILE* f = std::fopen(path.string().c_str(), "rb")) {
    std::array<uint8_t, 65536> buf;
    size_t n = 0;
    while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0)
      bytes.insert(bytes.end(), buf.begin(), buf.begin() + static_cast<long>(n));
    std::fclose(f);
  }
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, r.error)) return r;
  double t0 = -1, next_point = 0, last_t = -1;
  int last_kills = -1, last_secrets = -1;
  P3 last{};
  std::vector<std::pair<double, P3>> recent;  // the eye over the boost window
  double last_boost = -10;
  for (;;) {
    std::string error;
    const cl::DemoRead rd = cl::next_frame(d, error);
    if (rd == cl::DemoRead::error) {
      r.error = error;
      break;
    }
    if (rd == cl::DemoRead::end) break;
    const cl::DemoState& s = d.state;
    if (s.map.empty()) continue;
    if (r.map.empty()) r.map = s.map;
    if (s.map != r.map) break;  // the next level: this run is over
    if (t0 < 0) t0 = s.time;
    const double t = s.time - t0;
    const P3 p = at(s);
    r.monsters = s.stats[cl::kStatTotalMonsters];
    r.total_secrets = s.stats[cl::kStatTotalSecrets];
    const int k = s.stats[cl::kStatMonsters], sc = s.stats[cl::kStatSecrets];
    if (last_kills >= 0)
      for (int i = last_kills; i < k; i++) {
        r.kill_at.push_back(p);
        r.kill_t.push_back(t);
      }
    if (last_secrets >= 0)
      for (int i = last_secrets; i < sc; i++) {
        r.secret_at.push_back(p);
        r.secret_t.push_back(t);
      }
    last_kills = k;
    last_secrets = sc;
    r.kills = k;
    r.secrets = sc;
    if (s.intermission) {
      r.exit_s = t;
      break;
    }
    if (t <= last_t) continue;  // a frame on the same tic
    const float dx = p[0] - last[0], dy = p[1] - last[1], dz = p[2] - last[2];
    if (last_t >= 0 && dx * dx + dy * dy + dz * dz > kTeleport * kTeleport) recent.clear();
    last_t = t;
    last = p;
    recent.push_back({t, p});
    while (!recent.empty() && recent.front().first < t - kBoostWindow_s)
      recent.erase(recent.begin());
    if (!recent.empty() && p[2] - recent.front().second[2] > kBoostRise && t - last_boost > 1) {
      r.boosts.push_back(recent.front().second);
      r.boost_t.push_back(recent.front().first);
      last_boost = t;
    }
    if (t >= next_point) {
      r.track.push_back(p);
      next_point += kTrackStep_s;
    }
  }
  // The first frame can still hold the eye before the player is placed: drop a first point that
  // a teleporter's step lies from the next.
  if (r.track.size() > 1) {
    const P3 a = r.track[0], b = r.track[1];
    const float dx = b[0] - a[0], dy = b[1] - a[1];
    if (dx * dx + dy * dy > kTeleport * kTeleport) r.track.erase(r.track.begin());
  }
  return r;
}

std::string of(int a, int b) {
  return a < 0 || b < 0 ? "-" : std::to_string(a) + "/" + std::to_string(b);
}

void run(gl::Lab& lab) {
  const std::string dir = lab.arg("demos");
  if (dir.empty()) {
    lab.note("no --demos folder: nothing to read (the smoke run)");
    lab.metric("runs", 0, "count");
    return;
  }
  std::vector<std::filesystem::path> files;
  std::error_code ec;
  for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
    const std::string x = e.path().extension().string();
    if (x == ".dem" || x == ".dz") files.push_back(e.path());
  }
  if (ec) {
    lab.fail("se-q1-speedruns: the folder " + dir + " does not read: " + ec.message(),
             "next: pass --demos=<a folder of .dem or .dz files>");
    return;
  }
  std::sort(files.begin(), files.end());
  const std::string only = lab.arg("only"), maps = lab.arg("maps");
  std::vector<uint8_t> content;
  ge::vfs::Pak pak;
  const std::string& game = lab.arg("game");
  if (!game.empty()) {
    ce::Source src;
    std::string error;
    if (!ce::open_source(game, &src, &error)) {
      lab.fail("se-q1-speedruns: " + error,
               "next: pass --game=<a release zip, a pack or a folder>");
      return;
    }
    content = src.content();
    if (!ge::vfs::read_pak(content, &pak, &error)) {
      lab.fail("se-q1-speedruns: the pack does not read: " + error, "next: check the --game path");
      return;
    }
  }
  std::vector<Run> runs;
  for (const auto& f : files) {
    const std::string stem = f.stem().string();
    if (stem.size() >= 2 && only != "all" && only.find(stem[1]) == std::string::npos) continue;
    if (!maps.empty()) {
      bool hit = false;
      for (size_t a = 0; a <= maps.size();) {
        size_t b = maps.find(',', a);
        if (b == std::string::npos) b = maps.size();
        const std::string m = maps.substr(a, b - a);
        if (!m.empty() && stem.find("_" + m + "_") != std::string::npos) hit = true;
        a = b + 1;
      }
      if (!hit) continue;
    }
    runs.push_back(read_run(f));
  }
  const In in = {{"demos", "the folder given"}, {"only", only}, {"maps", maps}};
  gl::LabTable t{
      "each recorded run",
      {"file", "map", "kind", "skill", "exit s", "kills", "secrets", "boosts", "points", "read"},
      {}};
  std::string tsv = "run\tmap\tkind\tt\tx\ty\tz\twhat\n";
  const auto rows = [&](const Run& r, const std::vector<P3>& ps, const std::vector<double>* ts,
                        const char* what) {
    for (size_t i = 0; i < ps.size(); i++) {
      const double tt = ts != nullptr ? (*ts)[i] : static_cast<double>(i) * kTrackStep_s;
      tsv += r.file + "\t" + r.map + "\t" + r.kind + "\t" + to_fixed(tt, 2) + "\t" +
             to_fixed(ps[i][0], 0) + "\t" + to_fixed(ps[i][1], 0) + "\t" + to_fixed(ps[i][2], 0) +
             "\t" + what + "\n";
    }
  };
  int read = 0;
  for (const Run& r : runs) {
    read += r.error.empty() ? 1 : 0;
    t.rows.push_back({r.file, r.map, r.kind, r.skill, r.exit_s >= 0 ? to_fixed(r.exit_s, 1) : "-",
                      of(r.kills, r.monsters), of(r.secrets, r.total_secrets),
                      std::to_string(r.boosts.size()), std::to_string(r.track.size()),
                      r.error.empty() ? "yes" : r.error});
    rows(r, r.track, nullptr, "track");
    rows(r, r.kill_at, &r.kill_t, "kill");
    rows(r, r.secret_at, &r.secret_t, "secret");
    rows(r, r.boosts, &r.boost_t, "boost");
  }
  lab.table(t);
  lab.metric("runs", static_cast<double>(runs.size()), "count");
  lab.metric("runs.read", read, "count");
  lab.attach("routes.tsv", std::vector<uint8_t>(tsv.begin(), tsv.end()));
  if (game.empty() || lab.arg("pictures") == "0") return;
  const ge::vfs::PakEntry* pal = ge::vfs::pak_find(pak, "gfx/palette.lmp");
  for (const Run& r : runs) {
    const ge::vfs::PakEntry* bsp = ge::vfs::pak_find(pak, ("maps/" + r.map + ".bsp").c_str());
    if (pal == nullptr || bsp == nullptr || r.track.empty()) continue;
    lv::Plate plate;
    std::string error;
    if (!lv::build_plate(ge::vfs::pak_file(content, *bsp), r.map, ge::vfs::pak_file(content, *pal),
                         1400, &plate, &error)) {
      lab.note(r.file + ": no plate: " + error);
      continue;
    }
    lv::RouteMarks marks;
    marks.track = r.track;
    marks.kills = r.kill_at;
    marks.secrets = r.secret_at;
    marks.exits = r.boosts;  // the rings: where a boost lifted him
    const ge::ui::Image img = lv::draw_route(plate, marks);
    gl::Picture p;
    p.width = img.width;
    p.height = img.height;
    p.rgba = img.rgba;
    std::string stem = r.file.substr(0, r.file.find('.'));
    std::replace(stem.begin(), stem.end(), '_', '-');
    lab.shot(
        {"run-" + stem,
         "RUN " + r.map + " " + r.kind + " " + r.skill + ", " +
             (r.exit_s >= 0 ? to_fixed(r.exit_s, 1) + " S" : std::string("NO EXIT")) + ", KILLS " +
             of(r.kills, r.monsters) + ", SECRETS " + of(r.secrets, r.total_secrets),
         "A recorded fast run on the live map's plate: the route (blue at the start to red "
         "at the exit), where kills counted (crosses, at the player), where secrets counted "
         "(gold diamonds) and where a boost lifted him (gold rings: a rocket or grenade "
         "jump, a push).",
         "the route ends at the exit and every boost sits where a ledge is otherwise out of "
         "reach",
         in},
        p);
  }
}

const gl::LabSpec kSpec{
    "se-q1-speedruns",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/client/demo.hpp read for routes: Quake's recorded fast runs as the "
    "campaign planner's data",
    "bots",
    "How do the fastest recorded runs clear each map: in what order, by which boosts, and in how "
    "long?",
    {
        {"demos", "", "a folder of recorded runs (.dem or .dz; none: the smoke run reads nothing)"},
        {"game", "", "the player's own release zip, pack or folder: the plates the routes go on"},
        {"only", "H", "the kinds by the file name's second letter: H 100%, R any %, HR or all"},
        {"maps", "", "a comma list of maps to keep (none: every map)"},
        {"pictures", "1", "0: the table and routes.tsv alone"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
