// A BRUSH MODEL TURNED BY ITS ENTITY'S ANGLES: the frame a trace takes its line into and turns the
// normal back out of, the reach that holds the model at any angle, and the rows it is drawn by,
// which put a vertex where the trace finds it.
//
// ge:summary Tests: a turned brush model's frame, its exact inverse, its reach and its drawn rows
// ge:keywords test turned-brush to_model to_world turned_reach turned_rows
// ge:skill shooter testing
#include "brush/map.hpp"
#include "check.hpp"
#include "shooter/turned.hpp"

namespace sh = ge::shooter;

namespace {

bool near(float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; }
bool near(double a, double b) { return a - b < 1e-6 && b - a < 1e-6; }

}  // namespace

TEST(nothing_turns_at_zero) {
  CHECK(!sh::turned({0, 0, 0}));
  CHECK(sh::turned({0, 0.5f, 0}));
  CHECK(sh::turned({-0.0001f, 0, 0}));
  double rows[9];
  sh::turned_rows({0, 0, 0}, rows);
  for (int i = 0; i < 9; i++) CHECK(rows[i] == (i % 4 == 0 ? 1.0 : 0.0));
}

TEST(the_frame_is_forward_left_and_up) {
  // Yaw 90: the model's +x (forward) points along the map's +y, its +y (left) along -x.
  const sh::Vec3f f = sh::to_world({1, 0, 0}, {0, 90, 0});
  CHECK(near(f.x, 0) && near(f.y, 1) && near(f.z, 0));
  const sh::Vec3f l = sh::to_world({0, 1, 0}, {0, 90, 0});
  CHECK(near(l.x, -1) && near(l.y, 0) && near(l.z, 0));
  // Pitch is down-positive: pitch 90 tips the model's forward straight down.
  const sh::Vec3f d = sh::to_world({1, 0, 0}, {90, 0, 0});
  CHECK(near(d.x, 0) && near(d.z, -1));
  // Roll lowers the right side, so roll 90 lifts the left (+y) straight up.
  const sh::Vec3f r = sh::to_world({0, 1, 0}, {0, 0, 90});
  CHECK(near(r.y, 0) && near(r.z, 1));
}

TEST(to_model_and_to_world_undo_each_other) {
  const sh::Vec3f p{37, -12, 5.5f};
  for (int k = 0; k < 24; k++) {
    const auto a = static_cast<float>(k);
    const sh::Vec3f angles{a * 7.5f - 80, a * 15, a * 11 - 40};
    const sh::Vec3f back = sh::to_world(sh::to_model(p, angles), angles);
    CHECK(near(back.x, p.x) && near(back.y, p.y) && near(back.z, p.z));
  }
}

TEST(the_reach_holds_every_corner) {
  // A cube 96 units each way: its corner is 96 root 3 away.
  CHECK(near(sh::turned_reach({-96, -96, -96}, {96, 96, 96}), 166.276878f));
  // A lopsided box reaches its farther face on each axis.
  CHECK(near(sh::turned_reach({-3, -40, 0}, {4, 10, 0}), sh::root_of(16 + 1600)));
}

TEST(the_drawn_rows_put_a_vertex_where_the_trace_finds_it) {
  // A model vertex in the map's frame, turned by the trace's frame then taken to the engine's
  // axes, lands where the rows put the same vertex taken to the engine's axes first.
  const float l[3] = {48, -20, 9};
  for (int k = 0; k < 16; k++) {
    const auto a = static_cast<float>(k);
    const sh::Vec3f angles{a * 9 - 70, a * 23, a * 13 - 100};
    const sh::Vec3f w = sh::to_world({l[0], l[1], l[2]}, angles);
    const float wf[3] = {w.x, w.y, w.z};
    double want[3], v[3];
    ge::brush::to_engine(wf, want);
    ge::brush::to_engine(l, v);
    double rows[9];
    sh::turned_rows(angles, rows);
    for (int i = 0; i < 3; i++) {
      const double* r = &rows[size_t(i) * 3];
      const double got = r[0] * v[0] + r[1] * v[1] + r[2] * v[2];
      CHECK(near(got, want[i]));
    }
  }
}
