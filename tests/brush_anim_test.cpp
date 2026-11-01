// Texture sequences (plugins/brush/anim.hpp): a '+' cycle turns at five frames a second, its
// alternate cycle when asked, every surface together; a '-' set gives each surface one frame for
// good; names match in any case; a gap keeps the frames below it and is noted; a texture of no
// set, and every texture of a family that animates otherwise, is its own frame; a family that
// chains its frames turns each closed chain, and an open one stands still and is noted.
//
// ge:summary Test: + cycles and alternates by time, - random sets by surface, gaps noted
// ge:keywords test texture animated cycle alternate random frame gap chain next
// ge:skill brush testing
#include <string>
#include <vector>

#include "brush/anim.hpp"
#include "check.hpp"

namespace br = ge::brush;

namespace {

br::Map named(const std::vector<std::string>& names, br::Family family = br::Family::v29) {
  br::Map m;
  m.family = family;
  for (const std::string& n : names) m.textures.push_back({n, 64, 64, 0, {}});
  return m;
}

}  // namespace

TEST(a_cycle_turns_with_the_clock_and_its_alternate_when_asked) {
  // 0 wall, 1 +0lava, 2 +2LAVA, 3 +1lava, 4 +alava, 5 +blava
  const br::Map m = named({"wall", "+0lava", "+2LAVA", "+1lava", "+alava", "+Blava"});
  const br::TextureAnims a = br::build_texture_anims(m);
  CHECK(a.sets.size() == 1 && a.notes.empty());
  CHECK(a.sets[0].frames == (std::vector<uint32_t>{1, 3, 2}));
  CHECK(a.sets[0].alt == (std::vector<uint32_t>{4, 5}));
  CHECK(br::texture_at(a, 0, 3.0, false, 0) == 0);
  CHECK(br::texture_at(a, 1, 0.0, false, 0) == 1);
  CHECK(br::texture_at(a, 1, 0.2, false, 7) == 3);  // every surface together
  CHECK(br::texture_at(a, 2, 0.4, false, 9) == 2);  // any frame asks for the whole cycle
  CHECK(br::texture_at(a, 1, 0.6, false, 0) == 1);  // wraps after three
  CHECK(br::texture_at(a, 1, 0.2, true, 0) == 5);
  CHECK(br::texture_at(a, 4, 0.0, false, 0) == 1);   // an alternate frame asks for the main cycle
  CHECK(br::texture_at(a, 1, -0.1, false, 0) == 2);  // floor before zero
}

TEST(a_random_set_gives_each_surface_one_frame_for_good) {
  const br::Map m = named({"-0tile", "-1tile", "-2tile"});
  const br::TextureAnims a = br::build_texture_anims(m);
  CHECK(a.sets.size() == 1 && a.sets[0].random);
  for (uint32_t s = 0; s < 6; ++s) {
    CHECK(br::texture_at(a, 0, 0.0, false, s) == s % 3);
    CHECK(br::texture_at(a, 0, 9.9, true, s) == s % 3);
  }
}

TEST(a_gap_keeps_the_frames_below_it_and_is_noted) {
  const br::Map m = named({"+0slime", "+1slime", "+3slime", "+aonly"});
  const br::TextureAnims a = br::build_texture_anims(m);
  CHECK(a.sets.size() == 2);
  CHECK(a.notes.size() == 2);
  CHECK(a.set_of[2] == -1);  // past the gap: drawn as itself
  CHECK(br::texture_at(a, 2, 0.4, false, 0) == 2);
  CHECK(br::texture_at(a, 0, 0.4, false, 0) == 0);
}

TEST(a_family_that_animates_otherwise_has_no_sets) {
  const br::Map m = named({"+0lava", "+1lava"}, br::Family::v38);
  const br::TextureAnims a = br::build_texture_anims(m);
  CHECK(a.sets.empty());
  CHECK(br::texture_at(a, 0, 0.2, false, 0) == 0);
}

TEST(a_family_that_chains_its_frames_turns_each_closed_chain) {
  // 0 wall, 1 a, 2 b, 3 c (a -> b -> c -> a), 4 d -> 5 e -> nothing (open), 6 f -> 6 itself
  br::Map m = named({"wall", "a", "b", "c", "d", "e", "f"}, br::Family::v38);
  m.textures[1].next = 2, m.textures[2].next = 3, m.textures[3].next = 1;
  m.textures[4].next = 5, m.textures[6].next = 6;
  const br::TextureAnims a = br::build_texture_anims(m);
  CHECK(a.sets.size() == 1);
  CHECK(a.sets.size() == 1 && a.sets[0].frames == std::vector<uint32_t>({1, 2, 3}));
  CHECK(a.set_of[2] == 0 && a.set_of[4] == -1 && a.set_of[6] == -1);
  CHECK(a.notes.size() == 1);  // d's chain does not close
  CHECK(br::texture_at(a, 2, 0.0, false, 0) == 1);
  CHECK(br::texture_at(a, 3, 0.45, false, 0) == 3);  // frame 2 at five a second
  CHECK(br::texture_at(a, 4, 1.0, false, 0) == 4);
}
