// The village's buildings as drawn: a mesh a kind, the plinth, the looks (building_look.hpp).
//
// ge:summary The buildings' walls, gables, snowed roofs, windows, balconies, tower and spire
// ge:keywords village building-mesh gable roof eaves soffit windows balconies dormers spire
// ge:keywords plinth building-looks render timber
// ge:skill mountain render
// ge:test tests/mountain_building_look_test.cpp
#include "mountain/app/building_look.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <utility>

namespace ge::mountain::app {

namespace {

namespace vg = village;
using vg::BuildingKind;

using Rgb = std::array<float, 3>;
// A look's colours in part order: the render, the boards, the stone, the snow, the glass, the
// frames and the steel. Linear rgb.
using Look = std::array<Rgb, kBuildingParts>;

constexpr Rgb kSnowRgb{0.80f, 0.83f, 0.88f};
constexpr Rgb kPaneRgb{0.018f, 0.026f, 0.036f};
constexpr Rgb kFrameRgb{0.055f, 0.036f, 0.025f};
constexpr Rgb kStoneRgb{0.21f, 0.20f, 0.19f};
constexpr Rgb kDarkBoards{0.085f, 0.048f, 0.026f};
constexpr Rgb kHoneyBoards{0.27f, 0.14f, 0.055f};
constexpr Rgb kRedBoards{0.19f, 0.055f, 0.035f};
constexpr Rgb kSteelRgb{0.075f, 0.08f, 0.085f};

// The village's looks: white render on dark timber, cream on honey, ochre, pale grey on red-brown
// timber, a faded salmon; the garage's green steel on concrete; the church's white and slate.
constexpr std::array<Look, kBuildingLooks> kLooks{{
    {{{0.70f, 0.70f, 0.68f}, kDarkBoards, kStoneRgb, kSnowRgb, kPaneRgb, kFrameRgb, kSteelRgb}},
    {{{0.64f, 0.56f, 0.40f}, kHoneyBoards, kStoneRgb, kSnowRgb, kPaneRgb, kFrameRgb, kSteelRgb}},
    {{{0.55f, 0.34f, 0.11f}, kDarkBoards, kStoneRgb, kSnowRgb, kPaneRgb, kFrameRgb, kSteelRgb}},
    {{{0.48f, 0.48f, 0.46f}, kRedBoards, kStoneRgb, kSnowRgb, kPaneRgb, kFrameRgb, kSteelRgb}},
    {{{0.58f, 0.33f, 0.26f}, kDarkBoards, kStoneRgb, kSnowRgb, kPaneRgb, kFrameRgb, kSteelRgb}},
    {{{0.36f, 0.36f, 0.35f},
      kDarkBoards,
      {0.30f, 0.30f, 0.29f},
      kSnowRgb,
      kPaneRgb,
      {0.10f, 0.11f, 0.11f},
      {0.10f, 0.17f, 0.13f}}},
    {{{0.74f, 0.73f, 0.70f},
      kDarkBoards,
      {0.26f, 0.25f, 0.24f},
      kSnowRgb,
      kPaneRgb,
      kFrameRgb,
      {0.045f, 0.055f, 0.065f}}},
}};
constexpr uint32_t kGarageLook = 5;
constexpr uint32_t kChurchLook = 6;
// A lamp-lit room seen through the glass, linear rgb (tinted by kWindowNits).
constexpr Rgb kLitRoomRgb{1.0f, 0.68f, 0.38f};

// THE LIGHTS BY THE HOUR: the share of a kind's buildings lit at each hour of the solar day,
// linear between the points, from 0 to 24.
struct Lights {
  double hour;
  double share;
};
// Homes: up before the lifts open, home from the slopes at dusk, in bed by midnight.
constexpr Lights kHomes[] = {{0, 0.10},  {5, 0.06},  {6.5, 0.40},  {8.5, 0.15}, {16, 0.35},
                             {18, 0.75}, {21, 0.85}, {22.5, 0.55}, {24, 0.10}};
// The lodge: breakfast to the restaurant's closing.
constexpr Lights kLodge[] = {{0, 0.05},  {6.5, 0.05}, {7.5, 0.9},
                             {22, 0.95}, {23, 0.10},  {24, 0.05}};
// The hotels: the rooms in the evening, the night porter's after.
constexpr Lights kHotel[] = {{0, 0.20},  {6, 0.25},  {7.5, 0.60},  {9, 0.30}, {16, 0.40},
                             {18, 0.80}, {22, 0.75}, {23.5, 0.30}, {24, 0.20}};
// The ticket office, the rental, the school and the shops: open to the evening, shut after.
constexpr Lights kShops[] = {{0, 0},       {7.5, 0}, {8, 0.9}, {18.5, 0.9},
                             {19.5, 0.25}, {21, 0},  {24, 0}};
// First aid: the patrol's hours, then a light kept on.
constexpr Lights kFirstAid[] = {{0, 0.3}, {8, 0.9}, {17, 0.9}, {18, 0.3}, {24, 0.3}};
// The garage: the groomers go out after the lifts close and work into the night.
constexpr Lights kGarage[] = {{0, 0.8}, {4, 0.8}, {6, 0.2}, {17, 0.2}, {17.5, 0.8}, {24, 0.8}};
// The pump house: the snow guns' night shift.
constexpr Lights kPumps[] = {{0, 0.5}, {6, 0.5}, {7, 0.1}, {18, 0.1}, {19, 0.5}, {24, 0.5}};
// The church: the evening's service and the hours after it.
constexpr Lights kChurch[] = {{0, 0}, {16, 0}, {16.5, 0.9}, {21, 0.9}, {21.5, 0}, {24, 0}};

std::span<const Lights> schedule(BuildingKind kind) {
  switch (kind) {
    case BuildingKind::house:
    case BuildingKind::flats:
      return kHomes;
    case BuildingKind::lodge:
      return kLodge;
    case BuildingKind::hotel:
      return kHotel;
    case BuildingKind::ticket:
    case BuildingKind::rental:
    case BuildingKind::school:
    case BuildingKind::shop:
      return kShops;
    case BuildingKind::first_aid:
      return kFirstAid;
    case BuildingKind::garage:
      return kGarage;
    case BuildingKind::pump_house:
      return kPumps;
    case BuildingKind::church:
      return kChurch;
  }
  return kHomes;
}

void box(render::MeshData& m, float cx, float cy, float cz, float hx, float hy, float hz,
         uint8_t part) {
  const float c[3] = {cx, cy, cz};
  const float h[3] = {hx, hy, hz};
  render::add_box(m, c, h, part);
}

// A box from (x0, y0, z0) to (x1, y1, z1).
void block(render::MeshData& m, float x0, float y0, float z0, float x1, float y1, float z1,
           uint8_t part) {
  box(m, (x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2, (x1 - x0) / 2, (y1 - y0) / 2, (z1 - z0) / 2,
      part);
}

// Triangle a b c wound so its face looks along `toward`.
void face(render::MeshData& m, const float* a, const float* b, const float* c, const float* toward,
          uint8_t part) {
  const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
  const float v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
  const float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                      u[0] * v[1] - u[1] * v[0]};
  if (n[0] * toward[0] + n[1] * toward[1] + n[2] * toward[2] >= 0) {
    render::add_triangle(m, a, b, c, part);
  } else {
    render::add_triangle(m, a, c, b, part);
  }
}

// Quad a b c d (in order round it) looking along `toward`.
void quad(render::MeshData& m, const float* a, const float* b, const float* c, const float* d,
          const float* toward, uint8_t part) {
  face(m, a, b, c, toward, part);
  face(m, a, c, d, toward, part);
}

// A gabled roof over walls `hw` either side of x 0 and `hd` of z 0, from the eaves to the ridge:
// its ridge along z (the gable to the front) or along x. The snow on top, the soffit under in the
// frames' colour, the gable ends in `wall`, and the roof out `over` past the walls all round.
void gable(render::MeshData& m, float hw, float hd, float eaves, float ridge, bool along_z,
           float over, uint8_t wall) {
  // Across the ridge `a` and along it `l`, each as world x or z.
  const float ha = along_z ? hw : hd;
  const float hl = along_z ? hd : hw;
  const auto at = [along_z](float a, float y, float l, float* p) {
    p[0] = along_z ? a : l;
    p[1] = y;
    p[2] = along_z ? l : a;
  };
  const float drop = (ridge - eaves) / ha * over;
  const float thick = 0.22f;
  for (const float side : {-1.0f, 1.0f}) {
    float r0[3];
    float r1[3];
    float e0[3];
    float e1[3];
    at(0, ridge, -hl - over, r0);
    at(0, ridge, hl + over, r1);
    at(side * (ha + over), eaves - drop, hl + over, e1);
    at(side * (ha + over), eaves - drop, -hl - over, e0);
    float up[3];
    at(side * (ridge - eaves), ha, 0, up);
    quad(m, r0, r1, e1, e0, up, kSnow);
    float s0[3];
    float s1[3];
    float f0[3];
    float f1[3];
    at(0, ridge - thick, -hl - over, s0);
    at(0, ridge - thick, hl + over, s1);
    at(side * (ha + over), eaves - drop - thick, hl + over, f1);
    at(side * (ha + over), eaves - drop - thick, -hl - over, f0);
    const float down[3] = {-up[0], -up[1], -up[2]};
    quad(m, s0, s1, f1, f0, down, kFrame);
    // The fascia along the eaves.
    float out[3];
    at(side, 0, 0, out);
    quad(m, e0, e1, f1, f0, out, kFrame);
  }
  // The gable ends, and the roof's edge over them.
  for (const float end : {-1.0f, 1.0f}) {
    float a[3];
    float b[3];
    float c[3];
    float out[3];
    at(-ha, eaves, end * hl, a);
    at(ha, eaves, end * hl, b);
    at(0, ridge, end * hl, c);
    at(0, 0, end, out);
    face(m, a, b, c, out, wall);
    for (const float side : {-1.0f, 1.0f}) {
      float t0[3];
      float t1[3];
      float b0[3];
      float b1[3];
      at(0, ridge, end * (hl + over), t0);
      at(side * (ha + over), eaves - drop, end * (hl + over), t1);
      at(side * (ha + over), eaves - drop - thick, end * (hl + over), b1);
      at(0, ridge - thick, end * (hl + over), b0);
      quad(m, t0, t1, b1, b0, out, kFrame);
    }
  }
}

// Windows `w` wide and `h` tall from sill `y0`, every `pitch` m along each wall named, set a
// hand's breadth proud of walls `hw` and `hd` from the middle; `front` the +z wall, `sides` ±x,
// `back` -z.
struct Walls {
  bool front = true;
  bool sides = true;
  bool back = true;
};
void windows(render::MeshData& m, float hw, float hd, float y0, float w, float h, float pitch,
             Walls walls) {
  const float proud = 0.05f;
  const auto row = [&](float half, const auto& put) {
    const int n = static_cast<int>(std::floor((2 * half - 1.5f) / pitch));
    for (int i = 0; i < n; ++i)
      put((static_cast<float>(i) - static_cast<float>(n - 1) / 2) * pitch);
  };
  const float yc = y0 + h / 2;
  if (walls.front || walls.back) {
    row(hw, [&](float x) {
      if (walls.front) box(m, x, yc, hd, w / 2, h / 2, proud, kPane);
      if (walls.back) box(m, x, yc, -hd, w / 2, h / 2, proud, kPane);
    });
  }
  if (walls.sides) {
    row(hd, [&](float z) {
      box(m, hw, yc, z, proud, h / 2, w / 2, kPane);
      box(m, -hw, yc, z, proud, h / 2, w / 2, kPane);
    });
  }
}

// A door `w` wide and `h` tall in the front wall at x.
void door(render::MeshData& m, float x, float hd, float w, float h) {
  box(m, x, h / 2, hd, w / 2, h / 2, 0.07f, kFrame);
}

// A balcony across the front at floor height y: its floor and its rail of boards.
void balcony(render::MeshData& m, float hw, float hd, float y, float reach) {
  block(m, -hw, y - 0.18f, hd, hw, y, hd + reach, kBoards);
  block(m, -hw, y, hd + reach - 0.08f, hw, y + 1.0f, hd + reach, kBoards);
}

void house(render::MeshData& m, const vg::BuildingSize& s) {
  const auto hw = static_cast<float>(s.width) / 2;
  const auto hd = static_cast<float>(s.depth) / 2;
  const auto eaves = static_cast<float>(s.eaves);
  block(m, -hw, 0, -hd, hw, 2.8f, hd, kStone);
  block(m, -hw, 2.8f, -hd, hw, eaves, hd, kBoards);
  gable(m, hw, hd, eaves, static_cast<float>(s.ridge), true, kEaves * 1.4f, kBoards);
  windows(m, hw, hd, 0.9f, 1.0f, 1.2f, 3.2f, {});
  windows(m, hw, hd, 3.6f, 1.0f, 1.3f, 2.8f, {});
  door(m, -hw * 0.45f, hd, 1.1f, 2.2f);
  balcony(m, hw * 0.8f, hd, 3.0f, 1.2f);
}

void flats(render::MeshData& m, const vg::BuildingSize& s) {
  const auto hw = static_cast<float>(s.width) / 2;
  const auto hd = static_cast<float>(s.depth) / 2;
  const auto eaves = static_cast<float>(s.eaves);
  block(m, -hw, 0, -hd, hw, 1.0f, hd, kStone);
  block(m, -hw, 1.0f, -hd, hw, eaves, hd, kRender);
  gable(m, hw, hd, eaves, static_cast<float>(s.ridge), false, kEaves * 1.2f, kRender);
  const float storey = (eaves - 1.0f) / static_cast<float>(s.floors - 1);
  for (int f = 0; f < s.floors - 1; ++f) {
    const float y = 1.0f + storey * static_cast<float>(f);
    windows(m, hw, hd, y + 0.9f, 1.2f, 1.4f, 3.0f, {});
    if (f > 0) balcony(m, hw * 0.7f, hd, y, 1.3f);
  }
  door(m, 0, hd, 1.6f, 2.3f);
}

void lodge(render::MeshData& m, const vg::BuildingSize& s) {
  const auto hw = static_cast<float>(s.width) / 2;
  const auto hd = static_cast<float>(s.depth) / 2;
  const auto eaves = static_cast<float>(s.eaves);
  block(m, -hw, 0, -hd, hw, 3.2f, hd, kBoards);
  // The restaurant's band of glass round three sides, its mullions, and the wall behind it.
  block(m, -hw, 3.2f, -hd, hw, eaves, hd, kRender);
  block(m, -hw + 0.6f, 3.6f, hd - 0.1f, hw - 0.6f, eaves - 0.5f, hd + 0.05f, kPane);
  block(m, -hw - 0.05f, 3.6f, -hd + 1.5f, -hw + 0.1f, eaves - 0.5f, hd - 0.6f, kPane);
  block(m, hw - 0.1f, 3.6f, -hd + 1.5f, hw + 0.05f, eaves - 0.5f, hd - 0.6f, kPane);
  for (int i = 0; i <= 10; ++i) {
    const float x = -hw + 0.6f + (2 * hw - 1.2f) * static_cast<float>(i) / 10;
    block(m, x - 0.1f, 3.6f, hd, x + 0.1f, eaves - 0.5f, hd + 0.12f, kFrame);
  }
  // The terrace before the glass, on posts.
  block(m, -hw, 3.0f, hd, hw, 3.2f, hd + 3.0f, kBoards);
  for (const float x : {-hw + 0.3f, -hw / 3, hw / 3, hw - 0.3f}) {
    block(m, x - 0.15f, 0, hd + 2.6f, x + 0.15f, 3.0f, hd + 2.9f, kBoards);
  }
  windows(m, hw, hd, 0.9f, 1.4f, 1.3f, 4.0f, {false, true, true});
  door(m, -hw * 0.3f, hd, 2.4f, 2.5f);
  door(m, hw * 0.3f, hd, 2.4f, 2.5f);
  gable(m, hw, hd, eaves, static_cast<float>(s.ridge), false, kEaves * 1.6f, kBoards);
}

void hotel(render::MeshData& m, const vg::BuildingSize& s) {
  const auto hw = static_cast<float>(s.width) / 2;
  const auto hd = static_cast<float>(s.depth) / 2;
  const auto eaves = static_cast<float>(s.eaves);
  block(m, -hw, 0, -hd, hw, 3.0f, hd, kStone);
  block(m, -hw, 3.0f, -hd, hw, eaves, hd, kRender);
  gable(m, hw, hd, eaves, static_cast<float>(s.ridge), true, kEaves * 1.8f, kBoards);
  windows(m, hw, hd, 0.8f, 1.6f, 1.6f, 3.6f, {});
  const float storey = (eaves - 3.0f) / static_cast<float>(s.floors - 1);
  for (int f = 0; f < s.floors - 1; ++f) {
    const float y = 3.0f + storey * static_cast<float>(f);
    windows(m, hw, hd, y + 0.6f, 1.2f, 1.7f, 3.0f, {});
    balcony(m, hw * 0.9f, hd, y + 0.2f, 1.5f);
  }
  door(m, 0, hd, 2.6f, 2.5f);
}

// A shop front: display windows under an awning, a rendered flat over, its gable to the street
// or along it.
void shop(render::MeshData& m, const vg::BuildingSize& s, bool gable_front) {
  const auto hw = static_cast<float>(s.width) / 2;
  const auto hd = static_cast<float>(s.depth) / 2;
  const auto eaves = static_cast<float>(s.eaves);
  block(m, -hw, 0, -hd, hw, 3.2f, hd, kStone);
  block(m, -hw, 3.2f, -hd, hw, eaves, hd, kRender);
  block(m, -hw + 0.5f, 0.5f, hd - 0.05f, hw - 0.5f, 2.7f, hd + 0.05f, kPane);
  door(m, 0, hd + 0.02f, 1.3f, 2.4f);
  // The awning, sloped out over the sidewalk.
  float a[3] = {-hw, 3.1f, hd};
  float b[3] = {hw, 3.1f, hd};
  float c[3] = {hw, 2.6f, hd + 1.4f};
  float d[3] = {-hw, 2.6f, hd + 1.4f};
  const float up[3] = {0, 1, 0.3f};
  const float down[3] = {0, -1, -0.3f};
  quad(m, a, b, c, d, up, kSteel);
  quad(m, a, b, c, d, down, kSteel);
  windows(m, hw, hd, 4.0f, 1.0f, 1.3f, 2.8f, {});
  gable(m, hw, hd, eaves, static_cast<float>(s.ridge), gable_front, kEaves, kBoards);
}

// A hut of boards or render: `glass` a row of windows across its front (the kiosk's counter).
void hut(render::MeshData& m, const vg::BuildingSize& s, uint8_t wall, bool glass) {
  const auto hw = static_cast<float>(s.width) / 2;
  const auto hd = static_cast<float>(s.depth) / 2;
  const auto eaves = static_cast<float>(s.eaves);
  block(m, -hw, 0, -hd, hw, eaves, hd, wall);
  gable(m, hw, hd, eaves, static_cast<float>(s.ridge), false, kEaves, wall);
  if (glass) {
    block(m, -hw + 0.4f, 1.1f, hd - 0.05f, hw - 0.4f, 2.4f, hd + 0.05f, kPane);
    block(m, -hw + 0.3f, 1.0f, hd, hw - 0.3f, 1.1f, hd + 0.5f, kBoards);
    windows(m, hw, hd, 1.1f, 0.9f, 1.0f, 2.5f, {false, true, false});
  } else {
    windows(m, hw, hd, 1.0f, 1.0f, 1.1f, 2.6f, {});
  }
}

void school(render::MeshData& m, const vg::BuildingSize& s) {
  hut(m, s, kBoards, false);
  door(m, 0, static_cast<float>(s.depth) / 2, 1.2f, 2.2f);
  // Two dormers out of the front of the roof, each a box with its window and its snowed cap.
  const auto hd = static_cast<float>(s.depth) / 2;
  const auto eaves = static_cast<float>(s.eaves);
  const float y = eaves + 0.4f;
  for (const float x : {-2.2f, 2.2f}) {
    block(m, x - 0.9f, eaves - 0.2f, hd * 0.2f, x + 0.9f, y + 1.3f, hd - 0.4f, kBoards);
    block(m, x - 0.5f, y, hd - 0.45f, x + 0.5f, y + 0.9f, hd - 0.35f, kPane);
    block(m, x - 1.05f, y + 1.3f, hd * 0.2f, x + 1.05f, y + 1.5f, hd - 0.25f, kSnow);
  }
}

void first_aid(render::MeshData& m, const vg::BuildingSize& s) {
  hut(m, s, kRender, false);
  door(m, 0, static_cast<float>(s.depth) / 2, 2.0f, 2.3f);
}

void garage(render::MeshData& m, const vg::BuildingSize& s) {
  const auto hw = static_cast<float>(s.width) / 2;
  const auto hd = static_cast<float>(s.depth) / 2;
  const auto eaves = static_cast<float>(s.eaves);
  block(m, -hw, 0, -hd, hw, 1.0f, hd, kStone);
  block(m, -hw, 1.0f, -hd, hw, eaves, hd, kSteel);
  // The cladding's ribs down the long walls.
  const int ribs = static_cast<int>(hw);
  for (int i = 0; i < ribs; ++i) {
    const float x = -hw + 1 + 2 * static_cast<float>(i);
    box(m, x, (1.0f + eaves) / 2, -hd, 0.06f, (eaves - 1.0f) / 2, 0.06f, kSteel);
  }
  for (const float x : {-hw * 2 / 3, 0.0f, hw * 2 / 3}) door(m, x, hd, 6.0f, 5.5f);
  windows(m, hw, hd, 3.0f, 2.0f, 1.0f, 5.0f, {false, true, true});
  gable(m, hw, hd, eaves, static_cast<float>(s.ridge), false, kEaves * 0.6f, kSteel);
}

void pump_house(render::MeshData& m, const vg::BuildingSize& s) {
  const auto hw = static_cast<float>(s.width) / 2;
  const auto hd = static_cast<float>(s.depth) / 2;
  const auto top = static_cast<float>(s.ridge);
  block(m, -hw, 0, -hd, hw, top, hd, kStone);
  block(m, -hw - 0.3f, top, -hd - 0.3f, hw + 0.3f, top + 0.15f, hd + 0.3f, kFrame);
  block(m, -hw - 0.25f, top + 0.15f, -hd - 0.25f, hw + 0.25f, top + 0.5f, hd + 0.25f, kSnow);
  door(m, -hw * 0.4f, hd, 2.0f, 2.6f);
  windows(m, hw, hd, 2.8f, 1.2f, 0.6f, 3.0f, {false, true, false});
  // The intake's louvres high in the front wall.
  block(m, hw * 0.2f, 2.6f, hd - 0.05f, hw * 0.8f, 3.6f, hd + 0.08f, kSteel);
}

void church(render::MeshData& m, const vg::BuildingSize& s) {
  const auto hw = static_cast<float>(s.width) / 2;
  const auto hd = static_cast<float>(s.depth) / 2;
  const auto eaves = static_cast<float>(s.eaves);
  const auto side = static_cast<float>(vg::buildings::kTowerSide);
  const auto spire = static_cast<float>(vg::buildings::kSpire);
  // The nave behind the tower, its middle half the tower back.
  const float nave_front = hd - side;
  const float nave_mid = (-hd + nave_front) / 2;
  const float nave_half = (nave_front + hd) / 2;
  block(m, -hw, 0, -hd, hw, 0.8f, nave_front, kStone);
  block(m, -hw, 0.8f, -hd, hw, eaves, nave_front, kRender);
  render::MeshData roof;
  gable(roof, hw, nave_half, eaves, static_cast<float>(s.ridge), true, kEaves * 0.6f, kRender);
  // Built round z 0, moved back over the nave.
  for (size_t i = 0; i + 2 < roof.positions.size(); i += 3) roof.positions[i + 2] += nave_mid;
  const uint32_t base = m.vertex_count();
  m.positions.insert(m.positions.end(), roof.positions.begin(), roof.positions.end());
  m.normals.insert(m.normals.end(), roof.normals.begin(), roof.normals.end());
  m.parts.insert(m.parts.end(), roof.parts.begin(), roof.parts.end());
  for (const uint32_t k : roof.indices) m.indices.push_back(base + k);
  // The nave's tall windows down both sides.
  for (int i = 0; i < 4; ++i) {
    const float z = -hd + 2.5f + (nave_front - (-hd) - 5.0f) * static_cast<float>(i) / 3;
    box(m, hw, 4.2f, z, 0.05f, 1.9f, 0.6f, kPane);
    box(m, -hw, 4.2f, z, 0.05f, 1.9f, 0.6f, kPane);
  }
  // The tower to the belfry, its openings dark, then the spire.
  const float ht = side / 2;
  const float belfry = spire * 0.66f;
  block(m, -ht, 0, nave_front, ht, belfry, hd, kRender);
  for (const float x : {-1.0f, 1.0f}) {
    box(m, x * ht, belfry - 2.2f, nave_front + ht, 0.06f, 1.2f, 0.7f, kFrame);
    box(m, 0, belfry - 2.2f, nave_front + ht + x * ht, 0.7f, 1.2f, 0.06f, kFrame);
  }
  door(m, 0, hd, 1.8f, 3.2f);
  const float apex[3] = {0, spire, nave_front + ht};
  const float c[4][2] = {{-ht - 0.2f, nave_front - 0.2f},
                         {ht + 0.2f, nave_front - 0.2f},
                         {ht + 0.2f, hd + 0.2f},
                         {-ht - 0.2f, hd + 0.2f}};
  for (int k = 0; k < 4; ++k) {
    const float a[3] = {c[k][0], belfry, c[k][1]};
    const float b[3] = {c[(k + 1) % 4][0], belfry, c[(k + 1) % 4][1]};
    const float out[3] = {(a[0] + b[0]) / 2, 0.3f, (a[2] + b[2]) / 2 - apex[2]};
    face(m, a, b, apex, out, kSteel);
  }
}

// The far LOD: the walls in the kind's main part and the roof (the church its tower and spire).
void far_mesh(render::MeshData& m, BuildingKind kind, const vg::BuildingSize& s) {
  const auto hw = static_cast<float>(s.width) / 2;
  const auto hd = static_cast<float>(s.depth) / 2;
  const auto eaves = static_cast<float>(s.eaves);
  const auto ridge = static_cast<float>(s.ridge);
  uint8_t wall = kRender;
  if (kind == BuildingKind::house || kind == BuildingKind::school || kind == BuildingKind::ticket ||
      kind == BuildingKind::lodge) {
    wall = kBoards;
  } else if (kind == BuildingKind::pump_house) {
    wall = kStone;
  } else if (kind == BuildingKind::garage) {
    wall = kSteel;
  }
  if (kind == BuildingKind::pump_house) {
    block(m, -hw, 0, -hd, hw, ridge, hd, wall);
    block(m, -hw - 0.25f, ridge, -hd - 0.25f, hw + 0.25f, ridge + 0.5f, hd + 0.25f, kSnow);
    return;
  }
  if (kind == BuildingKind::church) {
    const auto side = static_cast<float>(vg::buildings::kTowerSide);
    const float nave_front = hd - side;
    block(m, -hw, 0, -hd, hw, eaves, nave_front, kRender);
    block(m, -side / 2, 0, nave_front, side / 2, static_cast<float>(vg::buildings::kSpire) * 0.66f,
          hd, kRender);
    // The nave's roof as a box under the snow, the spire as a slim one.
    block(m, -hw, eaves, -hd, hw, (eaves + ridge) / 2, nave_front, kSnow);
    block(m, -0.8f, static_cast<float>(vg::buildings::kSpire) * 0.66f, nave_front + side / 2 - 0.8f,
          0.8f, static_cast<float>(vg::buildings::kSpire), nave_front + side / 2 + 0.8f, kSteel);
    return;
  }
  block(m, -hw, 0, -hd, hw, eaves, hd, wall);
  const bool along_z =
      kind == BuildingKind::house || kind == BuildingKind::hotel || kind == BuildingKind::shop;
  // Two roof planes and their gable ends, without the soffit.
  const float ha = along_z ? hw : hd;
  const float hl = along_z ? hd : hw;
  const auto at = [along_z](float a, float y, float l, float* p) {
    p[0] = along_z ? a : l;
    p[1] = y;
    p[2] = along_z ? l : a;
  };
  for (const float sd : {-1.0f, 1.0f}) {
    float r0[3];
    float r1[3];
    float e0[3];
    float e1[3];
    float up[3];
    at(0, ridge, -hl - kEaves, r0);
    at(0, ridge, hl + kEaves, r1);
    at(sd * (ha + kEaves), eaves, hl + kEaves, e1);
    at(sd * (ha + kEaves), eaves, -hl - kEaves, e0);
    at(sd * (ridge - eaves), ha, 0, up);
    quad(m, r0, r1, e1, e0, up, kSnow);
    float a[3];
    float b[3];
    float c[3];
    float out[3];
    at(-ha, eaves, sd * hl, a);
    at(ha, eaves, sd * hl, b);
    at(0, ridge, sd * hl, c);
    at(0, 0, sd, out);
    face(m, a, b, c, out, wall);
  }
}

uint32_t mix(uint32_t h) {
  h ^= h >> 16;
  h *= 0x7feb352du;
  h ^= h >> 15;
  h *= 0x846ca68bu;
  h ^= h >> 16;
  return h;
}

}  // namespace

render::MeshData building_mesh(BuildingKind kind, int lod) {
  render::MeshData m;
  const vg::BuildingSize s = vg::building_size(kind);
  if (lod > 0) {
    far_mesh(m, kind, s);
    return m;
  }
  switch (kind) {
    case BuildingKind::lodge:
      lodge(m, s);
      break;
    case BuildingKind::ticket:
      hut(m, s, kBoards, true);
      break;
    case BuildingKind::rental:
      shop(m, s, false);
      break;
    case BuildingKind::school:
      school(m, s);
      break;
    case BuildingKind::first_aid:
      first_aid(m, s);
      break;
    case BuildingKind::hotel:
      hotel(m, s);
      break;
    case BuildingKind::garage:
      garage(m, s);
      break;
    case BuildingKind::pump_house:
      pump_house(m, s);
      break;
    case BuildingKind::house:
      house(m, s);
      break;
    case BuildingKind::flats:
      flats(m, s);
      break;
    case BuildingKind::shop:
      shop(m, s, true);
      break;
    case BuildingKind::church:
      church(m, s);
      break;
  }
  return m;
}

std::vector<render::MeshSource> building_meshes() {
  std::vector<render::MeshSource> out;
  out.reserve(vg::kBuildingKinds + 1);
  for (int k = 0; k < vg::kBuildingKinds; ++k) {
    const auto kind = static_cast<BuildingKind>(k);
    render::MeshSource s;
    s.lods.push_back({building_mesh(kind, 0), kBuildingLodPixels});
    s.lods.push_back({building_mesh(kind, 1), 0.0f});
    out.push_back(std::move(s));
  }
  render::MeshSource plinth;
  render::MeshData m;
  block(m, -0.5f, -1.0f, -0.5f, 0.5f, 0.0f, 0.5f, kStone);
  plinth.lods.push_back({std::move(m), 0.0f});
  out.push_back(std::move(plinth));
  return out;
}

std::vector<render::Surface> building_surfaces() {
  std::vector<render::Surface> out;
  out.reserve(static_cast<size_t>(kLitSets) * 2);
  for (const bool lit : {false, true}) {
    for (const Look& look : kLooks) {
      for (int part = 0; part < kBuildingParts; ++part) {
        render::Surface s = render::matte_surface(look[static_cast<size_t>(part)].data());
        if (part == kPane) {
          // Glass: a glint of the sky by day; lit, a warm room behind it.
          s.rough = 0.08f;
          s.spec = 0.5f;
          if (lit) {
            const Rgb room = kLitRoomRgb;
            std::copy(room.begin(), room.end(), s.albedo);
            s.rough = 0.3f;
            s.emissive_nits = kWindowNits;
          }
        }
        out.push_back(s);
      }
    }
  }
  return out;
}

uint32_t building_material(const vg::Building& b) {
  const uint32_t pick = mix(static_cast<uint32_t>(b.id) * 0x9e3779b9u + 0x2545u);
  uint32_t look = 0;
  switch (b.kind) {
    case BuildingKind::garage:
    case BuildingKind::pump_house:
      look = kGarageLook;
      break;
    case BuildingKind::church:
      look = kChurchLook;
      break;
    case BuildingKind::lodge:
      look = 1;
      break;
    case BuildingKind::hotel: {
      constexpr uint32_t kHotelLooks[] = {0, 1, 3};
      look = kHotelLooks[pick % 3];
      break;
    }
    default:
      look = pick % 5;
      break;
  }
  return look * kBuildingParts;
}

double lights_share(BuildingKind kind, double hour) {
  const std::span<const Lights> day = schedule(kind);
  const double h = hour - 24.0 * std::floor(hour / 24.0);
  for (size_t i = 1; i < day.size(); ++i) {
    if (h <= day[i].hour) {
      const Lights& a = day[i - 1];
      const Lights& b = day[i];
      return a.share + (b.share - a.share) * (h - a.hour) / (b.hour - a.hour);
    }
  }
  return day.back().share;
}

bool lights_on(const vg::Building& b, double hour, double sun_elevation) {
  if (sun_elevation > kLightsOnElevation) return false;
  // Each building's own draw, fixed, so the village goes dark one building at a time.
  const uint32_t draw = mix(static_cast<uint32_t>(b.id) * 0x85ebca6bu + 0x7f4au) & 0xffffu;
  return static_cast<double>(draw) / 65536.0 < lights_share(b.kind, hour);
}

void building_instances(const vg::Building& b, std::vector<render::Instance>& out, bool lit) {
  render::Instance in;
  in.mesh = static_cast<uint32_t>(b.kind);
  in.material = building_material(b) + (lit ? kLitSets : 0);
  in.position[0] = b.x;
  in.position[1] = b.y;
  in.position[2] = b.z;
  const double half = b.heading * 0.5;
  in.rotation[1] = static_cast<float>(std::sin(half));
  in.rotation[3] = static_cast<float>(std::cos(half));
  out.push_back(in);
  render::Instance plinth = in;
  plinth.mesh = kPlinthMesh;
  plinth.scale[0] = static_cast<float>(b.width + 0.3);
  plinth.scale[1] = static_cast<float>(b.y - b.low + kPlinthSink);
  plinth.scale[2] = static_cast<float>(b.depth + 0.3);
  out.push_back(plinth);
}

}  // namespace ge::mountain::app
