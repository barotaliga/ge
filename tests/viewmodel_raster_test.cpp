// The held model's CPU raster (engine/viewmodel/raster.hpp): a front face is drawn and its back
// is not, the nearer of two faces wins, a face cut by the near plane still fills what is in front
// of it, a glowing texel ignores the light, the resolve's bytes are the same twice, and the
// resolve laid over a picture keeps that picture where nothing was drawn.
//
// ge:summary Tests: the CPU raster's culling, depth, near clip, glow and repeatable resolve
// ge:keywords test viewmodel raster draw_lit depth near-clip back-face glow resolve
// ge:skill render testing
// ge:test tests/viewmodel_raster_test.cpp
#include <vector>

#include "check.hpp"
#include "viewmodel/raster.hpp"

namespace vm = ge::viewmodel;

namespace {

const float kBlack[3] = {0, 0, 0};

// A square facing the eye at depth z (front toward -z), half size h.
void square(std::vector<float>& p, std::vector<float>& n, std::vector<uint32_t>& idx, float z,
            float h, bool facing = true) {
  const uint32_t b = static_cast<uint32_t>(p.size() / 3);
  const float q[12] = {-h, -h, z, -h, h, z, h, h, z, h, -h, z};
  p.insert(p.end(), q, q + 12);
  for (int i = 0; i < 4; ++i) n.insert(n.end(), {0, 0, facing ? -1.0f : 1.0f});
  // (b - a) x (c - a) for a=(-h,-h) b=(-h,h) c=(h,h) is (0,2h,0) x (2h,2h,0) = (0,0,-4h^2): -z.
  if (facing)
    idx.insert(idx.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
  else
    idx.insert(idx.end(), {b, b + 2, b + 1, b, b + 3, b + 2});
}

vm::Lighting bright() {
  vm::Lighting l;
  l.ambient[0] = l.ambient[1] = l.ambient[2] = 1;
  l.gloss = 0;
  return l;
}

float centre_red(const vm::ViewTarget& t) {
  return t.rgb[(size_t(t.height / 2) * size_t(t.width) + size_t(t.width / 2)) * 3];
}

}  // namespace

TEST(a_front_face_is_drawn_and_a_back_face_is_not) {
  for (bool facing : {true, false}) {
    vm::ViewTarget t = vm::make_target(32, 32, 1, kBlack);
    std::vector<float> p, n, c;
    std::vector<uint32_t> idx;
    square(p, n, idx, 1, 0.5f, facing);
    c.assign(p.size(), 1.0f);
    vm::DrawLit d;
    d.positions = p;
    d.normals = n;
    d.indices = idx;
    d.colour = c;
    vm::draw_lit(t, d, bright());
    CHECK((centre_red(t) > 0) == facing);
  }
}

TEST(the_nearer_face_wins) {
  vm::ViewTarget t = vm::make_target(32, 32, 1, kBlack);
  std::vector<float> p, n, c;
  std::vector<uint32_t> idx;
  square(p, n, idx, 2, 1);
  square(p, n, idx, 1, 0.2f);
  c.assign(p.size(), 0.0f);
  for (size_t i = 12; i < 24; i += 3) c[i] = 1;  // the near square red, the far one black
  vm::DrawLit d;
  d.positions = p;
  d.normals = n;
  d.indices = idx;
  d.colour = c;
  vm::draw_lit(t, d, bright());
  CHECK(centre_red(t) > 0.5f);
}

TEST(a_face_cut_by_the_near_plane_fills_in_front) {
  vm::ViewTarget t = vm::make_target(32, 32, 1, kBlack);
  // A floor strip from behind the eye to far ahead, under it, facing up.
  const std::vector<float> p = {-1, -0.5f, -1, -1, -0.5f, 5, 1, -0.5f, 5, 1, -0.5f, -1};
  const std::vector<float> n = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
  const std::vector<uint32_t> idx = {0, 1, 2, 0, 2, 3};
  const std::vector<float> c(12, 1.0f);
  vm::DrawLit d;
  d.positions = p;
  d.normals = n;
  d.indices = idx;
  d.colour = c;
  d.cull_back = false;
  vm::draw_lit(t, d, bright());
  // The bottom row of the picture sees the floor just ahead.
  CHECK(t.rgb[(size_t(31) * 32 + 16) * 3] > 0.5f);
}

TEST(a_glowing_texel_ignores_the_light_and_the_resolve_repeats) {
  vm::Skin skin;
  skin.image.width = skin.image.height = 1;
  skin.image.rgba = {255, 128, 0, 255};
  skin.glow = {1};
  vm::Lighting dark;
  dark.floor = 0;
  dark.gloss = 0;
  dark.ambient[0] = dark.ambient[1] = dark.ambient[2] = 0;
  std::vector<float> p, n, uv;
  std::vector<uint32_t> idx;
  square(p, n, idx, 1, 0.5f);
  uv.assign(8, 0.5f);
  auto draw = [&] {
    vm::ViewTarget t = vm::make_target(16, 16, 2, kBlack);
    vm::DrawLit d;
    d.positions = p;
    d.normals = n;
    d.indices = idx;
    d.uv = uv;
    d.skin = &skin;
    vm::draw_lit(t, d, dark);
    return vm::resolve(t, 1.0);
  };
  const std::vector<uint8_t> a = draw(), b = draw();
  CHECK(a == b);
  const uint8_t* c = &a[size_t{8 * 16 + 8} * 4];
  CHECK(c[0] > 200 && c[2] < 10);
}

TEST(a_hidden_triangle_is_not_drawn) {
  for (bool hide : {false, true}) {
    vm::ViewTarget t = vm::make_target(32, 32, 1, kBlack);
    std::vector<float> p, n, c;
    std::vector<uint32_t> idx;
    square(p, n, idx, 1, 0.5f);
    c.assign(p.size(), 1.0f);
    const std::vector<uint8_t> mask = {uint8_t(hide ? 1 : 0), uint8_t(hide ? 1 : 0)};
    vm::DrawLit d;
    d.positions = p;
    d.normals = n;
    d.indices = idx;
    d.colour = c;
    d.hidden = mask;
    vm::draw_lit(t, d, bright());
    CHECK((centre_red(t) > 0) == !hide);
  }
}

TEST(resolve_over_keeps_the_picture_beneath_where_nothing_was_drawn) {
  vm::ViewTarget t = vm::make_target(32, 32, 2, kBlack);
  std::vector<float> p, n, c;
  std::vector<uint32_t> idx;
  square(p, n, idx, 1, 0.1f);
  c.assign(p.size(), 1.0f);
  vm::DrawLit d;
  d.positions = p;
  d.normals = n;
  d.indices = idx;
  d.colour = c;
  vm::draw_lit(t, d, bright());
  std::vector<uint8_t> under(size_t{32} * 32 * 4, 255);
  for (size_t i = 0; i < under.size(); i += 4) under[i] = 40;  // a cyan picture
  const std::vector<uint8_t> over = vm::resolve_over(t, under);
  const std::vector<uint8_t> plain = vm::resolve(t);
  CHECK(over.size() == under.size());
  // A corner: the picture beneath. The centre: the drawn square, as the plain resolve has it.
  CHECK(over[0] == 40 && over[1] == 255);
  const size_t mid = (size_t{16} * 32 + 16) * 4;
  CHECK(over[mid] == plain[mid] && over[mid + 1] == plain[mid + 1]);
}

TEST(coverage_counts_the_pixels_a_model_fills) {
  // A square plate at the grip, facing the eye, placed 0.5 m ahead: through a 90 deg lens a plate
  // 1.2 m across fills the whole picture, one 0.1 m across a small share of it.
  auto plate = [](float half) {
    vm::HeldModel m;
    vm::HeldFrame f;
    f.positions = {-half, -half, 0, half, -half, 0, half, half, 0, -half, half, 0};
    f.normals = {0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1};
    m.frames.push_back(f);
    m.indices = {0, 2, 1, 0, 3, 2};
    m.colour.assign(12, 0.5f);
    return m;
  };
  vm::Placement p;
  p.m[11] = 0.5;  // half a metre ahead of the eye
  const double fov = 3.141592653589793 / 2;
  CHECK(vm::held_coverage(plate(0.6f), p, fov, 64, 64) > 0.99);
  const double small = vm::held_coverage(plate(0.05f), p, fov, 64, 64);
  CHECK(small > 0.005 && small < 0.02);  // (0.1 / 1.0)^2 = 0.01 of the picture
  vm::HeldModel none;
  none.frames.emplace_back();
  CHECK(vm::held_coverage(none, p, fov, 64, 64) == 0);
}
