// The classic pack's held weapons as view models (plugins/classicenhance/weapons.hpp), on the
// fixtures alone: the morph fixture model turned into the held frame keeps its fronts outward,
// its frames, its texture coordinates and its fullbright texels as the glow; the light under a
// point of the fixture map is its floor's lightmap, brighter under a lamp than in a corner, and
// nothing below the map is nothing; the map's torches become live lights; and the pack's held
// weapons are found by name, sorted, a damaged one skipped with its reason; each weapon names its
// muzzle fire, and a shot plays the firing frames at ten a second and comes back to rest.
//
// ge:summary Tests the classic held weapons: the held frame, winding, glow, the floor's light
// ge:keywords test classicenhance viewmodel held-weapon morph winding glow fullbright probe
// ge:keywords lightmap torch flame pack weapons
// ge:skill classicenhance testing
#include <cmath>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classicenhance/weapons.hpp"
#include "morph/write.hpp"
#include "vfs/pak.hpp"

namespace ce = ge::classicenhance;
namespace mo = ge::morph;
namespace vm = ge::viewmodel;

namespace {

ce::Palette grey_palette() {
  ce::Palette p;
  for (size_t i = 0; i < 256; i++) {
    for (size_t c = 0; c < 3; c++) p.rgb[i * 3 + c] = uint8_t(i);
  }
  return p;
}

ge::brush::Map fixture_map() {
  std::vector<uint8_t> bytes;
  std::string error;
  ge::brush::Map map;
  CHECK_MSG(
      ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes, &error),
      error);
  CHECK_MSG(ge::brush::read_bsp(bytes, &map, &error), error);
  return map;
}

}  // namespace

TEST(held_from_morph_puts_the_eye_frame_into_the_held_frame) {
  const mo::Model m = mo::sample_model();
  const vm::HeldModel h = ce::held_from_morph(m, grey_palette());
  CHECK(h.frames.size() == m.frames.size());
  CHECK(h.vertex_count() == m.vertex_count());
  CHECK(h.uv == m.st);
  // The file's +x (forward) is the held frame's +z, its +y (left) the held frame's -x, its +z up.
  for (size_t f = 0; f < m.frames.size(); f++) {
    for (size_t v = 0; v < m.vertex_count(); v++) {
      const float* in = &m.frames[f].positions[v * 3];
      const float* out = &h.frames[f].positions[v * 3];
      CHECK(std::fabs(out[0] - float(-in[1] * ge::brush::kUnitM)) < 1e-6f);
      CHECK(std::fabs(out[1] - float(in[2] * ge::brush::kUnitM)) < 1e-6f);
      CHECK(std::fabs(out[2] - float(in[0] * ge::brush::kUnitM)) < 1e-6f);
    }
  }
}

TEST(held_from_morph_keeps_every_front_outward) {
  // The fixture is an octahedron round its origin: a front points away from the centre. The
  // reflection into the held frame would turn every front in; the winding turns them back.
  const vm::HeldModel h = ce::held_from_morph(mo::sample_model(), grey_palette());
  const std::vector<float>& p = h.frames[0].positions;
  int outward = 0;
  for (size_t t = 0; t + 2 < h.indices.size(); t += 3) {
    const float* a = &p[size_t{h.indices[t]} * 3];
    const float* b = &p[size_t{h.indices[t + 1]} * 3];
    const float* c = &p[size_t{h.indices[t + 2]} * 3];
    const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                        e1[0] * e2[1] - e1[1] * e2[0]};
    const float mid[3] = {a[0] + b[0] + c[0], a[1] + b[1] + c[1], a[2] + b[2] + c[2]};
    if (n[0] * mid[0] + n[1] * mid[1] + n[2] * mid[2] > 0) outward++;
    // The smooth normals point out too.
    const float* na = &h.frames[0].normals[size_t{h.indices[t]} * 3];
    CHECK(na[0] * a[0] + na[1] * a[1] + na[2] * a[2] > 0);
  }
  CHECK(outward == int(h.indices.size() / 3));
}

TEST(held_from_morph_marks_the_fullbright_texels_as_glow) {
  const mo::Model m = mo::sample_model();
  const vm::HeldModel lone = ce::held_from_morph(m, grey_palette(), 0);
  CHECK(lone.skin.image.width == m.skins[0].width && lone.skin.glow.empty());  // none fullbright
  const vm::HeldModel lit = ce::held_from_morph(m, grey_palette(), 1);
  CHECK(lit.skin.glow.size() == m.skins[1].indices.size());
  int glowing = 0;
  for (size_t i = 0; i < m.skins[1].indices.size(); i++) {
    const bool full = m.skins[1].indices[i] >= ce::kFullbrightFirst;
    CHECK((lit.skin.glow[i] != 0) == full);
    glowing += full ? 1 : 0;
    CHECK(lit.skin.image.rgba[i * 4] == m.skins[1].indices[i]);  // the grey palette's own index
  }
  CHECK(glowing > 0);
}

TEST(classic_weapon_row_holds_the_weapon_at_the_eye) {
  const vm::PlaceRow row = ce::classic_weapon_row();
  CHECK(row.offset_m[0] == 0 && row.offset_m[1] == 0 && row.offset_m[2] == 0);
  CHECK(row.bob_forward == vm::classic_row().bob_forward);
}

TEST(probe_under_reads_the_floor_lightmap) {
  const ge::brush::Map map = fixture_map();
  bool found = false;
  // Under the hall's lamp (256, 192, 200), and in its far corner.
  const float lamp[3] = {256, 192, 48};
  const vm::Probe under = ce::probe_under(map, lamp, &found);
  CHECK(found);
  const float corner[3] = {12, 12, 48};
  const vm::Probe dim = ce::probe_under(map, corner, &found);
  CHECK(found);
  CHECK(under.ambient[0] > dim.ambient[0] && dim.ambient[0] > 0);
  CHECK(under.direct[0] > 0 && under.toward[1] == 1.0f);
  // Below the whole map there is no floor: the probe is dark and says so.
  const float below[3] = {256, 192, -4096};
  const vm::Probe none = ce::probe_under(map, below, &found);
  CHECK(!found && none.ambient[0] == 0 && none.direct[0] == 0);
}

TEST(flame_lights_are_the_maps_torches) {
  ge::brush::Map map;
  ge::brush::Entity torch;
  torch.pairs = {
      {"classname", "light_torch_small_walltorch"}, {"origin", "64 32 40"}, {"light", "250"}};
  ge::brush::Entity flame;
  flame.pairs = {{"classname", "light_flame_large_yellow"}, {"origin", "0 0 0"}};
  ge::brush::Entity plain;
  plain.pairs = {{"classname", "light"}, {"origin", "0 0 0"}};
  map.entities = {torch, flame, plain};
  const std::vector<vm::PointLight> lights = ce::flame_lights(map);
  CHECK(lights.size() == 2);
  const float o[3] = {64, 32, 40};
  double at[3];
  ge::brush::to_engine(o, at);
  for (size_t k = 0; k < 3; k++) CHECK(lights[0].position[k] == at[k]);
  CHECK(std::fabs(lights[0].range_m - float(250 * ge::brush::kUnitM)) < 1e-5f);
  CHECK(lights[1].range_m < lights[0].range_m);  // the default 200
}

TEST(pack_weapons_finds_the_held_models_by_name) {
  const std::vector<uint8_t> model = mo::write_model(mo::sample_model());
  std::vector<uint8_t> damaged(model.begin(), model.begin() + 40);
  std::vector<uint8_t> pack;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak({{"progs/v_two.mdl", model},
                                {"progs/g_two.mdl", model},
                                {"progs/v_one.mdl", model},
                                {"progs/v_bad.mdl", damaged}},
                               &pack, &error),
            error);
  std::vector<std::string> skipped;
  const std::vector<ce::PackWeapon> w = ce::pack_weapons(pack, &skipped);
  CHECK(w.size() == 2 && w[0].name == "progs/v_one.mdl" && w[1].name == "progs/v_two.mdl");
  CHECK(w[0].model.frames.size() == 3);
  CHECK(skipped.size() == 1 && skipped[0].find("v_bad") != std::string::npos);
}

TEST(classic_fire_names_each_weapons_muzzle_look) {
  const ce::ClassicFire* shot = ce::classic_fire_of("progs/v_shot.mdl");
  CHECK(shot != nullptr && shot->muzzle == "muzzle.shotgun" && shot->refire_s > 0);
  CHECK(ce::classic_fire_of("v_rock2")->muzzle == "muzzle.rocket");
  CHECK(ce::classic_fire_of("progs/v_axe.mdl")->muzzle.empty());  // a blade throws no flame
  CHECK(ce::classic_fire_of("progs/g_shot.mdl") == nullptr);
  for (const ce::ClassicFire& f : ce::classic_fires()) CHECK(f.refire_s > 0);
}

TEST(classic_fire_frame_plays_the_firing_frames_then_rests) {
  int a = -1, b = -1;
  double t = -1;
  ce::classic_fire_frame(7, 0, &a, &b, &t);
  CHECK(a == 1 && b == 2 && t == 0);
  ce::classic_fire_frame(7, 0.25, &a, &b, &t);  // halfway through the third
  CHECK(a == 3 && b == 4 && std::fabs(t - 0.5) < 1e-9);
  ce::classic_fire_frame(7, 0.55, &a, &b, &t);  // the last firing frame turns back to rest
  CHECK(a == 6 && b == 0);
  ce::classic_fire_frame(7, 0.61, &a, &b, &t);
  CHECK(a == 0 && b == 0 && t == 0);
  ce::classic_fire_frame(1, 0.05, &a, &b, &t);  // one frame: nothing to play
  CHECK(a == 0 && b == 0);
}
