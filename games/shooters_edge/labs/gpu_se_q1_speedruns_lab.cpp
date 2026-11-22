// LAB `se-q1-speedruns`: how do Quake's fastest players clear each map, at 100% and at any %?
// Reads a folder of recorded single-player runs (`.dem`, or packed `.dz`: open_demo unpacks it),
// each one map from its start to its exit, and draws what the campaign bot's planner can learn
// from them: the route (the eye every fifth of a second), where each kill and each secret counted,
// the blast jumps (a blow taken with the rocket or grenade launcher in hand and then a speed no run
// or jump reaches: the flight from take-off to landing), boosts (the eye rising faster than any
// jump or stair can lift it: a blast jump, a push), and the time to the exit (the intermission's
// start on the demo's clock).
//
// A file's name says its run when it is `<skill><kind>_<map>_<anything>`: skill E (easy) or N
// (nightmare), kind H (100%: every kill and secret) or R (any %, the exit as soon as it is open).
// `--only` keeps the kinds asked for (default H: the 100% runs the bot is measured against).
//
// The table, a run a row: the map, the kind, the time to the exit, the level's kills and secrets as
// the run's own counters left them, the boosts. Pictures: each run on the live map's plate. The
// attachment `routes.tsv`: every run's track, kills, secrets, boosts and blast jumps (take-off,
// then landing) as rows (the planner's data: `run map kind t x y z what`).
//
// ge:summary Lab: recorded fast runs of Quake's maps read: route, kills, secrets, blast jumps, exit
// ge:keywords lab se-q1-speedruns speedrun demo route-data 100-percent any-percent boosts
// ge:keywords rocket-jump grenade-jump
// ge:keywords rocket-jump exit-time campaign planner dzip
// ge:skill shooter bots labs
// ge:test tests/shooters_edge_companion_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "classicenhance/source.hpp"
#include "core/libm/libm.hpp"
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

constexpr double kTrackStep_s = 0.2;  // the route's point spacing on the demo's clock
constexpr double kBoostWindow_s = 0.3;
constexpr float kBoostRise = 150;  // units in the window: a jump lifts ~45, stairs at a run ~95
constexpr float kTeleport = 400;   // a step this long between frames is a teleporter
// A BLAST JUMP: a blow taken with the rocket or grenade launcher in hand (the game's item bits for
// them in the active-weapon stat), then within `kBlastWithin_s` a speed no run or jump reaches
// (`kBlastSpeed` across, or `kBlastRise` upward); its flight is followed until the eye stops
// falling.
constexpr int kItemGrenadeLauncher = 16, kItemRocketLauncher = 32;
constexpr double kBlastWithin_s = 0.4;
constexpr float kBlastSpeed = 600;  // u/s: a run is 320, a bunny hop rarely past 500
constexpr float kBlastRise = 400;   // u/s up: a jump leaves the ground at 270
constexpr double kBlastLongest_s = 4;
constexpr float kTrickPitch = 25;  // a rocket jump's view at least this far down, degrees

struct Run {
  std::string file, map, kind, skill;
  double exit_s = -1;  // the intermission's start, demo clock from the first frame (-1: none)
  int kills = -1, monsters = -1, secrets = -1, total_secrets = -1;
  std::vector<P3> track, kill_at, secret_at, boosts;
  std::vector<double> kill_t, secret_t, boost_t;
  // Blast jumps: each one's flight (take-off first, landing last), its start time and launcher.
  std::vector<std::vector<P3>> blasts;
  std::vector<double> blast_t;
  std::vector<bool> blast_grenade;
  // And each one's shot as the nav's tricks take it (map_book.hpp `RocketTrick`): the view's pitch
  // down, the run across before the blow, units/s, and whether the shot went behind (the flip).
  std::vector<float> blast_pitch, blast_run;
  std::vector<bool> blast_flip;
  float view_height = 22;
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
  // A blow with a launcher in hand waits up to kBlastWithin_s for the speed that makes it a jump.
  int64_t blows = 0;
  double blow_t = -10;
  P3 blow_at{};
  bool blow_grenade = false, blow_flip = false;
  float blow_pitch = 0, blow_run = 0;
  float run_x = 0, run_y = 0;  // the run across over the last tic, units/s
  P3 stood{};  // where the eye last held its height a tic (standing or running on a floor)
  double stood_t = -10;
  std::vector<P3> flight;  // a blast jump in the air (empty: none)
  double flight_t = 0, still_since = -1;
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
    const int weapon = s.stats[cl::kStatActiveWeapon];
    if (s.blows_taken > blows &&
        (weapon == kItemRocketLauncher || weapon == kItemGrenadeLauncher) && flight.empty()) {
      blow_t = t;
      // The takeoff: where he last stood (the eye level a tic) within half a second, as a jump
      // before the shot has him in the air when the blow lands.
      blow_at = last_t >= 0 ? (t - stood_t <= 0.5 ? stood : last) : p;
      blow_grenade = weapon == kItemGrenadeLauncher;
      blow_pitch = s.angles.x;
      blow_run = std::sqrt(run_x * run_x + run_y * run_y);
      // Behind: the view's yaw more than a right angle off the run's heading.
      const double heading = ge::core::libm::atan2(run_y, run_x) * (180 / 3.141592653589793);
      double off = s.angles.y - heading;
      while (off > 180) off -= 360;
      while (off < -180) off += 360;
      blow_flip = blow_run > 100 && (off > 90 || off < -90);
    }
    r.view_height = s.view_height;
    blows = s.blows_taken;
    if (t <= last_t) continue;  // a frame on the same tic
    const float dx = p[0] - last[0], dy = p[1] - last[1], dz = p[2] - last[2];
    const bool teleported = last_t >= 0 && dx * dx + dy * dy + dz * dz > kTeleport * kTeleport;
    if (teleported) recent.clear();
    const auto dt = static_cast<float>(t - last_t);
    if (!flight.empty()) {
      // In the air: followed until the eye has stopped falling for a tenth of a second (landed),
      // a teleporter takes him, or the flight runs past its longest.
      const bool done = teleported || t - flight_t > kBlastLongest_s;
      if (!done) {
        flight.push_back(p);
        if (dz > -0.5f && dz < 0.5f && t - flight_t > 0.15) {
          if (still_since < 0) still_since = t;
        } else {
          still_since = -1;
        }
      }
      if (done || (still_since >= 0 && t - still_since >= 0.1)) {
        r.blasts.push_back(flight);
        flight.clear();
        still_since = -1;
      }
    } else if (last_t >= 0 && t - blow_t <= kBlastWithin_s && !teleported) {
      const float v2 = (dx * dx + dy * dy) / (dt * dt);  // across: a fall is no blast
      if (v2 > kBlastSpeed * kBlastSpeed || dz / dt > kBlastRise) {
        flight = {blow_at, p};
        flight_t = blow_t;
        r.blast_t.push_back(blow_t);
        r.blast_grenade.push_back(blow_grenade);
        r.blast_pitch.push_back(blow_pitch);
        r.blast_run.push_back(blow_run);
        r.blast_flip.push_back(blow_flip);
        blow_t = -10;
      }
    }
    if (last_t >= 0 && !teleported && dz > -0.5f && dz < 0.5f && flight.empty()) {
      stood = p;
      stood_t = t;
    }
    if (last_t >= 0 && !teleported) {
      run_x = dx / dt;
      run_y = dy / dt;
    }
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
  if (flight.size() >= 2) r.blasts.push_back(flight);
  // The first frames can still hold the eye before the player is placed: the points before a
  // teleporter's step in the first two seconds are dropped.
  for (size_t i = 0; i + 1 < r.track.size() && i < 10; i++) {
    const P3 a = r.track[i], b = r.track[i + 1];
    const float dx = b[0] - a[0], dy = b[1] - a[1];
    if (dx * dx + dy * dy > kTeleport * kTeleport) {
      r.track.erase(r.track.begin(), r.track.begin() + static_cast<long>(i) + 1);
      break;
    }
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
  gl::LabTable t{"each recorded run",
                 {"file", "map", "kind", "skill", "exit s", "kills", "secrets", "rocket jumps",
                  "grenade jumps", "boosts", "points", "read"},
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
    t.rows.push_back(
        {r.file, r.map, r.kind, r.skill, r.exit_s >= 0 ? to_fixed(r.exit_s, 1) : "-",
         of(r.kills, r.monsters), of(r.secrets, r.total_secrets),
         std::to_string(std::count(r.blast_grenade.begin(), r.blast_grenade.end(), false)),
         std::to_string(std::count(r.blast_grenade.begin(), r.blast_grenade.end(), true)),
         std::to_string(r.boosts.size()), std::to_string(r.track.size()),
         r.error.empty() ? "yes" : r.error});
    rows(r, r.track, nullptr, "track");
    rows(r, r.kill_at, &r.kill_t, "kill");
    rows(r, r.secret_at, &r.secret_t, "secret");
    rows(r, r.boosts, &r.boost_t, "boost");
    for (size_t i = 0; i < r.blasts.size(); i++) {
      const char* what = r.blast_grenade[i] ? "grenade-jump" : "rocket-jump";
      const std::vector<double> at_t(r.blasts[i].size(), r.blast_t[i]);
      std::vector<P3> ends = {r.blasts[i].front(), r.blasts[i].back()};
      rows(r, ends, &at_t, what);  // the take-off, then the landing
    }
  }
  lab.table(t);
  lab.metric("runs", static_cast<double>(runs.size()), "count");
  lab.metric("runs.read", read, "count");
  lab.attach("routes.tsv", std::vector<uint8_t>(tsv.begin(), tsv.end()));
  // The 100% runs' blast jumps as the nav's trick rows
  // (games/shooters_edge/bots/rocket_tricks.cpp): map, takeoff and landing origins, pitch, run, how
  // many runs made it, flip, level (a flip at a run past 400 expert, any other run intermediate, a
  // standing one amateur). One a map within 64 units of another's takeoff and landing is counted
  // with it.
  struct Row {
    std::string map;
    P3 from, to;
    float pitch, run;
    int jumps;
    bool flip;
    int level;
  };
  std::vector<Row> tricks;
  for (const Run& r : runs) {
    if (r.kind != "100%") continue;
    for (size_t i = 0; i < r.blasts.size(); i++) {
      // A rocket fired under his own feet looks down (a blow from a monster's with the launcher
      // in hand looks anywhere): the rest are no trick of his.
      if (!r.blast_grenade[i] && r.blast_pitch[i] < kTrickPitch) continue;
      P3 a = r.blasts[i].front(), b = r.blasts[i].back();
      a[2] -= r.view_height;
      b[2] -= r.view_height;
      const auto near = [](const P3& u, const P3& v) {
        const float x = u[0] - v[0], y = u[1] - v[1], z = u[2] - v[2];
        return x * x + y * y + z * z < 64 * 64;
      };
      bool merged = false;
      for (Row& tr : tricks)
        if (tr.map == r.map && near(tr.from, a) && near(tr.to, b)) {
          tr.jumps++;
          merged = true;
          break;
        }
      if (merged) continue;
      const float run = r.blast_run[i];
      const bool flip = r.blast_flip[i];
      tricks.push_back({r.map, a, b, r.blast_pitch[i], run, 1, flip,
                        flip && run > 400 ? 2
                        : run > 100       ? 1
                                          : 0});
    }
  }
  // A map's rows together (rocket_tricks.cpp finds them as one run), in the order read.
  std::stable_sort(tricks.begin(), tricks.end(),
                   [](const Row& a, const Row& b) { return a.map < b.map; });
  std::string trick_rows;
  for (const Row& tr : tricks) {
    const auto v = [](const P3& p) {
      return "{" + to_fixed(p[0], 0) + ", " + to_fixed(p[1], 0) + ", " + to_fixed(p[2], 0) + "}";
    };
    trick_rows += "    {\"" + tr.map + "\", " + v(tr.from) + ", " + v(tr.to) + ", " +
                  to_fixed(tr.pitch, 0) + ", " + to_fixed(tr.run, 0) + ", " +
                  std::to_string(tr.jumps) + ", " + (tr.flip ? "true" : "false") + ", " +
                  std::to_string(tr.level) + "},\n";
  }
  lab.attach("tricks.txt", std::vector<uint8_t>(trick_rows.begin(), trick_rows.end()));
  lab.metric("tricks", static_cast<double>(tricks.size()), "count");
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
    marks.blasts = r.blasts;
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
             of(r.kills, r.monsters) + ", SECRETS " + of(r.secrets, r.total_secrets) +
             ", BLAST JUMPS " + std::to_string(r.blasts.size()),
         "A recorded fast run on the live map's plate: the route (blue at the start to red "
         "at the exit), where kills counted (crosses, at the player), where secrets counted "
         "(gold diamonds) and each rocket or grenade jump (orange: the flight from the "
         "filled take-off to the white landing).",
         "the route ends at the exit and every blast jump lands where a ledge or a gap is "
         "otherwise out of reach",
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
    "How do the fastest recorded runs clear each map: in what order, by which rocket and grenade "
    "jumps, and in how "
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
