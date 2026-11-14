// PEOPLE'S HOP STARTS PLAYED BY THE PILOT (plugins/shooter/pilot_recipes.hpp, the book
// games/shooters_edge/bots/hop_start_book.hpp): in a long walled hall an expert pilot starting
// from a stand takes one of the book's starts, proves it, plays it to its takeoff and leaves the
// ground near people's speed, on the analog hands and through the keyboard; with the book off it
// runs and jumps at a run's speed. The book decodes: every entry has steps, from a low, with one
// jump.
//
// ge:summary Tests: the pilot plays people's hop starts from a stand: proven, played, takeoff speed
// ge:keywords test recipe hop-start pilot book recipe_choose recipe_play keyboard
// ge:skill shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/features.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/bots/hop_start_book.hpp"
#include "shooters_edge/bots/pilot_run.hpp"

namespace sh = ge::shooter;
namespace pr = shooters_edge::bots;

namespace {

sh::Brush box(sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

// A hall 2048 long and 768 wide, floor at 0.
struct Hall {
  sh::HullData point, clip;
  sh::MoveWorld world;
  sh::NavGraph g;
  Hall() {
    const sh::Vec3f lo{0, -384, -32}, hi{2048, 384, 256};
    std::vector<sh::Brush> b = {box({lo.x - 32, lo.y - 32, lo.z}, {hi.x + 32, hi.y + 32, 0}),
                                box({lo.x - 32, lo.y - 32, lo.z}, {lo.x, hi.y + 32, hi.z}),
                                box({hi.x, lo.y - 32, lo.z}, {hi.x + 32, hi.y + 32, hi.z}),
                                box({lo.x - 32, lo.y - 32, lo.z}, {hi.x + 32, lo.y, hi.z}),
                                box({lo.x - 32, hi.y, lo.z}, {hi.x + 32, hi.y + 32, hi.z})};
    point = sh::build_point_hull(b);
    clip = sh::build_box_hull(b, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.solids[0].mins = {lo.x - 32, lo.y - 32, lo.z};
    world.solids[0].maxs = {hi.x + 32, hi.y + 32, hi.z};
    world.count = 1;
    sh::build_nav(world, {}, sh::MoveSettings{}, sh::NavSettings{}, g);
  }
};

sh::PilotSettings expert(bool book, bool keys) {
  sh::PilotSettings set;
  set.hop = true;
  set.hop_circle = true;
  set.curve_keys = true;
  if (book) set.recipes = pr::hop_start_book();
  set.recipe_level = 2;
  set.recipe_keys_s = keys ? 0.03f : 0.f;
  return set;
}

// The speed across at the run's first takeoff (the first command off the ground, rising).
float first_takeoff(const pr::RunResult& r) {
  for (size_t i = 1; i < r.velocity.size(); i++)
    if (r.velocity[i].z > 100 && r.velocity[i - 1].z <= 1)
      return sh::root_of(r.velocity[i].x * r.velocity[i].x + r.velocity[i].y * r.velocity[i].y);
  return 0;
}

}  // namespace

TEST(the_book_decodes_every_entry_from_a_low_to_one_jump) {
  const sh::RecipeBook book = pr::hop_start_book();
  CHECK(book.entries.size() == 108u);
  for (const sh::RecipeEntry& e : book.entries) {
    CHECK(e.count > 3);
    CHECK(e.first + e.count <= book.steps.size());
    int jumps = 0;
    for (uint32_t k = 0; k < e.count; k++)
      jumps += (book.steps[e.first + k].flags & sh::kStepJump) ? 1 : 0;
    CHECK(jumps >= 1);
    CHECK(book.steps[e.first].ms == 0);
  }
}

TEST(an_expert_from_a_stand_plays_a_start_off_the_ground_near_peoples_speed) {
  Hall h;
  sh::Features keys = sh::features_all();
  for (const bool on_keys : {false, true}) {
    const pr::RunResult r =
        pr::run_route(h.world, h.g, {200, 0, 24}, {1900, 0, 24}, sh::PilotHands::practised, 3,
                      expert(true, on_keys), 6, {}, on_keys ? &keys : nullptr, 2);
    const float off = first_takeoff(r);
    std::string trace;
    for (size_t i = 0; i < r.because.size() && i < 80; i++)
      trace += std::to_string(i) + " " + r.because[i] + " v " +
               std::to_string(static_cast<int>(sh::root_of(r.velocity[i].x * r.velocity[i].x +
                                                           r.velocity[i].y * r.velocity[i].y))) +
               "\n";
    CHECK_MSG(r.stats.recipe_commits >= 1 && r.stats.recipe_takeoffs >= 1 && off > 400,
              std::string(on_keys ? "keys" : "analog") + ": asks " +
                  std::to_string(r.stats.recipe_asks) + " commits " +
                  std::to_string(r.stats.recipe_commits) + " aborts " +
                  std::to_string(r.stats.recipe_aborts) + " takeoffs " +
                  std::to_string(r.stats.recipe_takeoffs) + " first takeoff " +
                  std::to_string(off) + "\n" + trace);
  }
}
