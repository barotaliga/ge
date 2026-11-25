// The frame profile (engine/prof/frames.hpp): the first tick only opens a frame, each later tick
// folds the frame's zones into their own time in that frame (a nested zone's time is not counted
// in its parent's), the record stops itself once the frames asked for are in, and the report
// names every zone, the time in no zone and the slowest frame with its dearest zone.
//
// ge:summary Tests: the frame profile's ticks, its per-frame fold and its report
// ge:keywords test prof frame-profile frames_tick frames_report slowest-frames zones
// ge:skill perf testing
// ge:test tests/prof_frames_test.cpp
#include <string>

#include "check.hpp"
#include "prof/frames.hpp"
#include "prof/zones.hpp"

namespace prof = ge::prof;

namespace {

void work(int n) {
  volatile int x = 0;
  for (int i = 0; i < n; i++) x = x + i;
}

void outer(int n) {
  GE_ZONE("test.frame.outer");
  {
    GE_ZONE("test.frame.inner");
    work(n);
  }
}

}  // namespace

TEST(the_first_tick_opens_a_frame_and_records_none) {
  prof::FrameProfile fp;
  prof::frames_start(fp, 3);
  CHECK(prof::frames_running(fp));
  outer(10);
  CHECK(!prof::frames_tick_with(fp, 5'000'000));
  CHECK(fp.frame_ns.size() == size_t{0});
  prof::frames_stop(fp);
}

TEST(each_tick_folds_its_frame_and_the_record_stops_when_whole) {
  prof::FrameProfile fp;
  prof::frames_start(fp, 2);
  prof::frames_tick_with(fp, 0);  // opens frame 1
  outer(1000);
  CHECK(!prof::frames_tick_with(fp, 4'000'000));
  outer(1000);
  outer(1000);
  CHECK(prof::frames_tick_with(fp, 90'000'000));
  CHECK(!prof::frames_running(fp));
  CHECK(fp.done);
  CHECK(fp.frame_ns.size() == size_t{2});
  CHECK(fp.zones.size() == size_t{2});
  // Both zones were met in both frames; the outer's own time leaves out the inner's.
  for (const auto& z : fp.ns) CHECK(z.size() == size_t{2});
  // Nothing is recorded once the record is whole.
  outer(10);
  CHECK(!prof::frames_tick_with(fp, 1));
  CHECK(fp.frame_ns.size() == size_t{2});
}

TEST(the_report_names_the_zones_the_loose_time_and_the_slowest_frame) {
  prof::FrameProfile fp;
  prof::frames_start(fp, 2);
  prof::frames_tick_with(fp, 0);
  outer(100);
  prof::frames_tick_with(fp, 2'000'000);
  outer(100);
  prof::frames_tick_with(fp, 80'000'000);
  const std::string r = prof::frames_report(fp, 1);
  CHECK(r.find("profile: 2 frames") != std::string::npos);
  CHECK(r.find("test.frame.inner") != std::string::npos);
  CHECK(r.find("test.frame.outer") != std::string::npos);
  CHECK(r.find("(no zone)") != std::string::npos);
  CHECK(r.find("frame 2: 80.00 ms") != std::string::npos);
  CHECK(r.find("frame 1:") == std::string::npos);  // one slowest frame asked for
}

TEST(a_report_of_no_frames_says_so) {
  prof::FrameProfile fp;
  CHECK(prof::frames_report(fp).find("no frames") != std::string::npos);
}
