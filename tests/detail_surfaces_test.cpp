// engine/detail/surfaces.hpp: a texture's class by its name's words, matched in their order (a
// "mossy rock" is stone, a "lava rock" liquid), whatever the case; a name with none is unknown;
// every class has its name.
//
// ge:summary Test: a surface's class by its texture's name
// ge:keywords test detail surface-class texture-name classify_name
// ge:skill detail testing
#include <string>

#include "check.hpp"
#include "detail/surfaces.hpp"

namespace {

namespace gd = ge::detail;
using C = gd::SurfaceClass;

}  // namespace

TEST(a_textures_name_gives_its_class) {
  CHECK(gd::classify_name("grass_lawn") == C::grass);
  CHECK(gd::classify_name("GRASS1") == C::grass);
  CHECK(gd::classify_name("mossy_rock") == C::stone);
  CHECK(gd::classify_name("metal_floor") == C::metal);
  CHECK(gd::classify_name("wood_plank") == C::wood);
  CHECK(gd::classify_name("ground_02") == C::dirt);
  CHECK(gd::classify_name("sky4") == C::sky);
  CHECK(gd::classify_name("lava_rock") == C::liquid);
  CHECK(gd::classify_name("wall_09") == C::unknown);
}

TEST(every_class_has_a_name) {
  for (int k = 0; k < gd::kSurfaceClasses; ++k) {
    const std::string name(gd::surface_name(static_cast<C>(k)));
    CHECK_MSG(!name.empty() && name != "?", "class " + std::to_string(k) + " has no name");
  }
  CHECK(gd::surface_name(C::stone) == "stone");
}
