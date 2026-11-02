// LAB `se-record`: THE GAME BEING PLAYED, RECORDED. The listen server the window runs
// (client/listen.hpp) on the fixture halls: the player picks up the rocket launcher, then runs
// rings round the hall firing a rocket every couple of seconds, for `--seconds`. Two recorders
// (client/listen_record.hpp) take every frame of the same server: one plain (`demo_pack 0`, a
// `.mvd`), one packed as it goes (the default, `.mvdz` at zstd 19). The packed file must unpack to
// the plain one's bytes exactly, and play back (client/demo.hpp) with the player where the server
// had him, every frame.
//
// What it shows: the two files growing over the match (the packed one a thin line under the plain),
// the player's path from above as the server ran it and as the recording plays it back (one line on
// the other), and the cost of recording a frame. The recording, the round trip and the playback are
// deterministic; the sizes (the zstd library's) and the times are measured. The packed recording
// and the fixture's pack are attached, so `se-timedemo --basedir` can draw it in the window's
// frame.
//
// ge:summary Lab: the listen game recorded plain and packed, the sizes over time, played back
// ge:keywords lab se-record record listen-record mvdz packed sizes ratio playback from-above
// ge:skill shooter replay labs
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/listen_record.hpp"
#include "vfs/pak.hpp"
#include "vfs/unpack.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace br = ge::brush;
namespace sh = ge::shooter;
using ge::core::num::to_fixed;

br::Entity entity(std::initializer_list<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  for (const auto& p : pairs) e.pairs.push_back(p);
  return e;
}

void run(gl::Lab& lab) {
  const double seconds = std::clamp(lab.arg_num("seconds"), 1.0, 600.0);
  const std::vector<std::pair<std::string, std::string>> in = {{"seconds", to_fixed(seconds, 0)}};
  br::Fixture f = br::sample_fixture();
  f.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "128 192 40"}, {"angle", "0"}}));
  f.entities.push_back(entity({{"classname", "weapon_rocketlauncher"}, {"origin", "128 192 0"}}));
  std::vector<uint8_t> bsp, pak;
  std::string error;
  if (!br::write_bsp(f, br::Family::v29, &bsp, &error) ||
      !ge::vfs::write_pak({{"maps/halls.bsp", bsp}}, &pak, &error)) {
    lab.fail("se-record: the fixture halls: " + error, "next: run tests/brush_write_test.cpp");
    return;
  }
  cl::SearchPath files;
  ge::progs::Progs progs;
  cl::Listen l;
  std::unique_ptr<cl::LoadedMap> map;
  if (cl::add_archive(files, "pak0.pak", pak, error))
    map = cl::load_named_map(files, "halls", error);
  if (!map || !cl::baseline_progs(progs, error) ||
      !cl::listen_start(l, progs, std::move(map), {}, error)) {
    lab.fail("se-record: the listen server did not start: " + error,
             "next: run tests/shooters_edge_listen_test.cpp");
    return;
  }

  cl::ListenRecorder plain, packed;
  plain.game_dir = packed.game_dir = "id1";
  cl::record_listen_begin(plain, l, 0);
  cl::record_listen_begin(packed, l, cl::kDefaultPackLevel);
  std::vector<uint8_t> plain_file = plain.ready, packed_file = packed.ready;
  plain.ready.clear();
  packed.ready.clear();
  gl::Trace plain_kib{"plain .mvd", {}, {}}, packed_kib{".mvdz (zstd 19)", {}, {}};
  gl::Trace server_path{"the server", {}, {}};
  double record_s = 0;
  const double start = l.server->sv.time;
  const int frames = static_cast<int>(seconds / cl::listen_tic(l));
  for (int i = 0; i < frames; i++) {
    sh::MoveCmd cmd;
    if (i == 80) l.impulse = 7;  // the launcher, picked up at the spawn, in hand
    if (i > 160) {               // rings round the hall, a rocket every 150 frames
      cmd.forward = 320;
      l.view.y = static_cast<float>(std::fmod(l.view.y + 1.2, 360.0));
      if (i % 150 == 0) cmd.buttons = sh::kButtonAttack;
    }
    cl::listen_frame(l, cmd);
    cl::record_listen_frame(plain, l);
    const auto t0 = std::chrono::steady_clock::now();
    cl::record_listen_frame(packed, l);
    record_s += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    plain_file.insert(plain_file.end(), plain.ready.begin(), plain.ready.end());
    packed_file.insert(packed_file.end(), packed.ready.begin(), packed.ready.end());
    plain.ready.clear();
    packed.ready.clear();
    if (i % 20 == 0) {
      const double t = l.server->sv.time - start;
      plain_kib.x.push_back(t);
      plain_kib.y.push_back(static_cast<double>(plain_file.size()) / 1024);
      packed_kib.x.push_back(t);
      packed_kib.y.push_back(static_cast<double>(packed_file.size()) / 1024);
    }
    if (i % 10 == 0) {
      server_path.x.push_back(l.server->sv.edicts[1].origin.x);
      server_path.y.push_back(l.server->sv.edicts[1].origin.y);
    }
  }
  cl::record_listen_end(plain);
  cl::record_listen_end(packed);
  plain_file.insert(plain_file.end(), plain.ready.begin(), plain.ready.end());
  packed_file.insert(packed_file.end(), packed.ready.begin(), packed.ready.end());

  ge::vfs::Unpacked u;
  const bool unpacked = ge::vfs::unpack_one(packed_file, u, error);
  const bool same = unpacked && u.bytes == plain_file;
  lab.metric("record.frames", static_cast<double>(packed.frames), "count");
  lab.metric("record.round-trip", same ? 1 : 0, "bool");
  const double ratio = static_cast<double>(plain_file.size()) /
                       static_cast<double>(std::max<size_t>(1, packed_file.size()));
  lab.metric("record.plain-bytes", static_cast<double>(plain_file.size()), "bytes",
             gl::Det::measured);
  lab.metric("record.packed-bytes", static_cast<double>(packed_file.size()), "bytes",
             gl::Det::measured);
  lab.metric("record.ratio", ratio, "x", gl::Det::measured);
  lab.metric("record.frame-us", record_s / std::max(1, frames) * 1e6, "us", gl::Det::measured);
  if (!same) {
    lab.fail("se-record: the packed recording does not unpack to the plain one (" + error + ")",
             "next: run tests/shooters_edge_listen_record_test.cpp");
    return;
  }

  // Played back: the player's path off the recording, against the server's.
  cl::Demo d;
  if (!cl::open_demo(packed_file, d, error)) {
    lab.fail("se-record: the packed recording does not open: " + error,
             "next: run tests/shooters_edge_listen_record_test.cpp");
    return;
  }
  gl::Trace played{"played back", {}, {}};
  int64_t read = 0;
  for (;;) {
    const cl::DemoRead r = cl::next_frame(d, error);
    if (r == cl::DemoRead::error) {
      lab.fail("se-record: the recording does not read: " + error,
               "next: run tests/shooters_edge_listen_record_test.cpp");
      return;
    }
    if (r == cl::DemoRead::end) break;
    // The first frame read is the signon's alone; the recorded frames follow it.
    if (read > 0 && (read - 1) % 10 == 0 && !d.state.players.empty() &&
        d.state.players[0].present) {
      played.x.push_back(d.state.players[0].origin.x);
      played.y.push_back(d.state.players[0].origin.y);
    }
    read++;
  }
  double worst = 0;
  for (size_t k = 0; k < std::min(played.x.size(), server_path.x.size()); k++)
    worst = std::max({worst, std::abs(played.x[k] - server_path.x[k]),
                      std::abs(played.y[k] - server_path.y[k])});
  lab.metric("playback.frames", static_cast<double>(read), "count");
  lab.metric("playback.worst-offset", worst, "u");
  if (worst > 0.125) {
    lab.fail("se-record: the played-back player strays " + to_fixed(worst, 3) +
                 " units from the server's",
             "next: the recorder's origins are eighths of a unit; check listen_record.cpp");
  }

  gl::LabTable t{"the two files", {"file", "KiB", "ratio", "record a frame"}, {}};
  t.rows.push_back({"match1.mvd (demo_pack 0)",
                    to_fixed(static_cast<double>(plain_file.size()) / 1024.0, 0), "1.00x", "-"});
  t.rows.push_back({"match1.mvdz (demo_pack 1, level 19)",
                    to_fixed(static_cast<double>(packed_file.size()) / 1024.0, 0),
                    to_fixed(ratio, 2) + "x",
                    to_fixed(record_s / std::max(1, frames) * 1e6, 0) + " us"});
  lab.table(t, gl::Det::measured);

  gl::LineChart sizes;
  sizes.spikes = false;
  sizes.frame = {"THE RECORDING'S FILE OVER THE MATCH",
                 "the same frames written plain and packed as they are recorded (" +
                     to_fixed(ratio, 1) + "x smaller packed)",
                 "the match's clock (s)",
                 "file (KiB)",
                 960,
                 420};
  sizes.traces = {plain_kib, packed_kib};
  const gl::Chart sc = gl::line_chart(sizes);
  lab.shot({"sizes", sc.title,
            "Both files as the recorders write them, frame by frame. The packed one is given to a "
            "zstd stream that flushes every 2 s of play (or MiB of recording), so a crash loses at "
            "most the last 2 s; each flush costs a little of the ratio.",
            "the packed line stays far under the plain one, rising in a step at each flush", in},
           gl::to_picture(sc.plot), gl::Det::measured);
  gl::LineChart above;
  above.spikes = false;
  above.frame = {"THE PLAYER FROM ABOVE",
                 "as the server ran him, and as the packed recording plays him back",
                 "x (units)",
                 "y (units)",
                 720,
                 720};
  above.traces = {server_path, played};
  const gl::Chart ac = gl::line_chart(above);
  lab.shot({"from-above", ac.title,
            "The player's origin every tenth frame: the server's own, and the playback's off the "
            ".mvdz. He spawns, takes the launcher and runs rings round the hall.",
            "the two lines lie on each other (they may differ by an eighth of a unit at most)", in},
           gl::to_picture(ac.plot));
  lab.attach("match1.mvdz", packed_file, gl::Det::measured);
  lab.attach("pak0.pak", pak);
}

const gl::LabSpec kSpec{
    "se-record",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/client: the console's record and stop (listen_record.hpp), plain and "
    "packed",
    "shooter",
    "How small is a recording of the game being played, packed as it is written, and does it play "
    "back as the server ran it?",
    {{"seconds", "30", "the match's length, s"}},
    "--seconds=3",
    run,
};
GE_LAB(kSpec);

}  // namespace
