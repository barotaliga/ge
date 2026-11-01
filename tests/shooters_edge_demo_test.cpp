// Tests: recorded games read back and benchmarked (games/shooters_edge/client/demo.hpp,
// timedemo.hpp). A scripted eye path round the halls written in all three formats
// (demo_write.hpp) reads back frame for frame: the map, the eye over the origin by the view height,
// the angles, the HUD's status off the stats; a client recording whose command struct is padded to
// 28 bytes reads the same; a damaged file is refused with its place; the search path finds a pack's
// demo by its name with or without the extension; a multiview recording (plain or gzip) follows the
// lowest player or the one asked for, with that player's own stats; the sounds each frame starts
// and the level's loops are kept; a voice format (`.qwdv`) opens on its header and a wrong header
// is refused; the report's 1% low is the slowest frames' rate.
//
// ge:summary Tests: every demo format read back frame by frame, demos found, the fps report
// ge:keywords test demo timedemo benchmark single-player client-recording one-percent-low
// ge:keywords find_demo bench_stats damaged multiview gzip track sound-events voice-format
// ge:skill shooter testing
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/inflate.hpp"
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

// The same angle, a 16-bit wire angle's step apart at most, whichever way round it is written.
bool same_angle(float a, float b) {
  const double d = std::fmod(std::fabs(static_cast<double>(a) - b), 360.0);
  return std::min(d, 360 - d) <= 360.0 / 65536;
}

// A gzip file holding `bytes` in stored deflate blocks (as a stock gzip at level 0 writes it).
std::vector<uint8_t> gzip_stored(const std::vector<uint8_t>& bytes) {
  std::vector<uint8_t> z = {0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 3};
  const auto u32 = [&](uint32_t v) {
    for (int i = 0; i < 4; i++) z.push_back(static_cast<uint8_t>(v >> (8 * i)));
  };
  for (size_t at = 0; at < bytes.size() || at == 0; at += 65535) {
    const size_t n = std::min<size_t>(65535, bytes.size() - at);
    z.push_back(at + n >= bytes.size() ? 1 : 0);
    z.push_back(static_cast<uint8_t>(n));
    z.push_back(static_cast<uint8_t>(n >> 8));
    z.push_back(static_cast<uint8_t>(~n));
    z.push_back(static_cast<uint8_t>(~n >> 8));
    z.insert(z.end(), bytes.begin() + static_cast<long>(at),
             bytes.begin() + static_cast<long>(at + n));
    if (n == 0) break;
  }
  u32(ge::core::crc32(bytes.data(), bytes.size()));
  u32(static_cast<uint32_t>(bytes.size()));
  return z;
}

TEST(a_multiview_recording_reads_back_frame_for_frame) {
  const cl::DemoScript s = script();
  const std::vector<uint8_t> bytes = cl::write_multiview_demo(s);
  // Told from the other two by its bytes, and read the same out of a gzip file.
  for (const std::vector<uint8_t>& file : {bytes, gzip_stored(bytes)}) {
    cl::Demo d;
    std::string error;
    CHECK_MSG(cl::open_demo(file, d, error), error);
    CHECK(d.state.format == cl::DemoFormat::multiview);
    std::vector<sh::Vec3f> eyes, angles;
    std::vector<int> health;
    cl::DemoRead end;
    while ((end = cl::next_frame(d, error)) == cl::DemoRead::frame) {
      eyes.push_back(d.state.eye);
      angles.push_back(d.state.angles);
      health.push_back(d.state.stats[cl::kStatHealth]);
    }
    CHECK_MSG(end == cl::DemoRead::end, error);
    CHECK(d.state.map == "halls" && d.state.levels == 1 && d.state.protocol == 28);
    CHECK(d.state.player == 0);  // the lowest slot playing
    // The signon and the first frame are one frame (their records carry no milliseconds).
    CHECK(eyes.size() == s.origins.size());
    for (size_t i = 0; i < s.origins.size() && i < eyes.size(); i++) {
      const sh::Vec3f o = s.origins[i];
      CHECK(eyes[i].x == o.x && eyes[i].y == o.y && eyes[i].z == o.z + 22);
      CHECK(same_angle(angles[i].x, s.angles[i].x) && same_angle(angles[i].y, s.angles[i].y));
      CHECK(health[i] == s.health);  // the second player's stats never reach the view
    }
    // The clock is the milliseconds' sum: 14 a frame after the first, and the end's.
    CHECK(near(static_cast<float>(d.state.time), 0.014f * static_cast<float>(s.origins.size()),
               1e-4f));
    CHECK(d.hidden == static_cast<int64_t>(s.origins.size()));
    CHECK(d.state.players[3].present);
    CHECK(d.state.players[3].origin.x == -64 && same_angle(d.state.players[3].angles.y, 90));
    CHECK(d.state.entities.size() > 40 && d.state.entities[40].model == 3);
    CHECK(d.state.entities[40].origin.x == s.origins.back().x + 89);
  }
}

TEST(a_multiview_recording_follows_the_player_asked_for) {
  const cl::DemoScript s = script();
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(cl::write_multiview_demo(s), d, error), error);
  d.track = 3;
  int frames = 0;
  while (cl::next_frame(d, error) == cl::DemoRead::frame) {
    frames++;
    CHECK(d.state.player == 3);
    CHECK(d.state.eye.x == -64 && d.state.eye.y == 32 && d.state.eye.z == 24 + 22);
  }
  CHECK(frames == static_cast<int>(s.origins.size()));
  // His own stats, then the hurt his stats record carried halfway.
  CHECK(d.state.stats[cl::kStatHealth] == 12);
  CHECK(d.state.stats[cl::kStatRockets] == 3);
}

TEST(a_damaged_multiview_recording_is_refused_with_its_place) {
  std::vector<uint8_t> bytes = cl::write_multiview_demo(script());
  // The second frame's record kind turned into a client's command.
  size_t at = 0;
  for (int k = 0; k < 6; k++) {  // signon, lists, two stats, the first frame, its hidden record
    const int kind = bytes[at + 1] & 7;
    at += 2 + (kind == 3 ? 4 : 0);
    at += 4 + (bytes[at] | bytes[at + 1] << 8 | bytes[at + 2] << 16);
  }
  bytes[at + 1] = 0;
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(bytes, d, error), error);
  cl::DemoRead r;
  while ((r = cl::next_frame(d, error)) == cl::DemoRead::frame) {
  }
  CHECK(r == cl::DemoRead::error);
  CHECK_MSG(error.find("at byte " + std::to_string(at)) != std::string::npos, error);
  // A gzip file that does not unpack is refused before any reading.
  std::vector<uint8_t> gz = gzip_stored(cl::write_multiview_demo(script()));
  gz[gz.size() - 8] ^= 1;
  CHECK(!cl::open_demo(gz, d, error));
  CHECK(error.find("gzip") != std::string::npos);
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
                            {"demo2.rec", cl::write_client_demo(script())},
                            {"demo3.rec", cl::write_multiview_demo(script())}},
                           &pak, &error));
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  for (const char* name : {"demo1", "demo1.dem", "demo2", "demo3"}) {
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

TEST(playback_keeps_the_demo_s_own_pace) {
  cl::SearchPath files;
  std::string error;
  std::vector<uint8_t> pak;
  CHECK(ge::vfs::write_pak({{"match.rec", cl::write_multiview_demo(script())}}, &pak, &error));
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  cl::Timedemo t;
  CHECK_MSG(cl::timedemo_start(files, "match", t, error), error);
  CHECK(cl::playdemo_step(t, 0, 1, error) == cl::TimedemoStep::new_level);
  const int64_t first = t.demo.state.frames;
  // Paused: the clock does not move, so no frame is read.
  CHECK(cl::playdemo_step(t, 1.0, 0, error) == cl::TimedemoStep::frame);
  CHECK(t.demo.state.frames == first);
  // At speed 1, 28 ms of wall time a step reads the recording's 14 ms frames two at a time; at
  // speed 2, four.
  CHECK(cl::playdemo_step(t, 0.028, 1, error) == cl::TimedemoStep::frame);
  CHECK(t.demo.state.frames == first + 2);
  CHECK(cl::playdemo_step(t, 0.028, 2, error) == cl::TimedemoStep::frame);
  CHECK(t.demo.state.frames == first + 6);
  int steps = 0;
  while (cl::playdemo_step(t, 0.028, 1, error) == cl::TimedemoStep::frame) steps++;
  CHECK_MSG(error.empty(), error);
  CHECK(steps >= 40 && steps <= 43);  // the other 83 frames and the end, two a step
  CHECK(t.frame_s.empty());           // playback times nothing
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

TEST(the_gpu_line_gives_the_median_span_or_says_it_was_not_measured) {
  cl::PassTimes p;
  CHECK(cl::span_report(p).find("not measured") != std::string::npos);
  for (int i = 0; i < 5; i++) cl::span_add(p, 6 + i);  // median 8
  cl::span_add(p, NAN);                                // not measured: left out
  CHECK(p.span_ms.size() == 5);
  CHECK(cl::span_report(p).find("median 8.00 ms") != std::string::npos);
}

TEST(the_price_reads_each_saving_against_the_base_runs_either_side) {
  // The machine slows by 1 ms a run: the bracketing base runs take the drift out.
  const cl::PriceRun bases[] = {{"", 10, 8}, {"", 11, 9}, {"", 12, 10}, {"", 13, 11}};
  const cl::PriceRun off[] = {
      {"ce_water", 9.5, 7.5}, {"ce_shadows", 7.5, 5.5}, {"the HUD", 12.6, 10.5}};
  const std::vector<cl::Price> p = cl::prices(bases, off);
  CHECK(p.size() == 3);
  CHECK(p[0].name == "ce_shadows" && p[0].frame_ms == 4 && p[0].span_ms == 4);
  CHECK(p[1].name == "ce_water" && p[1].frame_ms == 1);
  CHECK(p[2].name == "the HUD" &&
        near(float(p[2].frame_ms), -0.1f, 1e-5f));  // under the noise: can be negative
  CHECK(cl::price_noise(bases) == 1);
  const std::vector<std::string> lines = cl::price_report(bases, p);
  CHECK(lines.size() == 4);
  CHECK(lines[0].find("10.00 to 13.00 ms over 4 runs") != std::string::npos);
  CHECK(lines[0].find("under 1.00 ms is noise") != std::string::npos);
  CHECK(lines[1].find("ce_shadows off saves 4.00 ms") != std::string::npos);
}

}  // namespace

TEST(a_demo_keeps_the_sounds_each_frame_starts_and_the_level_s_loops) {
  const cl::DemoScript s = script();
  cl::Demo d;
  std::string error;
  CHECK(cl::open_demo(cl::write_single_demo(s), d, error));
  int started = 0;
  cl::DemoSound first;
  while (cl::next_frame(d, error) == cl::DemoRead::frame) {
    for (const cl::DemoSound& e : d.state.started) {
      if (started++ == 0) first = e;
    }
  }
  CHECK_MSG(error.empty(), error);
  CHECK(started == 9);  // the rocket's sound every tenth frame of 90
  CHECK(first.entity == 2 && first.channel == 1 && first.sound == 1);
  CHECK(near(first.volume, 200.0f / 255.0f, 1e-6f) && first.attenuation == 1.0f);
  CHECK(d.state.static_sounds.size() == 1);
  CHECK(d.state.static_sounds[0].sound == 2 && d.state.static_sounds[0].volume == 1.0f);
  CHECK(d.state.static_sounds[0].origin.x == 16);
}

TEST(a_voice_format_opens_on_its_header_and_a_wrong_header_is_refused) {
  const cl::DemoScript s = script();
  std::vector<uint8_t> bytes = cl::voice_header(cl::DemoFormat::client);
  const std::vector<uint8_t> plain = cl::write_client_demo(s);
  bytes.insert(bytes.end(), plain.begin(), plain.end());
  const Read r = read_all(bytes, cl::DemoFormat::client);
  check_path(r, s);
  CHECK(r.last.voice_format);
  // The header names a multiview recording, the records are a client's: refused.
  std::vector<uint8_t> wrong = bytes;
  wrong[5] = 2;
  cl::Demo d;
  std::string error;
  CHECK(!cl::open_demo(wrong, d, error));
  CHECK(error.find("multiview") != std::string::npos);
  wrong = bytes;
  wrong[4] = 9;  // a version this build does not read
  CHECK(!cl::open_demo(wrong, d, error));
  CHECK(error.find("version 9") != std::string::npos);
  CHECK(cl::voice_name("match.mvdv") && cl::voice_name("RED1.QWDV"));
  CHECK(!cl::voice_name("match.mvd") && !cl::voice_name("v"));
  CHECK(cl::voice_extension(cl::DemoFormat::multiview) == ".mvdv");
}

TEST(a_recording_keeps_its_server_s_game_folder_and_each_player_s_colours) {
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(cl::write_multiview_match(cl::highlight_match()), d, error), error);
  CHECK_MSG(cl::next_frame(d, error) == cl::DemoRead::frame, error);
  CHECK(d.state.game == "baseline");
  for (size_t k = 0; k < 4; k++) {  // the writer's colours: slot k wears k + 2 over k + 3
    CHECK(d.state.top[k] == static_cast<int>(k) + 2);
    CHECK(d.state.bottom[k] == static_cast<int>(k) + 3);
  }
  CHECK(d.state.players[1].present && d.state.players[1].model > 0);  // drawn, not the eye
}

TEST(a_demo_plays_from_the_game_folder_its_server_ran_unless_another_was_asked) {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "se_demo_game_dir_test";
  fs::remove_all(root);
  fs::create_directories(root / "id1");
  fs::create_directories(root / "fortress");
  const std::string base = root.string();
  std::string note;
  // The mod's folder is there: the demo plays from it.
  CHECK(cl::demo_game_dir(base, "id1", "fortress", note) == "fortress" && note.empty());
  // A -game asked wins; the base and an unnamed folder change nothing.
  CHECK(cl::demo_game_dir(base, "arena", "fortress", note) == "arena" && note.empty());
  CHECK(cl::demo_game_dir(base, "id1", "id1", note) == "id1" && note.empty());
  CHECK(cl::demo_game_dir(base, "id1", "", note) == "id1" && note.empty());
  // A folder that is not there: the window's, with a line saying what to do.
  CHECK(cl::demo_game_dir(base, "id1", "ctf", note) == "id1");
  CHECK(note.find("ctf") != std::string::npos && note.find("next:") != std::string::npos);
  // A recording naming a path is never followed out of the base folder.
  CHECK(cl::demo_game_dir(base, "id1", "../fortress", note) == "id1");
  CHECK(cl::demo_game_dir(base, "id1", "..", note) == "id1");
  // The window's side: the mod's folder mounted once, said once, and let go for the base again.
  cl::SearchPath window;
  std::string error;
  CHECK_MSG(cl::open_game_dir(base, "id1", window, error), error);
  cl::DemoFolder f;
  cl::DemoState s;
  s.game = "fortress";
  std::vector<std::string> say;
  const cl::SearchPath* from = &cl::demo_folder(f, base, "id1", s, window, say);
  CHECK(from != &window && f.game == "fortress" && say.size() == 1);
  CHECK(from->folder == (root / "fortress").string());
  CHECK(&cl::demo_folder(f, base, "id1", s, window, say) == from && say.size() == 1);
  s.game = "ctf";  // not there: the window's, the note said once over two levels
  CHECK(&cl::demo_folder(f, base, "id1", s, window, say) == &window && !f.path);
  CHECK(&cl::demo_folder(f, base, "id1", s, window, say) == &window && say.size() == 2);
  fs::remove_all(root);
}
