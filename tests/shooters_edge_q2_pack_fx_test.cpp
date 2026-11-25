// Tests: Ground Zero's temporary entities as the window draws them in Quake II's own look
// (games/shooters_edge/client/world_fx_q2.cpp, q2_model_fx.hpp, q2_temp_fx.hpp): the green
// blaster's and the flechette's hits (specks, the flare's skin, the light), more blood, electric
// sparks, the heat beam's sparks and steam, the plain, big and speckless explosions, the
// disruptor's burst and its darkening light, the A-M bomb's growing blast, the tesla's bolt and the
// heat beam held by their sources, laid as links, and the sounds the client plays for each.
//
// ge:summary Tests: Ground Zero's temporary entities drawn and heard as Quake II draws them
// ge:keywords test q2 ground-zero te_flechette te_blaster2 te_moreblood te_electric_sparks
// ge:keywords te_heatbeam te_lightning te_tracker_explosion te_nukeblast te_explosion1_big
// ge:keywords q2_links q2_held_line q2_pack_point mission-pack
// ge:skill shooter testing
#include <cmath>
#include <string_view>

#include "check.hpp"
#include "shooters_edge/client/q2_model_fx.hpp"
#include "shooters_edge/client/q2_temp_fx.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/client/world_models.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace sh = ge::shooter;

cl::FxState state() {
  cl::FxState s;
  s.palette.assign(768, 128);
  return s;
}

cl::Effect point(std::string_view name, int code) {
  cl::Effect e;
  e.kind = cl::EffectKind::named;
  e.name = name;
  e.code = code;
  e.at = e.end = {0, 0, 0};
  e.way = {0, 0, 1};
  return e;
}

bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

// The models the held lines lay this frame: how many are `m`.
int links(const cl::FxFrame& out, cl::Q2FxModel m) {
  const int index = static_cast<int>(cl::EffectModel::q2_fx) + static_cast<int>(m);
  int n = 0;
  for (const cl::Placed& p : out.models) n += p.modelindex == index ? 1 : 0;
  return n;
}

}  // namespace

TEST(the_packs_point_effects_are_named_both_ways) {
  for (const cl::Q2PackTe& t : cl::kQ2PackTes) {
    CHECK(cl::q2_pack_te(t.code) == t.name);
    CHECK(cl::q2_pack_te_code(t.name) == t.code);
  }
  CHECK(cl::q2_pack_te(5).empty() && cl::q2_pack_te_code("te_blood") == 0);
}

TEST(the_client_plays_each_effects_own_sound) {
  for (int code : {30, 43, 44, 46, 55})
    CHECK(std::string_view(cl::q2_temp_sound(code, 0).name) == "weapons/lashit.wav");
  CHECK(std::string_view(cl::q2_temp_sound(47, 0).name) == "weapons/disrupthit.wav");
  CHECK(std::string_view(cl::q2_temp_sound(53, 0).name) == "weapons/rocklx1a.wav");
  CHECK(std::string_view(cl::q2_temp_sound(54, 0).name) == "weapons/rocklx1a.wav");
  CHECK(cl::q2_temp_sound(35, 0).name == nullptr);  // the plain explosion is silent
  CHECK(cl::q2_temp_sound(42, 0).name == nullptr);  // so is more blood
}

TEST(the_green_blasters_and_flechettes_hits_wear_the_flares_skins_and_lights) {
  cl::FxState s = state();
  cl::FxFrame out;
  const double p[3] = {0, 0, 0};
  CHECK(cl::q2_pack_point(s, point("te_blaster2", 30), p, 0, out));
  CHECK(out.births.size() == 40 && s.q2_models.size() == 1);
  CHECK(s.q2_models[0].skin == 1 && s.q2_models[0].light == 150);
  CHECK(s.q2_models[0].rgb[0] == 0 && s.q2_models[0].rgb[1] == 1 && s.q2_models[0].rgb[2] == 0);
  CHECK(cl::q2_pack_point(s, point("te_flechette", 55), p, 0, out));
  CHECK(out.births.size() == 80 && s.q2_models.size() == 2);
  CHECK(s.q2_models[1].skin == 2 && near(s.q2_models[1].rgb[2], 0.75));
  // The flare fades over 0.3 s, its light with it.
  const cl::Q2FxPose half = cl::q2_model_fx_pose(s.q2_models[1], 0.15, true);
  CHECK(half.alive && half.skin == 2 && near(half.alpha, 0.5) && near(half.light, 75));
  CHECK(!cl::q2_model_fx_pose(s.q2_models[1], 0.31, true).alive);
}

TEST(blood_sparks_and_steam_throw_their_counts) {
  const double p[3] = {0, 0, 0};
  const std::pair<std::string_view, size_t> kinds[] = {{"te_moreblood", 250},
                                                       {"te_electric_sparks", 40},
                                                       {"te_heatbeam_sparks", 50},
                                                       {"te_heatbeam_steam", 20}};
  for (const auto& [name, count] : kinds) {
    cl::FxState s = state();
    cl::FxFrame out;
    CHECK(cl::q2_pack_point(s, point(name, cl::q2_pack_te_code(name)), p, 0, out));
    CHECK(out.births.size() == count && s.q2_models.empty());
  }
  cl::FxState s = state();
  cl::FxFrame out;
  CHECK(!cl::q2_pack_point(s, point("te_blaster", 2), p, 0, out));  // not the pack's
}

TEST(the_plain_big_and_speckless_explosions_are_the_rockets_ball) {
  const ge::classic::Q2FxSwitches q;
  const float at[3] = {0, 0, 0}, up[3] = {0, 0, 1};
  for (int code : {35, 53, 54}) {
    cl::Q2ModelFx m[2];
    CHECK(cl::q2_model_fx_of(code, 3, at, up, 0, q, m) == 1);
    CHECK(m[0].model == cl::Q2FxModel::explosion && m[0].frames == 15 && m[0].base == 15);
    CHECK(m[0].light == 350 && m[0].scale == (code == 53 ? 2.0f : 1.0f));
  }
  CHECK(cl::q2_blast_code("te_explosion1_big") == 53 && cl::q2_blast_code("te_nukeblast") == 51);
}

TEST(the_disruptors_burst_throws_black_specks_and_darkens_for_a_tenth) {
  cl::FxState s = state();
  cl::FxFrame out;
  const double p[3] = {0, 0, 0};
  CHECK(cl::q2_classic_blast(s, 47, {0, 0, 0}, p, false, 0, out));
  CHECK(out.births.size() == 128 && s.q2_models.size() == 1 && !s.q2_models[0].drawn);
  const cl::Q2FxPose lit = cl::q2_model_fx_pose(s.q2_models[0], 0.05, true);
  CHECK(lit.alive && lit.light == 150 && s.q2_models[0].rgb[0] == -1);
  CHECK(!cl::q2_model_fx_pose(s.q2_models[0], 0.11, true).alive);
}

TEST(the_am_bombs_blast_grows_for_a_second) {
  cl::FxState s = state();
  cl::FxFrame out;
  const double p[3] = {0, 0, 0};
  CHECK(cl::q2_classic_blast(s, 51, {0, 0, 0}, p, false, 0, out));
  CHECK(out.births.empty() && s.q2_nukes.size() == 1);
  cl::q2_sustains(s, 0.5, out);
  CHECK(out.births.size() == 700);
  out.births.clear();
  cl::q2_sustains(s, 1.01, out);
  CHECK(out.births.empty() && s.q2_nukes.empty());
}

TEST(a_bolts_links_stop_short_of_the_tesla_and_a_short_one_is_one_link_at_the_end) {
  const cl::Q2Links bolt = cl::q2_links(100, true);
  CHECK(bolt.n == 3 && near(bolt.length, 80) && near(bolt.step, 22.5) && !bolt.alone);
  const cl::Q2Links short_bolt = cl::q2_links(50, true);
  CHECK(short_bolt.n == 1 && short_bolt.alone);
  const cl::Q2Links beam = cl::q2_links(100, false);
  CHECK(beam.n == 4 && near(beam.step, 68.0 / 3));
}

TEST(a_bolt_and_a_heat_beam_are_held_by_their_sources_and_let_go) {
  cl::FxState s = state();
  cl::Effect bolt;
  bolt.kind = cl::EffectKind::named_trail;
  bolt.name = "te_lightning";
  bolt.entity = 30;
  bolt.colour = 40;  // what it strikes
  bolt.at = {0, 0, 0};
  bolt.end = {100, 0, 0};
  CHECK(cl::q2_held_line(s, bolt, 0));
  CHECK(cl::q2_held_line(s, bolt, 0.05));  // the same pair: one bolt
  CHECK(s.q2_held.size() == 1);
  cl::Effect beam = bolt;
  beam.name = "te_monster_heatbeam";
  beam.colour = 0;
  CHECK(cl::q2_held_line(s, beam, 0.05));
  CHECK(s.q2_held.size() == 2 && near(s.q2_held[1].until, 0.15));  // a first beam: 0.1 s
  CHECK(cl::q2_held_line(s, beam, 0.1));
  CHECK(s.q2_held.size() == 2 && near(s.q2_held[1].until, 0.3));  // sent again: 0.2 s on
  const cl::PlayerState me;
  cl::FxFrame out;
  cl::q2_draw_model_fx(s, 0, me, 0, 0.12, out);
  CHECK(links(out, cl::Q2FxModel::lightning) == 3);
  CHECK(links(out, cl::Q2FxModel::heat_beam) == 4);
  CHECK(out.births.size() == 40);  // the monster's shell at its gun
  cl::FxFrame later;
  cl::q2_draw_model_fx(s, 0, me, 0, 0.26, later);
  CHECK(links(later, cl::Q2FxModel::lightning) == 0 && links(later, cl::Q2FxModel::heat_beam) == 4);
  cl::FxFrame gone;
  cl::q2_draw_model_fx(s, 0, me, 0, 0.31, gone);
  CHECK(gone.models.empty() && s.q2_held.empty());
  cl::Effect other = bolt;
  other.name = "te_railtrail";
  CHECK(!cl::q2_held_line(s, other, 0));
}

TEST(the_reckonings_plasma_explosion_is_explosion1_out_of_the_players_say) {
  ge::classic::Q2FxSwitches q;
  q.no_explosions = ge::classic::kQ2Rocket;
  q.light_hacks = 2;
  const float at[3] = {0, 0, 0}, up[3] = {0, 0, 1};
  cl::Q2ModelFx m[2];
  CHECK(cl::q2_blast_code("te_plasma_explosion") == 28);
  CHECK(cl::q2_model_fx_of(28, 0, at, up, 0, q, m) == 1);
  CHECK(m[0].drawn && m[0].light == 350 && m[0].frames == 15);
  CHECK(std::string_view(cl::q2_temp_sound(28, 0).name) == "weapons/rocklx1a.wav");
  cl::FxState s = state();
  cl::FxFrame out;
  const double p[3] = {0, 0, 0};
  CHECK(cl::q2_classic_blast(s, 28, {0, 0, 0}, p, false, 0, out));
  CHECK(out.births.size() == 256 && s.q2_models.size() == 1);
}
