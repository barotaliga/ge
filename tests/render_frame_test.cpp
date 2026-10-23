// engine/render/frame.hpp: the pass table's check names a read before its write and a pass
// listed twice and passes a good order; the frame block is filled from a FrameView (the sun's
// direction, the eye's altitude, the size, the weather); the stats sum their passes.
//
// ge:summary Tests for the frame's shared half: the pass table's check, the uniforms, the stats
// ge:keywords test render pass-table check_pass_table uniforms fill_uniforms frame-stats
// ge:keywords weather fog deck
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

TEST(uniforms_carry_the_weather) {
  ge::render::FrameView v;
  ge::render::FrameUniforms u;
  // The default is a clear sky: the sun whole, no deck, no fog.
  ge::render::fill_uniforms(u, v, 64, 36);
  CHECK(u.weather[0] == 1.0f && u.weather[1] == 0.0f && u.weather[2] == 0.0f);
  CHECK(u.weather[3] == 0.0f);
  v.weather.sun_pass = 0.25;
  v.weather.cover = 2;  // out of range: held to 1
  v.weather.visibility_m = 391.2;
  v.weather.hidden = 0.5;
  v.weather.deck_light = 2;
  v.weather.deck_zenith = 0.6;
  ge::render::fill_uniforms(u, v, 64, 36);
  CHECK(u.weather[0] == 0.25f && u.weather[1] == 1.0f && u.weather[3] == 0.5f);
  // A visual range V is an extinction of 3.912 / V.
  CHECK(std::fabs(u.weather[2] - 0.01f) < 1e-7f);
  CHECK(std::fabs(u.deck[0] - 2 * 0.86f) < 1e-6f && std::fabs(u.deck[3] - 0.6f) < 1e-7f);
}

// The pixel a camera-relative point lands on under a block's (column-major) view_proj.
static void pixel_of(const float* m, const double rel[3], double w, double h, double out[2]) {
  double c[4];
  for (int r = 0; r < 4; ++r) {
    c[r] = m[r] * rel[0] + m[4 + r] * rel[1] + m[8 + r] * rel[2] + m[12 + r];
  }
  out[0] = (c[0] / c[3] * 0.5 + 0.5) * w;
  out[1] = (0.5 - c[1] / c[3] * 0.5) * h;
}

TEST(jitter_moves_every_point_by_the_offset) {
  ge::render::FrameView v;
  v.eye[1] = 10;
  v.heading = 0.4;
  v.pitch = -0.1;
  ge::render::FrameUniforms plain, jittered;
  ge::render::fill_uniforms(plain, v, 640, 360);
  const double j[2] = {0.25, -0.375};
  ge::render::fill_uniforms(jittered, v, 640, 360, nullptr, j);
  CHECK(jittered.jitter[0] == 0.25f && jittered.jitter[1] == -0.375f);
  CHECK(std::fabs(jittered.jitter[2] - 0.25f / 640) < 1e-9f);
  const double points[2][3] = {{3, -2, 40}, {-30, 5, 400}};
  for (const auto& rel : points) {
    double a[2], b[2];
    pixel_of(plain.view_proj, rel, 640, 360, a);
    pixel_of(jittered.view_proj, rel, 640, 360, b);
    CHECK(std::fabs(b[0] - a[0] - 0.25) < 1e-3 && std::fabs(b[1] - a[1] + 0.375) < 1e-3);
  }
  // Unjittered, the previous camera is this one: no motion.
  for (int i = 0; i < 16; ++i) CHECK(plain.prev_view_proj[i] == plain.view_proj[i]);
}

TEST(history_carries_the_last_camera) {
  ge::render::FrameView v;
  v.eye[0] = 5;
  ge::render::FrameHistory h;
  ge::render::FrameUniforms first, second;
  const double j[2] = {0.1, 0.2};
  ge::render::fill_uniforms(first, v, 320, 200, &h, j);
  CHECK(h.valid);
  CHECK(first.eye_delta[0] == 0 && first.eye_delta[1] == 0 && first.eye_delta[2] == 0);
  ge::render::FrameUniforms unjittered;
  ge::render::fill_uniforms(unjittered, v, 320, 200);
  for (int i = 0; i < 16; ++i) CHECK(first.prev_view_proj[i] == unjittered.view_proj[i]);
  v.eye[0] = 7.5;
  v.heading = 0.2;
  ge::render::fill_uniforms(second, v, 320, 200, &h, j);
  CHECK(second.eye_delta[0] == 2.5f && second.eye_delta[2] == 0);
  // The previous camera is the first frame's, unjittered.
  for (int i = 0; i < 16; ++i) CHECK(second.prev_view_proj[i] == unjittered.view_proj[i]);
}

TEST(stats_sum_their_passes) {
  ge::render::FrameStats s;
  s.count = 2;
  s.passes[0] = {.name = "a", .draws = 2, .dispatches = 1};
  s.passes[1] = {.name = "b", .draws = 3, .dispatches = 4};
  s.passes[2] = {.name = "c", .draws = 100, .dispatches = 100};  // past count: not this frame's
  CHECK(s.draws() == 5);
  CHECK(s.dispatches() == 5);
}
