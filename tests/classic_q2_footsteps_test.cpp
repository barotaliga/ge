// Tests: Quake II's footsteps by surface (plugins/classic/q2_footsteps.hpp) and its loops merged
// (q2_loop_merge.hpp). The step sets of a level read from its textures' `.mat` files (default,
// ladder, a material's own, a bad name refused, case ignored, a set numbered to its first gap and
// at most 15); the surface under a box found by the sweep, shallow water's over the floor under
// it; a variant never the one heard last; the packed box unpacked; a loop's ear shares, the merge's
// gain and pan, and when `al_merge_looping` merges. And the music's file by name
// (classic/content/soundtrack.hpp's `music_file`): extension and case ignored, FLAC only.
//
// ge:summary Test: Quake II's footsteps by surface, its loops merged, the music's file by name
// ge:keywords test cl_footsteps step-sets material mat-file al_merge_looping loop-merge
// ge:keywords music_file ogg-play music-by-name
// ge:skill classic testing
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/trace.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/content/soundtrack.hpp"
#include "classic/q2_footsteps.hpp"
#include "classic/q2_loop_merge.hpp"

namespace {

namespace br = ge::brush;
namespace cl = ge::classic;

constexpr int32_t kSolid = 1, kWater = 0x20;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1, int32_t contents,
                   const std::string& tex) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, contents, tex);
}

// A metal floor, a grass floor beside it with a shallow pool over its far half, and a ladder.
br::Map level() {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-256, -256, -16, 0, 256, 0, kSolid, "e1u1/METAL1"));
  m.push_back(box(0, -256, -16, 256, 256, 0, kSolid, "e1u1/grass"));
  m.push_back(box(128, -256, 0, 256, 256, 8, kWater, "e1u1/water"));
  m.push_back(box(-300, -256, -16, -256, 256, 200, kSolid, "e1u1/rungs"));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  br::Map map;
  CHECK_MSG(br::read_bsp(bytes, &map, &error), error);
  return map;
}

// The files the level's game has: the `.mat` texts and the step sounds there.
struct Files {
  std::map<std::string, std::string> text;
  std::set<std::string> sounds;
};

cl::Q2StepSets sets_of(const br::Map& map, const Files& f, std::vector<std::string>* said) {
  return cl::q2_step_sets(
      map,
      [&f](const std::string& path) -> std::optional<std::string> {
        const auto it = f.text.find(path);
        if (it == f.text.end()) return std::nullopt;
        return it->second;
      },
      [&f](const std::string& name) {  // the packs' names match in any case
        std::string low = name;
        for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return f.sounds.count(low) != 0;
      },
      said);
}

Files files() {
  Files f;
  f.text["textures/e1u1/METAL1.mat"] = "Metal\n";
  f.text["textures/e1u1/grass.mat"] = "grass";
  f.text["textures/e1u1/water.mat"] = "water";
  f.text["textures/e1u1/rungs.mat"] = "LADDER";
  for (int k = 1; k <= 4; k++) f.sounds.insert("player/step" + std::to_string(k) + ".wav");
  for (int k = 1; k <= 20; k++) f.sounds.insert("player/steps/metal" + std::to_string(k) + ".wav");
  for (const int k : {1, 2, 4}) f.sounds.insert("player/steps/grass" + std::to_string(k) + ".wav");
  for (int k = 1; k <= 3; k++) f.sounds.insert("player/steps/water" + std::to_string(k) + ".wav");
  for (int k = 1; k <= 2; k++) f.sounds.insert("player/steps/ladder" + std::to_string(k) + ".wav");
  return f;
}

int texture_named(const br::Map& m, const std::string& name) {
  for (size_t t = 0; t < m.textures.size(); t++)
    if (m.textures[t].name == name) return static_cast<int>(t);
  return -1;
}

const std::string& material_under(const cl::Q2StepSets& s, br::BrushTracer& t, float x) {
  const float at[3] = {x, 0, 24}, lo[3] = {-16, -16, -24}, hi[3] = {16, 16, 32};
  return s.materials[size_t(cl::q2_step_set(s, cl::q2_step_surface(t, at, lo, hi)))];
}

TEST(a_levels_step_sets_come_from_its_textures_mat_files) {
  const br::Map map = level();
  std::vector<std::string> said;
  const cl::Q2StepSets s = sets_of(map, files(), &said);
  CHECK(said.empty());
  CHECK(s.any_material);
  CHECK(s.sets[cl::kQ2StepDefault].size() == 4);
  CHECK(s.sets[cl::kQ2StepLadder].size() == 2);
  const int metal = cl::q2_step_set(s, texture_named(map, "e1u1/METAL1"));
  CHECK(s.materials[size_t(metal)] == "Metal");
  CHECK(s.sets[size_t(metal)].size() == 15);                     // at most 15
  CHECK(s.sets[size_t(metal)][0] == "player/steps/Metal1.wav");  // the name as written
  const int grass = cl::q2_step_set(s, texture_named(map, "e1u1/grass"));
  CHECK(s.sets[size_t(grass)].size() == 2);  // grass3 missing: 1 and 2 only
  CHECK(cl::q2_step_set(s, texture_named(map, "e1u1/rungs")) == cl::kQ2StepLadder);
  CHECK(cl::q2_step_set(s, -1) == cl::kQ2StepDefault);
}

TEST(a_bad_material_is_said_and_the_texture_steps_as_the_default) {
  const br::Map map = level();
  Files f = files();
  f.text["textures/e1u1/grass.mat"] = "gr ass!";
  std::vector<std::string> said;
  const cl::Q2StepSets s = sets_of(map, f, &said);
  CHECK(said.size() == 1);
  if (!said.empty()) CHECK(said[0] == "Bad material \"gr ass!\" in textures/e1u1/grass.mat");
  CHECK(cl::q2_step_set(s, texture_named(map, "e1u1/grass")) == cl::kQ2StepDefault);
}

TEST(a_material_with_no_sounds_plays_the_default_and_no_mat_means_no_search) {
  const br::Map map = level();
  Files f = files();
  f.text["textures/e1u1/grass.mat"] = "mud";
  const cl::Q2StepSets s = sets_of(map, f, nullptr);
  const int mud = cl::q2_step_set(s, texture_named(map, "e1u1/grass"));
  CHECK(s.sets[size_t(mud)].empty());
  const std::string* one = cl::q2_step_pick(s, mud, 0, "");
  CHECK(one != nullptr && one->starts_with("player/step"));
  Files plain;
  plain.sounds = f.sounds;
  CHECK(!sets_of(map, plain, nullptr).any_material);
}

TEST(the_surface_underfoot_is_found_shallow_water_over_its_floor) {
  const br::Map map = level();
  const cl::Q2StepSets s = sets_of(map, files(), nullptr);
  br::BrushTracer t(map);
  CHECK(material_under(s, t, -100) == "Metal");
  CHECK(material_under(s, t, 64) == "grass");
  CHECK(material_under(s, t, 200) == "water");
  // Nothing under: the default.
  const float air[3] = {0, 0, 5000};
  CHECK(cl::q2_step_surface(t, air, nullptr, nullptr) == -1);
  // With no box the sweep reaches 75 units down: a floor 70 under is found, 80 under is not.
  const float near[3] = {-100, 0, 70}, far[3] = {-100, 0, 80};
  CHECK(cl::q2_step_surface(t, near, nullptr, nullptr) >= 0);
  CHECK(cl::q2_step_surface(t, far, nullptr, nullptr) == -1);
}

TEST(a_step_is_never_the_one_heard_last) {
  const br::Map map = level();
  const cl::Q2StepSets s = sets_of(map, files(), nullptr);
  std::string last;
  for (uint32_t roll = 0; roll < 200; roll++) {
    const std::string* n = cl::q2_step_pick(s, cl::kQ2StepDefault, roll * 7 / 3, last);
    CHECK(n != nullptr && *n != last);
    if (n != nullptr) last = *n;
  }
}

TEST(a_packed_box_unpacks_to_its_mins_and_maxs) {
  float lo[3], hi[3];
  // A player's 32 by 32 by 56 box: half width 16 (2 eighths of 8), depth 24 (3), height 32 (8).
  CHECK(cl::q2_unpack_solid(2 | (3 << 5) | (8 << 10), lo, hi));
  CHECK(lo[0] == -16 && lo[2] == -24 && hi[1] == 16 && hi[2] == 32);
  CHECK(!cl::q2_unpack_solid(0, lo, hi));
  CHECK(!cl::q2_unpack_solid(31, lo, hi));
}

TEST(copies_of_one_loop_merge_capped_by_ear_and_panned_by_their_balance) {
  CHECK(cl::q2_loop_multiplier(false, 0) == 0.003);
  CHECK(cl::q2_loop_multiplier(true, 3) == 0.003);
  CHECK(std::fabs(cl::q2_loop_multiplier(true, 1) - 0.0006) < 1e-12);
  CHECK(cl::q2_loop_multiplier(true, 0) == 0);
  // Within 80 units whole; past them down by the multiplier.
  const cl::Q2Ears near = cl::q2_loop_ear(1, 50, 1, 0.003, false);
  CHECK(near.right == 1 && near.left == 0);
  const cl::Q2Ears mid = cl::q2_loop_ear(1, 180, 0, 0.003, false);
  CHECK(std::fabs(mid.left - 0.35) < 1e-9 && std::fabs(mid.right - 0.35) < 1e-9);
  // Ten copies on the right: each ear's sum held to 1, the voice whole and full right.
  cl::Q2Ears sum;
  for (int k = 0; k < 10; k++) {
    const cl::Q2Ears e = cl::q2_loop_ear(1, 40, 1, 0.003, false);
    sum.left += e.left, sum.right += e.right;
  }
  const cl::Q2Merged m = cl::q2_merge(sum);
  CHECK(m.gain == 1 && m.pan == 1);
  // One centred and quiet: its gain the two shares, no pan.
  const cl::Q2Merged c = cl::q2_merge(cl::q2_loop_ear(0.2, 0, 0, 0.003, true));
  CHECK(std::fabs(c.gain - 0.4) < 1e-9 && c.pan == 0);
  CHECK(cl::q2_merge({}).gain == 0);
  CHECK(cl::q2_merges(1, true) && !cl::q2_merges(1, false) && cl::q2_merges(2, false));
  CHECK(!cl::q2_merges(0, true));
}

TEST(quakes_static_loops_of_one_file_merge_into_one_voice_held_per_ear) {
  // Attenuation 3 at 1000 units / 3: silent; at 0 units whole, centred.
  const cl::Q2Ears gone = cl::classic_static_ear(1, 1000.0 / 3, 0, 3);
  CHECK(gone.left == 0 && gone.right == 0);
  const cl::Q2Ears here = cl::classic_static_ear(1, 0, 0, 3);
  CHECK(here.left == 1 && here.right == 1);
  // A full centred source is the most a voice gives: gain 1, no pan.
  const cl::Q2Merged whole = cl::classic_static_merge(here);
  CHECK(whole.gain == 1 && whole.pan == 0);
  // Four torches hard right at full volume: the right ear held to 1, so half the gain, full right.
  cl::Q2Ears sum;
  for (int k = 0; k < 4; k++) {
    const cl::Q2Ears e = cl::classic_static_ear(1, 0, 1, 3);
    sum.left += e.left, sum.right += e.right;
  }
  const cl::Q2Merged right = cl::classic_static_merge(sum);
  CHECK(right.gain == 0.5 && right.pan == 1);
  // Two quiet ones left and right: one centred voice, their sum.
  const cl::Q2Ears a = cl::classic_static_ear(0.25, 0, -1, 3), b = cl::classic_static_ear(0.25, 0, 1, 3);
  const cl::Q2Merged both = cl::classic_static_merge({a.left + b.left, a.right + b.right});
  CHECK(std::fabs(both.gain - 0.5) < 1e-12 && both.pan == 0);
  CHECK(cl::classic_static_merge({}).gain == 0);
}

TEST(the_musics_file_by_name_ignores_case_and_extension_flac_only) {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "ge_classic_q2_footsteps_music";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir / "music" / "Quake2");
  std::filesystem::create_directories(dir / "music" / ".hidden");
  for (const char* f : {"music/track02.flac", "music/Quake2/Track05.FLAC", "music/theme.ogg",
                        "music/.hidden/secret.flac"})
    std::ofstream(dir / f) << "x";
  const std::string root = dir.string();
  CHECK(ge::classic::content::music_file(root, "track02").ends_with("track02.flac"));
  CHECK(ge::classic::content::music_file(root, "TRACK02.ogg").ends_with("track02.flac"));
  CHECK(ge::classic::content::music_file(root, "quake2/track05").ends_with("Track05.FLAC"));
  CHECK(ge::classic::content::music_file(root, "quake2\\track05.wav").ends_with("Track05.FLAC"));
  CHECK(ge::classic::content::music_file(root, "theme").empty());  // not FLAC: not played
  CHECK(ge::classic::content::music_file(root, ".hidden/secret").empty());
  CHECK(ge::classic::content::music_file(root, "../music/track02").empty());
  CHECK(ge::classic::content::music_file(root, "").empty());
  std::filesystem::remove_all(dir);
}

}  // namespace
