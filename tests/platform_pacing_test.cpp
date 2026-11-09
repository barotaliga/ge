// The window's pacing (engine/platform/pacing.hpp): the cvars bound, the precise wait,
// framestats. No window is opened.
//
// ge:summary Tests the window's pacing: the cvars, the precise wait, framestats' line
// ge:keywords test pacing cvars vid_vsync cl_maxfps wait_until_ns framestats
// ge:skill platform testing
#include "platform/pacing.hpp"

#include "check.hpp"
#include "console/console.hpp"

TEST(the_pacing_cvars_write_the_settings) {
  ge::console::Console con;
  ge::platform::Pacing p;
  ge::platform::add_pacing_cvars(con, p);
  con.execute("vid_vsync 0; cl_maxfps 250; cl_maxfps_hz 2; vid_framelatency 9; scr_showfps 2");
  CHECK(!p.settings.vsync);
  CHECK(p.settings.max_fps == 250);
  CHECK(p.settings.hz_mult == 2);
  CHECK(p.settings.latency == ge::loop::kMaxLatency);  // clamped
  CHECK(ge::platform::swap_images(p.settings) == 3);
  CHECK(p.show == 2);
  p.times.add(0.008);
  p.refresh = 120;
  p.cap = 240;
  const std::string line = ge::platform::pacing_text(p);
  CHECK(line.find("display 120.00 Hz") != std::string::npos);
  CHECK(line.find("cap 240.00 fps") != std::string::npos);
  CHECK(line.find("vsync off") != std::string::npos);
}

TEST(the_wait_lands_on_its_deadline) {
  for (int i = 0; i < 5; i++) {
    const uint64_t at = ge::platform::now_ns() + 2'000'000;  // 2 ms on
    ge::platform::wait_until_ns(at);
    const uint64_t now = ge::platform::now_ns();
    CHECK(now >= at);
    // Never early. How late is the OS's: a virtual Mac runner oversleeps every wait by more than
    // 1.5 ms, so this bounds only a gross fault (a wait of the wrong unit, a missed deadline).
    CHECK(now - at < 20'000'000);
  }
  const uint64_t before = ge::platform::now_ns();
  ge::platform::wait_until_ns(before - 1);  // past: at once
  CHECK(ge::platform::now_ns() - before < 20'000'000);
}
