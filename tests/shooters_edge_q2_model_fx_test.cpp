// client/q2_model_fx.hpp: Quake II's model effects by age (an explosion's ball, its frames, skins,
// share shown and light, smooth and stepped; a puff; a flash; the BFG's burst), each kind's
// effects and the player's say over them, the cables' segments; and off the wire (demo_q2.cpp): a
// splash's sparks heard, a laser's sparks in their exact colour, the grapple's cable from its
// start plus its offset; the mission packs' bolts, blasts, beams and lightning, and their sounds.
//
// ge:summary Test: Quake II's model effects by age and kind, cables, splash and laser sparks
// ge:keywords test q2 temp-entity model-effect explosion-model explosion-skins smoke flash
// ge:keywords blaster-flare cable segment chain chain_share splash laser-sparks grapple
// ge:keywords cl_smooth_explosions mission-packs heat-beam lightning flechette nuke tracker
// ge:skill shooter render testing
#include "shooters_edge/client/q2_model_fx.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/app/weapon_fx.hpp"
#include "shooters_edge/client/chain.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/q2_enhance_fx.hpp"
#include "shooters_edge/client/q2_temp_fx.hpp"

namespace cl = shooters_edge::client;

namespace {

bool near(double a, double b) { return std::fabs(a - b) < 1e-5; }

cl::Q2ModelFx one(int code, uint32_t pick = 0, const ge::classic::Q2FxSwitches& q = {}) {
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

TEST(a_chain_is_a_link_every_30_units_fitted_or_loose) {
  // Quake II's cables fitted: the last link 30 short of the end; Quake's bolts loose.
  CHECK(cl::chain_links(0) == 0);
  CHECK(cl::chain_links(10) == 1 && cl::chain_share(0, 1, 10, true) == 0);
  CHECK(cl::chain_links(100) == 4);
  CHECK(cl::chain_share(0, 4, 100, true) == 0);
  CHECK(near(cl::chain_share(3, 4, 100, true) * 100, 70));
  CHECK(near(cl::chain_share(1, 4, 100, true) * 100, 70.0 / 3));
  CHECK(near(cl::chain_share(3, 4, 100, false) * 100, 90));
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

TEST(the_mission_packs_bolts_flare_green_and_blue_and_their_blasts_keep_their_own_light) {
  cl::Q2ModelFx m[2];
  const float at[3] = {0, 0, 0}, up[3] = {0, 0, 1};
  // The ion bolt (30) and the flechette (55): the blaster's flare on skins 1 and 2, light 150.
  CHECK(cl::q2_model_fx_of(30, 0, at, up, 2, {}, m) == 1);
  CHECK(m[0].model == cl::Q2FxModel::flare && m[0].skin == 1 && m[0].light == 150);
  CHECK(m[0].rgb[0] == 0 && m[0].rgb[1] == 1 && m[0].rgb[2] == 0);
  CHECK(cl::q2_model_fx_pose(m[0], 2.05, true).skin == 1);
  CHECK(cl::q2_model_fx_of(55, 0, at, up, 2, {}, m) == 1);
  CHECK(m[0].skin == 2 && near(m[0].rgb[2], 0.75));
  // The big blast (53) twice the size; the plain and no-specks blasts lit; 62 and 63 dark.
  CHECK(one(53).scale == 2 && one(53).light == 350 && one(35).scale == 1);
  CHECK(one(35).light == 350 && one(54).light == 350);
  CHECK(one(62).light == 0 && one(63).light == 0 && one(63).base == 30);
  CHECK(cl::q2_blast_code("te_nukeblast") == 51 && cl::q2_blast_code("te_explosion1_np") == 54);
  // The plasma beam's side: the hand, the centre gun drawn on the right, a hidden gun centred.
  CHECK(cl::beam_hand(1, 1) == 1 && cl::beam_hand(0, 3) == 1 && cl::beam_hand(2, 2) == 0);
  // Their sounds, played by the client: the bolts' hit, the tracker's, the big blasts'.
  CHECK(std::string(cl::q2_temp_sound(30, 0, -1).name) == "weapons/lashit.wav");
  CHECK(std::string(cl::q2_temp_sound(47, 0, -1).name) == "weapons/disrupthit.wav");
  CHECK(std::string(cl::q2_temp_sound(53, 0, -1).name) == "weapons/rocklx1a.wav");
  CHECK(cl::q2_puff(42) != nullptr && cl::q2_puff(42)->count == 250);
}

TEST(the_lightnings_links_are_35_units_and_face_back_to_the_start) {
  CHECK(cl::chain_links(70, cl::kQ2LightningStep) == 2);
  CHECK(near(cl::chain_share(1, 2, 70, true, cl::kQ2LightningStep) * 70, 35));
  CHECK(cl::chain_links(64, cl::kQ2HeatBeamStep) == 2);
}

TEST(the_packs_bolts_beams_and_lightning_read_off_the_wire_by_name) {
  // The ion bolt (30): point (8, 16, 24), direction 5; its hit heard.
  cl::Demo d = read_one({3, 30, 64, 0, 128, 0, 192, 0, 5});
  CHECK(d.state.effects.size() == 1 && d.state.effects[0].name == "te_blaster2");
  CHECK(d.state.started.size() == 1);
  CHECK(std::string(d.state.started[0].name) == "weapons/lashit.wav");
  CHECK(read_one({3, 55, 64, 0, 128, 0, 192, 0, 5}).state.effects[0].name == "te_flechette");
  CHECK(read_one({3, 44, 64, 0, 128, 0, 192, 0, 5}).state.effects[0].name == "te_heatbeam_steam");
  // The heat beam (38): entity 3, start, end.
  d = read_one({3, 38, 3, 0, 64, 0, 0, 0, 0, 0, 128, 2, 0, 0, 0, 0});
  CHECK(d.state.effects.size() == 1 && d.state.effects[0].name == "te_heatbeam");
  CHECK(d.state.effects[0].entity == 3 && d.state.effects[0].end.x == 80);
  // The tesla's lightning (33): source 5, target 7, start, end; its crackle on the source.
  d = read_one({3, 33, 5, 0, 7, 0, 64, 0, 0, 0, 0, 0, 128, 2, 0, 0, 0, 0});
  CHECK(d.state.effects.size() == 1);
  const cl::DemoEffect& e = d.state.effects[0];
  CHECK(e.name == "te_lightning" && e.entity == 5 && e.count == 7 && e.end.x == 80);
  CHECK(d.state.started.size() == 1 && d.state.started[0].entity == 5);
  CHECK(d.state.started[0].channel == 2);
  CHECK(std::string(d.state.started[0].name) == "weapons/tesla.wav");
}

// The mission packs under classic enhance: each blast's kind by its code, the hits that are not a
// bullet's in their own colours (only the electric sparks lighting the face), a heat beam a hot
// strip 3 units wide.
TEST(the_mission_packs_blasts_hits_and_heat_beams_under_classic_enhance) {
  namespace fx = shooters_edge::effects;
  static_assert(cl::q2_blast_of(7) == fx::BlastOf::common);
  CHECK(cl::q2_blast_of(53) == fx::BlastOf::big && cl::q2_blast_of(51) == fx::BlastOf::huge);
  CHECK(cl::q2_blast_of(47) == fx::BlastOf::dark && cl::q2_blast_of(35) == fx::BlastOf::bare);
  CHECK(cl::q2_blast_of(54) == fx::BlastOf::bare && cl::q2_blast_of(17) == fx::BlastOf::common);
  cl::Q2PackHit h;
  CHECK(!cl::q2_pack_hit_of(0, &h) && !cl::q2_pack_hit_of(9, &h));
  CHECK(cl::q2_pack_hit_of(30, &h) && h.hit.rgb[1] > h.hit.rgb[0] && !h.flick);  // green
  CHECK(cl::q2_pack_hit_of(46, &h) && h.flick && h.hit.rgb[2] == 1);             // blue-white, lit
  CHECK(cl::q2_pack_hit_of(44, &h) && h.hit.fall < 0);                           // steam rises
  for (const int code : {43, 55}) CHECK(cl::q2_pack_hit_of(code, &h) && !h.flick);
  fx::Particles sparks;
  const double p[3] = {0, 0, 0}, up[3] = {0, 1, 0};
  CHECK(cl::q2_pack_hit_of(43, &h));
  fx::tinted_hit(p, up, h.hit, 4, &sparks);
  CHECK(int(sparks.size()) == h.hit.count && sparks[0].tint[0] == 1);
  CHECK(cl::kQ2HeatBeamU == 3 && cl::kQ2HeatBeamRgb[0] == 1 && cl::kQ2HeatBeamRgb[2] < 0.2f);
}
