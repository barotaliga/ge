// Tests: the game being played recorded (client/listen_record.hpp). The listen server on the
// fixture halls, the player walking onto the rocket launcher and firing it, recorded packed and
// plain from the same seed: the packed file unpacks to exactly the plain bytes, is far smaller,
// and plays back as a multiview recording of the map with the player where the server had him,
// his stats, his rocket's explosion and as many frames as were recorded. The file side: `record`
// writes `<game folder>/<name>.mvdz`, `stop` closes it and names it, a bad name or no folder is
// refused, and the file found by name plays.
//
// ge:summary Tests: record and stop on the listen game, packed and plain, played back
// ge:keywords test record stop listen-record mvdz multiview packed playback round-trip
// ge:skill shooter replay testing
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/listen_record.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "vfs/pak.hpp"
#include "vfs/unpack.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace fs = std::filesystem;

br::Entity entity(std::initializer_list<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  for (const auto& p : pairs) e.pairs.push_back(p);
  return e;
}

std::vector<uint8_t> halls() {
  br::Fixture f = br::sample_fixture();
  f.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "128 192 40"}, {"angle", "0"}}));
  f.entities.push_back(entity({{"classname", "weapon_rocketlauncher"}, {"origin", "128 192 0"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(f, br::Family::v29, &bytes, &error), error);
  return bytes;
}

const ge::progs::Progs& baseline() {
  static const ge::progs::Progs p = [] {
    ge::progs::Progs out;
    std::string error;
    CHECK_MSG(cl::baseline_progs(out, error), error);
    return out;
  }();
  return p;
}

cl::SearchPath halls_files() {
  cl::SearchPath files;
  std::vector<uint8_t> pak;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak({{"maps/halls.bsp", halls()}}, &pak, &error), error);
  CHECK_MSG(cl::add_archive(files, "pak0.pak", pak, error), error);
  return files;
}

bool start(cl::Listen& l, const cl::SearchPath& files) {
  std::string error;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "halls", error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return false;
  const bool ok = cl::listen_start(l, baseline(), std::move(map), {}, error);
  CHECK_MSG(ok, error);
  return ok;
}

// The scripted session: onto the launcher, the launcher in hand, a rocket at the far wall.
// `each` runs after every frame.
template <typename F>
void play(cl::Listen& l, F each) {
  sh::MoveCmd idle;
  const auto run = [&](int frames, sh::MoveCmd cmd) {
    for (int i = 0; i < frames; i++) {
      cl::listen_frame(l, cmd);
      cmd.impulse = 0;
      each();
    }
  };
  run(80, idle);
  l.impulse = 7;
  run(80, idle);
  sh::MoveCmd fire;
  fire.buttons = sh::kButtonAttack;
  run(2, fire);
  run(120, idle);
}

struct Recorded {
  std::vector<uint8_t> bytes;
  int64_t frames = 0, plain = 0;
  sh::Vec3f last_origin;
  float last_health = 0;
};

Recorded record_session(int level) {
  Recorded out;
  const cl::SearchPath files = halls_files();
  cl::Listen l;
  if (!start(l, files)) return out;
  cl::ListenRecorder r;
  cl::record_listen_begin(r, l, level);
  play(l, [&] {
    cl::record_listen_frame(r, l);
    out.bytes.insert(out.bytes.end(), r.ready.begin(), r.ready.end());
    r.ready.clear();
  });
  out.last_origin = l.server->sv.edicts[1].origin;
  out.last_health = l.server->sv.edicts[1].health;
  cl::record_listen_end(r);
  out.bytes.insert(out.bytes.end(), r.ready.begin(), r.ready.end());
  out.frames = r.frames;
  out.plain = r.plain;
  return out;
}

const Recorded& plain() {
  static const Recorded r = record_session(0);
  return r;
}
const Recorded& packed() {
  static const Recorded r = record_session(cl::kDefaultPackLevel);
  return r;
}

}  // namespace

TEST(a_packed_recording_unpacks_to_the_plain_one) {
  CHECK(plain().frames == 282 && packed().frames == 282);
  CHECK(static_cast<int64_t>(plain().bytes.size()) == plain().plain);
  ge::vfs::Unpacked u;
  std::string error;
  CHECK_MSG(ge::vfs::unpack_one(packed().bytes, u, error), error);
  CHECK(u.bytes == plain().bytes);
  CHECK(u.layers == std::vector<ge::vfs::Wrapping>{ge::vfs::Wrapping::zstd});
  CHECK(!u.truncated);
  // A recording repeats itself frame to frame; the packed file is a small part of the plain.
  CHECK_MSG(packed().bytes.size() * 8 < plain().bytes.size(),
            std::to_string(packed().bytes.size()) + " of " + std::to_string(plain().bytes.size()));
}

TEST(a_recording_plays_back_as_the_server_had_it) {
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(packed().bytes, d, error), error);
  CHECK(d.state.format == cl::DemoFormat::multiview);
  CHECK(d.layers == std::vector<ge::vfs::Wrapping>{ge::vfs::Wrapping::zstd});
  int64_t frames = 0;
  for (;;) {
    const cl::DemoRead r = cl::next_frame(d, error);
    CHECK_MSG(r != cl::DemoRead::error, error);
    if (r != cl::DemoRead::frame) break;
    frames++;
  }
  CHECK_MSG(d.state.map == "halls", d.state.map);
  CHECK_MSG(frames >= packed().frames, std::to_string(frames));
  CHECK(d.state.players.size() > 0 && d.state.players[0].present);
  const sh::Vec3f o = d.state.players[0].origin;
  const sh::Vec3f s = packed().last_origin;
  CHECK_MSG(std::abs(o.x - s.x) <= 0.125f && std::abs(o.y - s.y) <= 0.125f &&
                std::abs(o.z - s.z) <= 0.125f,
            std::to_string(o.x) + " " + std::to_string(o.y) + " " + std::to_string(o.z));
  CHECK(d.state.stats[0] == static_cast<int32_t>(packed().last_health));
  CHECK(d.state.stats[10] != 0);  // the launcher in hand
  CHECK(!d.state.names[0].empty());
}

TEST(record_and_stop_write_a_file_that_plays_by_name) {
  const fs::path root = fs::temp_directory_path() / "ge_listen_record_test";
  std::error_code stale;
  fs::remove_all(root, stale);  // a failed run's folder, if any
  fs::create_directories(root / "id1");
  {
    std::vector<uint8_t> pak;
    std::string error;
    CHECK_MSG(ge::vfs::write_pak({{"maps/halls.bsp", halls()}}, &pak, &error), error);
    std::ofstream(root / "id1" / "pak0.pak", std::ios::binary)
        .write(reinterpret_cast<const char*>(pak.data()), static_cast<std::streamsize>(pak.size()));
  }
  cl::SearchPath files;
  {
    std::string error;
    CHECK_MSG(cl::open_game_dir(root.string(), "", files, error), error);
  }
  cl::Listen l;
  if (!start(l, files)) return;
  cl::ListenRecorder r;
  cl::RecordSettings s;
  ge::console::Console con;
  cl::add_record_commands(con, r, l, files, s);
  con.execute("record bad/name");
  CHECK(!r.on);
  con.execute("stop");
  con.execute("record match1");
  CHECK(r.on);
  CHECK_MSG(r.path == (fs::path(files.folder) / "match1.mvdz").string(), r.path);
  con.execute("record again");
  CHECK_MSG(con.output().back().find("already recording") != std::string::npos,
            con.output().back());
  play(l, [&] { cl::record_tick(r, l, 1, con); });
  con.execute("stop");
  CHECK(!r.on);
  CHECK_MSG(con.output().back().find("recorded ") == 0, con.output().back());
  CHECK(fs::file_size(r.path) == static_cast<uintmax_t>(r.written));
  std::vector<uint8_t> bytes;
  {
    std::ifstream in(r.path, std::ios::binary);  // closed here: Windows removes no open file
    bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }
  CHECK(r.frames == packed().frames);  // the same session, a frame for each server frame
  // Played by its name, as `playdemo match1` finds it.
  std::vector<uint8_t> found_bytes;
  std::string found, error;
  CHECK_MSG(cl::find_demo(files, "match1", shooters_edge::profiles::classic_game().demo_extensions,
                          found_bytes, found, error),
            error);
  CHECK(found_bytes == bytes);
  cl::Demo d;
  CHECK_MSG(cl::open_demo(found_bytes, d, error), error);
  // Unpacked with demo_pack 0.
  s.pack = 0;
  con.execute("record plain1");
  CHECK_MSG(r.path == (fs::path(files.folder) / "plain1.mvd").string(), r.path);
  con.execute("stop");
  // No game folder: refused.
  cl::SearchPath none = halls_files();
  cl::ListenRecorder r2;
  CHECK(!cl::record_open(r2, l, none, "m", s, error));
  CHECK_MSG(error.find("-basedir") != std::string::npos, error);
  // The game folder's pack may still be open (Windows refuses to remove an open file): the
  // error_code form leaves the scratch folder for the next run's remove_all instead of throwing.
  std::error_code ec;
  fs::remove_all(root, ec);
}
