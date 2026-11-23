// client/q2_model_fx.hpp: Quake II's model effects by age (an explosion's ball, its frames, skins,
// share shown and light, smooth and stepped; a puff; a flash; the BFG's burst), each kind's
// effects and the player's say over them, the cables' segments; and off the wire (demo_q2.cpp): a
// splash's sparks heard, a laser's sparks in their exact colour, the grapple's cable from its
// start plus its offset.
//
// ge:summary Test: Quake II's model effects by age and kind, cables, splash and laser sparks
// ge:keywords test q2 temp-entity model-effect explosion-model explosion-skins smoke flash
// ge:keywords blaster-flare cable segment splash laser-sparks grapple cl_smooth_explosions
// ge:skill shooter render testing
#include "shooters_edge/client/q2_model_fx.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/q2_temp_fx.hpp"

namespace cl = shooters_edge::client;

namespace {

bool near(double a, double b) { return std::fabs(a - b) < 1e-5; }

cl::Q2ModelFx one(int code, uint32_t pick = 0, ge::classic::Q2FxSwitches q = {}) {
  cl::Q2ModelFx m[2];
  const float at[3] = {0, 0, 0}, up[3] = {0, 0, 1};
  CHECK(cl::q2_model_fx_of(code, pick, at, up, 10, q, m) >= 1);
  return m[0];
}

// One protocol 34 message read into a fresh stream.
cl::Demo read_one(const std::vector<uint8_t>& bytes) {
  cl::Demo d;
  cl::q2_stream_begin(d);
  size_t end = 0;
  std::string error;
  CHECK_MSG(cl::q2_message_end(d, bytes, 0, end, error), error);
  CHECK(end == bytes.size());
  return d;
}

}  // namespace

TEST(a_rockets_ball_runs_its_frames_skins_and_light_down_over_1_4_s) {
  const cl::Q2ModelFx r = one(7, 1);  // odd pick: the second run, frames 15..29
  CHECK(r.model == cl::Q2FxModel::explosion && r.frames == 15 && r.base == 15 && r.drawn);
  CHECK(r.light == 350 && r.rgb[0] == 1 && r.rgb[1] == 0.5f && r.full_bright);
  // Smooth (the default): a quarter of a second in, between frames 17 and 18, skin 1.
  cl::Q2FxPose p = cl::q2_model_fx_pose(r, 10.25, true);
  CHECK(p.alive && p.frame_from == 17 && p.frame == 18 && near(p.f, 0.5) && p.skin == 1);
  CHECK(near(p.alpha, 1 - 0.25 / 1.4) && p.see_through && near(p.light, 350 * p.alpha));
  // Skins: 5 for tenths 10..12, 6 from 13.
  CHECK(cl::q2_model_fx_pose(r, 11.05, true).skin == 5);
  CHECK(cl::q2_model_fx_pose(r, 11.35, true).skin == 6);
  CHECK(!cl::q2_model_fx_pose(r, 11.4, true).alive);  // gone at (15 - 1) tenths
  // Stepped: (16 - t) / 16 a whole tenth, solid until the tenth.
  p = cl::q2_model_fx_pose(r, 10.25, false);
  CHECK(near(p.alpha, 14.0 / 16) && !p.see_through && near(p.light, 350 * 14.0 / 16));
  CHECK(cl::q2_model_fx_pose(r, 11.05, false).see_through);
  // Even pick: the first run; a grenade's 19 frames from 30, gone after 1.8 s.
  CHECK(one(7, 0).base == 0);
  const cl::Q2ModelFx g = one(8);
  CHECK(g.frames == 19 && g.base == 30 && cl::q2_model_fx_pose(g, 11.75, true).alive);
  CHECK(!cl::q2_model_fx_pose(g, 11.8, true).alive);
  CHECK(cl::q2_model_fx_pose(g, 11.65, false).alpha == 0);  // stepped: none left at t 16
}

TEST(the_players_say_reaches_a_rockets_and_a_grenades_never_explosion1s) {
  ge::classic::Q2FxSwitches q;
  q.no_explosions = ge::classic::kQ2Rocket;
  q.light_hacks = 2;
  const cl::Q2ModelFx rocket = one(17, 0, q);  // in water: a rocket's still
  CHECK(!rocket.drawn && rocket.light == 200);
  const cl::Q2ModelFx grenade = one(8, 0, q);
  CHECK(grenade.drawn && grenade.light == 200);
  const cl::Q2ModelFx first = one(5, 0, q);
  CHECK(first.drawn && first.light == 350);
  CHECK(cl::q2_blast_kind(18) == ge::classic::kQ2Grenade && cl::q2_blast_kind(5) == 0);
  CHECK(cl::q2_blast_code("te_explosion") == 7 && cl::q2_blast_code("te_bfg_explosion") == 20);
  CHECK(cl::q2_blast_code("te_gunshot") == 0);
}

TEST(a_hit_leaves_its_smoke_and_flash_a_bolt_its_flare_turned_to_the_face) {
  cl::Q2ModelFx m[2];
  const float at[3] = {1, 2, 3}, wall[3] = {0, -1, 0};
  CHECK(cl::q2_model_fx_of(4, 0, at, wall, 2, {}, m) == 2);
  CHECK(m[0].model == cl::Q2FxModel::smoke && m[0].cls == cl::Q2FxClass::puff);
  CHECK(!m[0].full_bright && m[0].see_through && m[0].at[1] == 2);
  CHECK(m[1].model == cl::Q2FxModel::flash && m[1].full_bright);
  const cl::Q2FxPose smoke = cl::q2_model_fx_pose(m[0], 2.15, true);
  CHECK(smoke.alive && near(smoke.alpha, 0.5) && smoke.frame_from == 1);
  CHECK(!cl::q2_model_fx_pose(m[0], 2.31, true).alive);
  const cl::Q2FxPose flash = cl::q2_model_fx_pose(m[1], 2.05, true);
  CHECK(flash.alive && flash.alpha == 1 && flash.frame_from == 0 && flash.frame == 1);
  CHECK(!cl::q2_model_fx_pose(m[1], 2.1, true).alive);
  // The blaster's flare: pitched by the normal's z, yawed by its plan: a -y wall faces 270.
  CHECK(cl::q2_model_fx_of(2, 0, at, wall, 2, {}, m) == 1);
  CHECK(m[0].model == cl::Q2FxModel::flare && near(m[0].angles[0], 90));
  CHECK(near(m[0].angles[1], 270) && m[0].light == 150 && m[0].rgb[2] == 0);
  CHECK(near(cl::q2_model_fx_pose(m[0], 2.1, true).light, 100));
  // The BFG's burst: its sprite's four frames, no lerp, a green light; none for a splash.
  const cl::Q2ModelFx b = one(20);
  const cl::Q2FxPose bp = cl::q2_model_fx_pose(b, 10.25, true);
  CHECK(b.model == cl::Q2FxModel::bfg && bp.frame == 2 && bp.frame_from == 2 && b.rgb[1] == 1);
  CHECK(cl::q2_model_fx_of(10, 0, at, wall, 2, {}, m) == 0);
}

TEST(a_cable_is_a_segment_every_30_units_the_last_30_short_of_its_end) {
  CHECK(cl::q2_cable_segments(0) == 0);
  CHECK(cl::q2_cable_segments(10) == 1 && cl::q2_cable_segment(0, 1, 10) == 0);
  CHECK(cl::q2_cable_segments(100) == 4);
  CHECK(cl::q2_cable_segment(0, 4, 100) == 0);
  CHECK(near(cl::q2_cable_segment(3, 4, 100) * 100, 70));
  CHECK(near(cl::q2_cable_segment(1, 4, 100) * 100, 70.0 / 3));
}

TEST(a_splash_of_sparks_crackles_and_a_lasers_sparks_keep_their_colour) {
  // svc_temp_entity (3), a splash (10): count 8, the point (8, 16, 24) in eighths, a direction,
  // the kind: 1 sparks.
  const std::vector<uint8_t> sparks = {3, 10, 8, 64, 0, 128, 0, 192, 0, 5, 1};
  cl::Demo d = read_one(sparks);
  CHECK(d.state.effects.size() == 1 && d.state.effects[0].name == "sq_splash");
  CHECK(d.state.effects[0].count == 8 && d.state.effects[0].colour == 0xe0);
  CHECK(d.state.effects[0].at.x == 8 && d.state.effects[0].at.z == 24);
  CHECK(d.state.started.size() == 1 && d.state.started[0].attenuation == 3);
  const std::string heard = d.state.started[0].name;
  CHECK_MSG(heard.starts_with("world/spark") && heard.size() == 16, heard);
  // Blue water: its colour, silent.
  std::vector<uint8_t> water = sparks;
  water.back() = 2;
  d = read_one(water);
  CHECK(d.state.effects[0].colour == 0xb0 && d.state.started.empty());
  // A laser's sparks (15): count 4, the colour byte exactly.
  const std::vector<uint8_t> laser = {3, 15, 4, 64, 0, 128, 0, 192, 0, 5, 211};
  d = read_one(laser);
  CHECK(d.state.effects.size() == 1 && d.state.effects[0].name == "sq_laser_sparks");
  CHECK(d.state.effects[0].count == 4 && d.state.effects[0].colour == 211);
  CHECK(d.state.started.empty());
  // The odds: spark5 and spark6 a quarter each, spark7 the rest.
  int five = 0, seven = 0;
  for (uint32_t pick = 0; pick < 400; pick++) {
    const std::string n = cl::q2_temp_sound(10, pick, 1).name;
    five += n == "world/spark5.wav" ? 1 : 0;
    seven += n == "world/spark7.wav" ? 1 : 0;
  }
  CHECK(five == 100 && seven == 200);
  CHECK(cl::q2_temp_sound(10, 0, 4).name == nullptr);
}

TEST(the_grapples_cable_starts_at_its_start_plus_its_offset) {
  // svc_temp_entity (3), the grapple (24): entity 2, start (8, 0, 0), end (80, 0, 0), offset
  // (0, 0, 16).
  const std::vector<uint8_t> grapple = {3, 24, 2, 0, 64, 0, 0, 0, 0, 0,   128,
                                        2, 0,  0, 0, 0,  0, 0, 0, 0, 128, 0};
  cl::Demo d = read_one(grapple);
  CHECK(d.state.effects.size() == 1);
  const cl::DemoEffect& e = d.state.effects[0];
  CHECK(e.name == "te_grapple_cable" && e.entity == 2);
  CHECK(e.at.x == 8 && e.at.z == 16 && e.end.x == 80 && e.way.z == 16);
}
