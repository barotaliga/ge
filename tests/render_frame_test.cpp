// engine/render/frame.hpp: the pass table's check names a read before its write and a pass
// listed twice and passes a good order; the frame block is filled from a FrameView (the sun's
// direction, the eye's altitude, the size); the stats sum their passes.
//
// ge:summary Tests for the frame's shared half: the pass table's check, the uniforms, the stats
// ge:keywords test render pass-table check_pass_table uniforms fill_uniforms frame-stats
// ge:skill render testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "render/frame.hpp"

namespace {

using ge::render::PassRow;

constexpr PassRow kGood[] = {
    {"tables", "", "sky.view"},
    {"terrain", "height sky.view", "hdr depth"},
    {"sky", "sky.view depth", "hdr"},
    {"post", "hdr", "final"},
};

constexpr PassRow kReadEarly[] = {
    {"post", "hdr", "final"},
    {"terrain", "height", "hdr depth"},
};

constexpr PassRow kTwice[] = {
    {"terrain", "", "hdr"},
    {"terrain", "", "hdr"},
};

}  // namespace

TEST(pass_table_in_order_has_no_faults) {
  CHECK(ge::render::check_pass_table(kGood, "height").empty());
}

TEST(pass_table_read_before_write_is_named) {
  const std::string f = ge::render::check_pass_table(kReadEarly, "height");
  CHECK(f.find("pass 'post' reads 'hdr'") != std::string::npos);
  CHECK(f.find("terrain") == std::string::npos);
}

TEST(pass_table_input_missing_is_named) {
  const std::string f = ge::render::check_pass_table(kGood, "");
  CHECK(f.find("reads 'height'") != std::string::npos);
}

TEST(pass_table_duplicate_is_named) {
  CHECK(ge::render::check_pass_table(kTwice, "").find("listed twice") != std::string::npos);
}

TEST(uniforms_carry_the_view) {
  ge::render::FrameView v;
  v.eye[0] = 100;
  v.eye[1] = 250;
  v.eye[2] = -30;
  v.altitude_m = 1200;
  v.sun_elevation = 0.5;
  v.sun_azimuth = 3.14159265358979;  // due south: -z
  v.time_s = 2.5;
  v.frame = 7;
  ge::render::FrameUniforms u;
  ge::render::fill_uniforms(u, v, 640, 360);
  CHECK(u.eye[0] == 100 && u.eye[1] == 250 && u.eye[2] == -30);
  CHECK(std::fabs(u.eye[3] - 1.45f) < 1e-5f);  // (1200 + 250) m over the sea, km
  CHECK(std::fabs(u.world[0] - 1.2f) < 1e-6f);
  CHECK(std::fabs(u.sun[1] - std::sin(0.5f)) < 1e-6f);
  CHECK(u.sun[2] < -0.8f && std::fabs(u.sun[0]) < 1e-6f);
  CHECK(u.size[0] == 640 && u.size[1] == 360 && std::fabs(u.size[2] - 1.0f / 640) < 1e-9f);
  CHECK(u.time[0] == 2.5f && u.time[1] == 7.0f);
  for (float p : u.planes[5]) CHECK(p == 0);
}

TEST(stats_sum_their_passes) {
  ge::render::FrameStats s;
  s.count = 2;
  s.passes[0] = {"a", 0, 2, 1};
  s.passes[1] = {"b", 0, 3, 4};
  s.passes[2] = {"c", 0, 100, 100};  // past count: not this frame's
  CHECK(s.draws() == 5);
  CHECK(s.dispatches() == 5);
}
