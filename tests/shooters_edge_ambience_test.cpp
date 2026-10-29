// Shooter's Edge's ambience (games/shooters_edge/sound/ambience.hpp): the targets are the
// listener's leaf's levels over 255 times the volume, a solid leaf and a family that stores no
// levels are silence, and walking from an open hall into a room by water fades the sky out and
// the water in at the bed's rate rather than at once.
//
// ge:summary Tests for Shooter's Edge's ambience: the leaf's levels as targets, a walk
// ge:keywords test ambience ambient leaf water sky level volume walk fixture
// ge:skill audio testing
#include <string>
#include <vector>

#include "audio/synth.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/sound/ambience.hpp"

namespace {

namespace br = ge::brush;
using namespace ge::audio;

// The fixture halls with the hall under open sky and the side room by water.
br::Map halls(br::Family f) {
  br::Fixture fx = br::sample_fixture();
  fx.rooms[0].ambient[br::kAmbientSky] = 255;
  fx.rooms[2].ambient[br::kAmbientWater] = 170;
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(fx, f, &bytes, &error), error);
  br::Map m;
  CHECK_MSG(br::read_bsp(bytes, &m, &error), error);
  return m;
}

Pcm hum(float v) {
  Pcm p;
  p.rate = kRate;
  p.samples.assign(static_cast<size_t>(kRate / 4), v);
  return p;
}

constexpr float kHall[3] = {256, 192, 128}, kSide[3] = {704, 256, 96}, kWall[3] = {600, 20, 20};

}  // namespace

TEST(the_targets_are_the_leaf_s_levels_times_the_volume) {
  const br::Map m = halls(br::Family::v29);
  float out[4];
  shooters_edge::leaf_ambience(m, kHall, 0.5f, out);
  CHECK(out[br::kAmbientSky] == 0.5f && out[br::kAmbientWater] == 0);
  shooters_edge::leaf_ambience(m, kSide, 1.0f, out);
  CHECK_MSG(out[br::kAmbientWater] > 0.666f && out[br::kAmbientWater] < 0.667f,
            std::to_string(out[br::kAmbientWater]));
  CHECK(out[br::kAmbientSky] == 0);
  shooters_edge::leaf_ambience(m, kWall, 1.0f, out);  // inside a wall: silence
  for (float v : out) CHECK(v == 0);
}

TEST(a_family_without_levels_and_an_empty_map_are_silence) {
  float out[4];
  for (br::Family f : {br::Family::v38, br::Family::v46}) {
    shooters_edge::leaf_ambience(halls(f), kHall, 1.0f, out);
    for (float v : out) CHECK(v == 0);
  }
  shooters_edge::leaf_ambience(br::Map{}, kHall, 1.0f, out);
  for (float v : out) CHECK(v == 0);
}

TEST(walking_into_the_side_room_crossfades_the_sky_to_the_water) {
  const br::Map m = halls(br::Family::bsp2);
  const Pcm water = hum(0.1f), sky = hum(0.1f);
  Synth s;
  shooters_edge::Ambience amb(water, sky);
  const float dt = 0.1f;
  std::vector<float> out(2 * static_cast<size_t>(kRate * dt));
  for (int i = 0; i < 30; i++) {
    amb.step(s, m, kHall, dt, 1.0f);
    s.render(out.data(), static_cast<int>(out.size() / 2));
  }
  const auto& l = amb.bed().layers();
  CHECK(l[1].level == 1.0f && l[0].level == 0);  // the sky up, the water silent
  CHECK(s.stats().samples == 1);
  amb.step(s, m, kSide, dt, 1.0f);
  // One step in: each level moved by the rate's step and no more.
  CHECK_MSG(l[1].level > 0.95f && l[1].level < 0.97f, std::to_string(l[1].level));
  CHECK_MSG(l[0].level > 0.03f && l[0].level < 0.05f, std::to_string(l[0].level));
  for (int i = 0; i < 30; i++) {
    amb.step(s, m, kSide, dt, 1.0f);
    s.render(out.data(), static_cast<int>(out.size() / 2));
  }
  CHECK(l[1].level == 0 && !l[1].voice.valid());
  CHECK_MSG(l[0].level > 0.666f && l[0].level < 0.667f, std::to_string(l[0].level));
  CHECK(s.stats().samples == 1);
  amb.silence(s);
  for (int i = 0; i < 5; i++) s.render(out.data(), static_cast<int>(out.size() / 2));
  CHECK(s.stats().samples == 0);
}
