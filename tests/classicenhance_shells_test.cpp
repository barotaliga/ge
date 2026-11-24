// plugins/classicenhance/shells.hpp: each case's model is closed round its axis, wound outward and
// coloured from the palette's plain colours only; its bounce is the debris pool's kind; its sound
// lands loud and ends silent, the same variant the same sound, a soft floor duller than a hard one,
// metal ringing longer; the sounds' names round-trip and a stranger's name is refused.
//
// ge:summary Test: the spent cases' model, bounce shapes, landing sounds and their names
// ge:keywords test classicenhance shells casing model sound name
// ge:skill classicenhance testing
#include <cmath>
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "classicenhance/shells.hpp"

namespace {

namespace ce = ge::classicenhance;
namespace gd = ge::detail;

// A palette whose every colour is a grey but a few: the glowing end bright brass, which a case
// must never take.
std::vector<uint8_t> test_palette() {
  std::vector<uint8_t> p(768);
  for (int i = 0; i < 256; ++i) {
    const auto g = static_cast<uint8_t>(i);
    p[size_t(i) * 3] = p[size_t(i) * 3 + 1] = p[size_t(i) * 3 + 2] = g;
  }
  const uint8_t red[3] = {150, 34, 28}, brass[3] = {176, 136, 62};
  for (int k = 0; k < 3; ++k) {
    p[size_t(40) * 3 + size_t(k)] = red[k];
    p[size_t(41) * 3 + size_t(k)] = brass[k];
    p[size_t(250) * 3 + size_t(k)] = brass[k];  // the glowing end's copy
  }
  return p;
}

// The share of a sound's energy in its last half.
double tail_share(const ge::audio::Pcm& p) {
  double all = 0, tail = 0;
  for (size_t i = 0; i < p.samples.size(); ++i) {
    const double e = double(p.samples[i]) * p.samples[i];
    all += e;
    if (i > p.samples.size() / 8) tail += e;
  }
  return all > 0 ? tail / all : 0;
}

// The share of a sound's energy in its high band (a first difference's against the sound's).
double bright(const ge::audio::Pcm& p) {
  double all = 0, high = 0;
  for (size_t i = 1; i < p.samples.size(); ++i) {
    all += double(p.samples[i]) * p.samples[i];
    const double d = double(p.samples[i]) - p.samples[i - 1];
    high += d * d;
  }
  return all > 0 ? high / all : 0;
}

}  // namespace

TEST(each_case_is_a_closed_tube_wound_outward_in_plain_colours) {
  const std::vector<uint8_t> palette = test_palette();
  for (int k = 0; k < ce::kCasings; ++k) {
    const auto c = static_cast<ce::Casing>(k);
    const ge::morph::Model m = ce::casing_model(c, palette);
    const ce::CasingSize z = ce::casing_size(c);
    CHECK(m.frames.size() == 1 && m.skins.size() == 1);
    const std::vector<float>& pos = m.frames[0].positions;
    CHECK(pos.size() / 3 == m.vertex_count() && m.indices.size() % 3 == 0);
    for (const uint32_t i : m.indices) CHECK(i < m.vertex_count());
    // Every triangle faces away from the axis (the sides) or along it (the caps).
    for (size_t t = 0; t < m.indices.size(); t += 3) {
      const float* a = &pos[size_t(m.indices[t]) * 3];
      const float* b = &pos[size_t(m.indices[t + 1]) * 3];
      const float* d = &pos[size_t(m.indices[t + 2]) * 3];
      const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
      const float e2[3] = {d[0] - a[0], d[1] - a[1], d[2] - a[2]};
      const float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                          e1[0] * e2[1] - e1[1] * e2[0]};
      const float mid[3] = {(a[0] + b[0] + d[0]) / 3, (a[1] + b[1] + d[1]) / 3,
                            (a[2] + b[2] + d[2]) / 3};
      // Outward: from the axis for a side, from the middle along it for a cap.
      const float out[3] = {std::fabs(std::fabs(mid[0]) - z.length_u / 2) < 1e-4f ? mid[0] : 0,
                            mid[1], mid[2]};
      CHECK(n[0] * out[0] + n[1] * out[1] + n[2] * out[2] > 0);
    }
    CHECK(std::fabs(m.frames[0].max[0] - z.length_u / 2) < 1e-6f);
    for (const uint8_t i : m.skins[0].indices) CHECK(i < 224);
    for (const float st : m.st) CHECK(st >= 0 && st <= 1);
  }
  // The hull is red past its brass head; a rifle's case brass.
  const ge::morph::Model hull = ce::casing_model(ce::Casing::hull, palette);
  const ge::morph::Model brass = ce::casing_model(ce::Casing::brass, palette);
  CHECK(hull.skins[0].indices[size_t(3) * 32 + 15] == 40);
  CHECK(brass.skins[0].indices[size_t(5) * 32 + 15] == 41);
}

TEST(each_case_bounces_as_its_kind) {
  const auto shapes = ce::casing_shapes();
  CHECK(shapes.size() == size_t(ce::kCasings));
  for (int k = 0; k < ce::kCasings; ++k)
    CHECK(std::fabs(shapes[size_t(k)].radius_m -
                    ce::casing_size(static_cast<ce::Casing>(k)).radius_u * 0.025) < 1e-9);
  CHECK(shapes[size_t(ce::Casing::hull)].restitution <
        shapes[size_t(ce::Casing::brass)].restitution);
}

TEST(a_case_lands_loud_rings_by_what_it_lands_on_and_ends_silent) {
  for (int k = 0; k < ce::kCasings; ++k) {
    const auto c = static_cast<ce::Casing>(k);
    const ge::audio::Pcm stone = ce::casing_sound(c, gd::SurfaceClass::stone, 1);
    CHECK(stone.rate == ce::kCasingRate && !stone.samples.empty());
    float peak = 0;
    for (const float s : stone.samples) peak = std::max(peak, std::fabs(s));
    CHECK(std::fabs(peak - 0.6f) < 1e-5f);
    CHECK(stone.samples.back() == 0);
    // The same variant is the same sound; another is not.
    CHECK(ce::casing_sound(c, gd::SurfaceClass::stone, 1).samples == stone.samples);
    CHECK(ce::casing_sound(c, gd::SurfaceClass::stone, 2).samples != stone.samples);
    // Dirt is duller than stone; brass on metal rings longer than on dirt.
    const ge::audio::Pcm dirt = ce::casing_sound(c, gd::SurfaceClass::dirt, 1);
    CHECK(bright(dirt) < bright(stone));
    const ge::audio::Pcm metal = ce::casing_sound(c, gd::SurfaceClass::metal, 1);
    CHECK(tail_share(metal) > tail_share(dirt));
  }
}

TEST(the_sounds_names_round_trip_and_a_strangers_is_refused) {
  for (int k = 0; k < ce::kCasings; ++k)
    for (int s = 0; s < gd::kSurfaceClasses; ++s)
      for (int v = 0; v < ce::kCasingVariants; ++v) {
        const std::string name =
            ce::casing_sound_name(static_cast<ce::Casing>(k), static_cast<gd::SurfaceClass>(s), v);
        ce::Casing c{};
        gd::SurfaceClass on{};
        int variant = -1;
        CHECK(ce::parse_casing_sound(name, &c, &on, &variant));
        CHECK(int(c) == k && int(on) == s && variant == v);
      }
  CHECK(ce::casing_sound_name(ce::Casing::brass, gd::SurfaceClass::metal, 6) ==
        "ce/shell/brass/metal/2");
  ce::Casing c{};
  gd::SurfaceClass on{};
  int v = 0;
  for (const char* bad :
       {"weapons/guncock.wav", "ce/shell/brass/metal", "ce/shell/tin/metal/1",
        "ce/shell/brass/glass/1", "ce/shell/brass/metal/9", "ce/shell/brass/metal/12"})
    CHECK(!ce::parse_casing_sound(bad, &c, &on, &v));
}
