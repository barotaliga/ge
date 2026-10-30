// Tests: recorded games read back and benchmarked (games/shooters_edge/client/demo.hpp,
// timedemo.hpp). A scripted eye path round the halls written in both formats (demo_write.hpp)
// reads back frame for frame: the map, the eye over the origin by the view height, the angles,
// the HUD's status off the stats; a client recording whose command struct is padded to 28 bytes
// reads the same; a damaged file is refused with its place; the search path finds a pack's demo
// by its name with or without the extension; the report's 1% low is the slowest frames' rate.
//
// ge:summary Tests: both demo formats read back frame by frame, demos found, the fps report
// ge:keywords test demo timedemo benchmark single-player client-recording one-percent-low
// ge:keywords find_demo bench_stats damaged
// ge:skill shooter testing
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "vfs/pak.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace sh = ge::shooter;

cl::DemoScript script() { return cl::loop_script("halls", {128, 192, 24}, 64, 90); }

// Every frame of `bytes` read; the eyes and angles in order.
struct Read {
  std::vector<sh::Vec3f> eyes, angles;
  std::string map;
  int levels = 0;
  cl::DemoState last;
  cl::DemoRead end = cl::DemoRead::frame;
  std::string error;
};

Read read_all(std::vector<uint8_t> bytes, cl::DemoFormat want) {
  Read r;
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, r.error)) {
    r.end = cl::DemoRead::error;
    return r;
  }
  CHECK(d.state.format == want);
  for (;;) {
    r.end = cl::next_frame(d, r.error);
    if (r.end != cl::DemoRead::frame) break;
    if (d.state.map.empty()) continue;
    r.eyes.push_back(d.state.eye);
    r.angles.push_back(d.state.angles);
  }
  r.map = d.state.map;
  r.levels = d.state.levels;
  r.last = d.state;
  return r;
}

bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

void check_path(const Read& r, const cl::DemoScript& s) {
  CHECK_MSG(r.end == cl::DemoRead::end, r.error);
  CHECK(r.map == "halls");
  CHECK(r.levels == 1);
  CHECK(r.eyes.size() >= s.origins.size());
  // The last frames are the script's (a signon frame may come first).
  const size_t lead = r.eyes.size() - s.origins.size();
  for (size_t i = 0; i < s.origins.size(); i++) {
    const sh::Vec3f e = r.eyes[lead + i], o = s.origins[i];
    CHECK(e.x == o.x && e.y == o.y && e.z == o.z + 22);
    CHECK(near(r.angles[lead + i].y, s.angles[i].y, 1e-4f));
    CHECK(near(r.angles[lead + i].x, s.angles[i].x, 1e-4f));
  }
}

TEST(a_single_player_demo_reads_back_frame_for_frame) {
  const cl::DemoScript s = script();
  const Read r = read_all(cl::write_single_demo(s), cl::DemoFormat::single);
  check_path(r, s);
  CHECK(r.last.protocol == 15);
  CHECK(r.last.models.size() == 4 && r.last.models[1] == "maps/halls.bsp");
  const shooters_edge::hud::Status h = cl::demo_status(r.last, 1.0);
  CHECK(h.health == 100);
  CHECK(h.weapon == shooters_edge::hud::Weapon::rocket_launcher);
  CHECK(h.ammo[2] == 25);
  CHECK(h.armour == 100 && h.armour_class == shooters_edge::hud::ArmourClass::light);
}

TEST(a_client_recording_reads_back_frame_for_frame) {
  const cl::DemoScript s = script();
  const Read r = read_all(cl::write_client_demo(s), cl::DemoFormat::client);
  check_path(r, s);
  CHECK(r.last.protocol == 28);
  CHECK(r.last.entities.size() > 40 && r.last.entities[40].present);
  CHECK(r.last.entities[40].model == 3);  // its baseline's model, kept through the deltas
  CHECK(r.last.entities[40].origin.x == s.origins.back().x + 89);
  CHECK(cl::demo_status(r.last, 1.0).weapon == shooters_edge::hud::Weapon::rocket_launcher);
}

TEST(a_padded_command_struct_is_found_and_read_the_same) {
  const cl::DemoScript s = script();
  check_path(read_all(cl::write_client_demo(s, 28), cl::DemoFormat::client), s);
}

TEST(a_damaged_demo_is_refused_with_its_place) {
  std::vector<uint8_t> bytes = cl::write_single_demo(script());
  bytes[3 + 16 + 1 + 4 + 1] = 0x99;  // the level start's protocol number
  const Read r = read_all(bytes, cl::DemoFormat::single);
  CHECK(r.end == cl::DemoRead::error);
  CHECK(r.error.find("protocol") != std::string::npos);
  CHECK(r.error.find("at byte 3") != std::string::npos);

  std::vector<uint8_t> client = cl::write_client_demo(script());
  client[4] = 7;  // the first record's type
  cl::Demo d;
  std::string error;
  CHECK(!cl::open_demo(client, d, error));
  CHECK(!error.empty());

  // A file cut short ends at its last whole frame.
  std::vector<uint8_t> cut = cl::write_single_demo(script());
  cut.resize(cut.size() / 2);
  CHECK(read_all(cut, cl::DemoFormat::single).end == cl::DemoRead::end);
}

TEST(the_search_path_finds_a_demo_with_or_without_its_extension) {
  cl::SearchPath files;
  std::string error;
  std::vector<uint8_t> pak;
  CHECK(ge::vfs::write_pak({{"demo1.dem", cl::write_single_demo(script())},
                            {"demo2.rec", cl::write_client_demo(script())}},
                           &pak, &error));
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  for (const char* name : {"demo1", "demo1.dem", "demo2"}) {
    cl::Timedemo t;
    CHECK_MSG(cl::timedemo_start(files, name, t, error), error);
    int frames = 0, levels = 0;
    for (;;) {
      const cl::TimedemoStep step = cl::timedemo_step(t, error);
      if (step == cl::TimedemoStep::end || step == cl::TimedemoStep::error) {
        CHECK_MSG(step == cl::TimedemoStep::end, error);
        break;
      }
      if (step == cl::TimedemoStep::new_level) levels++;
      frames++;
      cl::timedemo_mark(t, static_cast<uint64_t>(frames) * 5'000'000);
    }
    CHECK(levels == 1);
    CHECK(frames >= 90);
    // The first frames after a level are not timed; the rest are 5 ms each.
    CHECK(t.frame_s.size() == static_cast<size_t>(frames - cl::kSkipAfterLevel));
    CHECK(near(static_cast<float>(cl::bench_stats(t.frame_s).avg_fps), 200, 0.01f));
  }
  cl::Timedemo none;
  CHECK(!cl::timedemo_start(files, "demo9", none, error));
  CHECK(error.find("demo9") != std::string::npos);

  // A file's path is read from disk and reported as that path.
  const std::string path = (std::filesystem::temp_directory_path() / "se_demo_test.dem").string();
  {
    const std::vector<uint8_t> b = cl::write_single_demo(script());
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  }
  cl::Timedemo disk;
  CHECK_MSG(cl::timedemo_start(files, path, disk, error), error);
  CHECK(disk.found == path);
  std::filesystem::remove(path);
  CHECK(!cl::timedemo_start(files, path, disk, error));
  CHECK(error.find("no demo file at") != std::string::npos);
}

TEST(the_report_s_lows_are_the_slowest_frames_rate) {
  std::vector<double> f(1000, 0.004);                // 250 fps
  for (size_t i = 0; i < 10; i++) f[i * 97] = 0.02;  // 1% at 50 fps
  f[500] = 0.05;                                     // one hitch, 20 fps
  const cl::BenchStats s = cl::bench_stats(f);
  CHECK(s.frames == 1000);
  CHECK(near(static_cast<float>(s.max_fps), 250, 0.01f));
  CHECK(near(static_cast<float>(s.min_fps), 20, 0.01f));
  CHECK(near(static_cast<float>(s.low01_fps), 20, 0.01f));         // the one slowest frame
  CHECK(near(static_cast<float>(s.low1_fps), 10 / 0.23f, 0.01f));  // the hitch and nine 20 ms
  CHECK(near(static_cast<float>(s.p50_ms), 4, 1e-3f));
  CHECK(s.avg_fps < 250 && s.avg_fps > 200);
  const std::vector<std::string> lines = cl::bench_report("demo1", "test", s);
  CHECK(lines.size() == 3);
  CHECK(lines[1].find("average") != std::string::npos);
  CHECK(lines[2].find("1% low") != std::string::npos);
  CHECK(cl::bench_stats({}).frames == 0);
}

}  // namespace
