// Tests: demo formats converted as they are read (games/shooters_edge/client/demo_convert.hpp). A
// single-player demo written in code (demo_write.hpp) converts to a multiview recording that
// reads back frame for frame: the same map, the eye on the same path, the same view angles, the
// same status, the rocket renumbered past the player slots where the source had it; fed a few
// bytes at a time, the stream gives the same bytes as the whole file, every take whole records;
// the loop's form leaves the end out; what the target cannot hold is counted; a pair that does
// not exist and a damaged demo are refused with the reason; a file's format is told by its name.
//
// ge:summary Tests: a .dem converted to .mvd as a stream, read back frame for frame
// ge:keywords test demo-convert dem mvd stream whole-records no-end dropped format_of_name
// ge:skill shooter testing
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_convert.hpp"
#include "shooters_edge/client/demo_pack.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/sequel_demo.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace sh = ge::shooter;

cl::DemoScript script() { return cl::loop_script("halls", {128, 192, 24}, 64, 90); }

bool same_angle(float a, float b, float step) {
  const double d = std::fmod(std::fabs(static_cast<double>(a) - b), 360.0);
  return std::min(d, 360 - d) <= step;
}

struct Frames {
  std::vector<sh::Vec3f> eyes, angles;
  std::vector<int> health, rockets;
  std::vector<bool> rocket_seen;
  cl::DemoState last;
  cl::DemoRead end = cl::DemoRead::frame;
  std::string error;
};

// Every frame with a map, read; `rocket` the entity number the rocket goes by.
Frames read_frames(std::vector<uint8_t> bytes, cl::DemoFormat want, int rocket) {
  Frames f;
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, f.error)) {
    f.end = cl::DemoRead::error;
    return f;
  }
  CHECK(d.state.format == want);
  while ((f.end = cl::next_frame(d, f.error)) == cl::DemoRead::frame) {
    if (d.state.map.empty()) continue;
    f.eyes.push_back(d.state.eye);
    f.angles.push_back(d.state.angles);
    f.health.push_back(d.state.stats[cl::kStatHealth]);
    f.rockets.push_back(d.state.stats[cl::kStatRockets]);
    const auto& e = d.state.entities;
    f.rocket_seen.push_back(static_cast<size_t>(rocket) < e.size() &&
                            e[static_cast<size_t>(rocket)].present);
  }
  f.last = d.state;
  return f;
}

std::vector<uint8_t> converted(const std::vector<uint8_t>& dem, cl::ConvertNotes& notes,
                               const cl::ConvertOptions& opt = {}) {
  std::vector<uint8_t> out;
  std::string error;
  CHECK_MSG(cl::convert_demo(dem, cl::DemoFormat::multiview, opt, out, notes, error), error);
  return out;
}

// The records of a multiview recording, walked: false when one is cut.
bool whole_records(const std::vector<uint8_t>& b, int& count, int& last_kind_id) {
  size_t at = 0;
  count = 0;
  while (at < b.size()) {
    if (at + 2 > b.size()) return false;
    const int kind = b[at + 1] & 7;
    at += 2;
    if (kind == 3) at += 4;
    if (at + 4 > b.size()) return false;
    uint32_t len = 0;
    std::memcpy(&len, &b[at], 4);
    at += 4;
    if (at + len > b.size()) return false;
    last_kind_id = len > 0 ? b[at] : -1;
    at += len;
    count++;
  }
  return true;
}

TEST(a_single_player_demo_converts_to_a_multiview_recording_frame_for_frame) {
  const cl::DemoScript s = script();
  const std::vector<uint8_t> dem = cl::write_single_demo(s);
  cl::ConvertNotes notes;
  const std::vector<uint8_t> mvd = converted(dem, notes);

  const Frames a = read_frames(dem, cl::DemoFormat::single, 2);
  const Frames b = read_frames(mvd, cl::DemoFormat::multiview, 33);  // 2 - 1 + 32
  CHECK_MSG(a.end == cl::DemoRead::end, a.error);
  CHECK_MSG(b.end == cl::DemoRead::end, b.error);
  CHECK(b.last.map == "halls" && b.last.levels == 1 && b.last.protocol == 28);
  CHECK(b.last.models == a.last.models && b.last.sounds == a.last.sounds);
  CHECK(b.last.static_sounds.size() == a.last.static_sounds.size());
  // The source reads its signon as a frame of its own; the target's signon rides its first frame.
  CHECK(a.eyes.size() >= s.origins.size() && b.eyes.size() == s.origins.size());
  const size_t lead = a.eyes.size() - s.origins.size();
  for (size_t i = 0; i < s.origins.size() && i < b.eyes.size(); i++) {
    const sh::Vec3f o = s.origins[i];
    CHECK(b.eyes[i].x == o.x && b.eyes[i].y == o.y && b.eyes[i].z == o.z + 22);
    CHECK(same_angle(b.angles[i].y, a.angles[lead + i].y, 360.0 / 65536));
    CHECK(same_angle(b.angles[i].x, a.angles[lead + i].x, 360.0 / 65536));
    CHECK(b.health[i] == a.health[lead + i] && b.rockets[i] == a.rockets[lead + i]);
    CHECK(b.rocket_seen[i] == a.rocket_seen[lead + i]);
  }
  CHECK(notes.frames == static_cast<int64_t>(s.origins.size()));
  CHECK(notes.levels == 1);
  CHECK(notes.dropped.contains("the sign-on stage numbers"));
  CHECK(notes.dropped.contains("the protocol's version message"));
}

TEST(the_stream_gives_the_same_bytes_in_whole_records_however_the_source_arrives) {
  const std::vector<uint8_t> dem = cl::write_single_demo(script());
  cl::ConvertNotes notes;
  const std::vector<uint8_t> whole = converted(dem, notes);
  for (size_t piece : {size_t{1}, size_t{7}, size_t{300}}) {
    cl::DemoConvert c;
    std::string error;
    CHECK_MSG(cl::convert_begin(c, cl::DemoFormat::single, cl::DemoFormat::multiview, {}, error),
              error);
    std::vector<uint8_t> all;
    int takes_with_bytes = 0;
    for (size_t at = 0; at < dem.size(); at += piece) {
      cl::convert_feed(c, std::span(dem).subspan(at, std::min(piece, dem.size() - at)));
      std::vector<uint8_t> took;
      CHECK_MSG(cl::convert_take(c, took, error), error);
      int count = 0, last = 0;
      CHECK(whole_records(took, count, last));  // never half a record
      if (!took.empty()) takes_with_bytes++;
      all.insert(all.end(), took.begin(), took.end());
    }
    cl::convert_end(c);
    CHECK_MSG(cl::convert_take(c, all, error), error);
    CHECK(all == whole);
    CHECK(takes_with_bytes > 10);  // frame by frame, not all at the end
  }
}

TEST(the_loops_form_leaves_the_end_out) {
  const std::vector<uint8_t> dem = cl::write_single_demo(script());
  cl::ConvertNotes notes;
  int count = 0, last = 0;
  CHECK(whole_records(converted(dem, notes), count, last));
  CHECK(last == 2);  // a disconnect ends it
  cl::ConvertOptions loop;
  loop.no_end = true;
  CHECK(whole_records(converted(dem, notes, loop), count, last));
  CHECK(last != 2);
}

TEST(a_pair_that_does_not_exist_and_a_damaged_demo_are_refused) {
  cl::DemoConvert c;
  std::string error;
  CHECK(!cl::convert_begin(c, cl::DemoFormat::sequel, cl::DemoFormat::single, {}, error));
  CHECK(error.find("does not convert") != std::string::npos);
  std::vector<uint8_t> dem = cl::write_single_demo(script());
  // The first frame block's first message id made one the protocol does not have.
  const size_t header = 3;
  uint32_t len = 0;
  std::memcpy(&len, &dem[header], 4);
  dem[header + 16 + len + 16] = 99;
  CHECK(cl::convert_begin(c, cl::DemoFormat::single, cl::DemoFormat::multiview, {}, error));
  cl::convert_feed(c, dem);
  cl::convert_end(c);
  std::vector<uint8_t> out;
  CHECK(!cl::convert_take(c, out, error));
  CHECK_MSG(error.find("message id 99") != std::string::npos, error);
  CHECK(!cl::convert_take(c, out, error));  // it stays stopped
}

TEST(a_sequel_recording_of_protocol_26_becomes_protocol_34_byte_for_byte) {
  const std::vector<uint8_t> dm2 = cl::sequel_fixture(4, 7);
  const std::vector<uint8_t> old = cl::sequel_as_protocol_26(dm2);
  CHECK(old.size() < dm2.size());
  for (const std::vector<uint8_t>* from : {&old, &dm2}) {
    std::vector<uint8_t> out;
    cl::ConvertNotes notes;
    std::string error;
    CHECK_MSG(cl::convert_demo(*from, cl::DemoFormat::sequel, {}, out, notes, error), error);
    CHECK(out == dm2);  // 34 comes back as it was; 26 becomes it
    CHECK(notes.frames == 40);
    CHECK(notes.rewritten.contains("a frame given its suppress count") == (from == &old));
  }
  // As a stream a block at a time, the loop's form without the end.
  cl::DemoConvert c;
  std::string error;
  cl::ConvertOptions loop;
  loop.no_end = true;
  CHECK(cl::convert_begin(c, cl::DemoFormat::sequel, cl::DemoFormat::sequel, loop, error));
  std::vector<uint8_t> all;
  for (size_t at = 0; at < old.size(); at += 5) {
    cl::convert_feed(c, std::span(old).subspan(at, std::min<size_t>(5, old.size() - at)));
    CHECK_MSG(cl::convert_take(c, all, error), error);
  }
  cl::convert_end(c);
  CHECK(cl::convert_take(c, all, error));
  CHECK(all.size() == dm2.size() - 4);
  CHECK(std::equal(all.begin(), all.end(), dm2.begin()));
}

// A converter's banner (a print) before the server data still opens as a multiview recording.
TEST(a_multiview_recording_opening_with_a_print_reads) {
  cl::ConvertNotes notes;
  std::vector<uint8_t> mvd = converted(cl::write_single_demo(script()), notes);
  const std::vector<uint8_t> banner = {8, 3, 'c', 'o', 'n', 'v', 'e', 'r', 't', 'e', 'd', '\n', 0};
  uint32_t len = 0;
  std::memcpy(&len, &mvd[2], 4);
  len += static_cast<uint32_t>(banner.size());
  std::memcpy(&mvd[2], &len, 4);
  mvd.insert(mvd.begin() + 6, banner.begin(), banner.end());
  const Frames f = read_frames(mvd, cl::DemoFormat::multiview, 33);
  CHECK_MSG(f.end == cl::DemoRead::end, f.error);
  CHECK(f.last.map == "halls" && f.eyes.size() == script().origins.size());
}

TEST(a_files_format_is_told_by_its_name) {
  cl::DemoFormat f{};
  CHECK(cl::format_of_name("demos/a.dem", f) && f == cl::DemoFormat::single);
  CHECK(cl::format_of_name("A" + cl::plain_extension(cl::DemoFormat::client), f) &&
        f == cl::DemoFormat::client);
  CHECK(cl::format_of_name("m.mvdz", f) && f == cl::DemoFormat::multiview);
  CHECK(cl::format_of_name("m.mvd.gz", f) && f == cl::DemoFormat::multiview);
  CHECK(cl::format_of_name("x.dm2", f) && f == cl::DemoFormat::sequel);
  CHECK(!cl::format_of_name("x.txt", f));
}

}  // namespace
