// Shooter's Edge's muzzle fire (muzzle.hpp).
//
// ge:summary Shooter's Edge's muzzle fire: the looks off the table, the shots stepped per frame
// ge:keywords shooters-edge muzzle fire_looks step_muzzle smoke_of shot_births spark_particle
// ge:skill render shooter
// ge:test tests/shooters_edge_muzzle_test.cpp
#include "shooters_edge/app/muzzle.hpp"

#include <algorithm>

namespace shooters_edge::effects {

bool fire_looks(std::span<const FireRow> rows, double scale, std::vector<gr::MuzzleLook>& out,
                std::string* error) {
  out.clear();
  for (size_t i = 0; i < rows.size(); ++i) {
    gr::MuzzleLook look = gr::default_muzzle();
    if (rows[i].preset.empty()) {
      look.length_m = look.width_m = 0;
      look.sparks = 0;
      look.smoke.smoke = 0;
    } else if (!gr::tuned_muzzle(rows[i].preset, rows[i].knobs, &look)) {
      if (error)
        *error = "weapon row " + std::to_string(i) + ": the fire \"" + std::string(rows[i].preset) +
                 "\" turned by \"" + std::string(rows[i].knobs) +
                 "\" names a preset or a knob render/muzzle.hpp does not know";
      return false;
    } else {
      gr::scale_muzzle(look, scale);
    }
    out.push_back(look);
  }
  return true;
}

bool fires(const gr::MuzzleLook& look) {
  return look.length_m > 0 || look.sparks > 0 || look.smoke.smoke > 0;
}

void step_muzzle(MuzzleShots& s, const gr::MuzzleLook* look, bool fired, const gr::MuzzlePose& pose,
                 double now_s, std::vector<gr::Particle>* births, std::vector<gr::Blast>* puffs,
                 std::vector<gr::MuzzleFire>* flames) {
  if (fired && look != nullptr && fires(*look)) {
    MuzzleShots::Live l;
    l.shot.look = look;
    l.shot.seed = s.seed++;
    l.shot.born_s = now_s;
    l.at_shot = pose;
    if (births) {
      std::vector<gr::Spark> sparks(gr::shot_births_max(*look));
      const size_t n = gr::shot_births(l.shot, pose, sparks);
      for (size_t i = 0; i < n; ++i) births->push_back(gr::spark_particle(sparks[i]));
    }
    s.live.push_back(l);
  }
  std::erase_if(s.live, [&](const MuzzleShots::Live& l) {
    const gr::MuzzleLook& k = gr::look_of(l.shot);
    const double age = now_s - l.shot.born_s;
    return age < -1e-6 || (age > k.life_s && (k.smoke.smoke <= 0 || age > k.smoke.smoke_s));
  });
  for (const MuzzleShots::Live& l : s.live) {
    const gr::MuzzleLook& k = gr::look_of(l.shot);
    const double age = now_s - l.shot.born_s;
    if (flames && age <= k.life_s && k.length_m > 0) flames->push_back({l.shot, pose});
    if (puffs && k.smoke.smoke > 0) puffs->push_back(gr::smoke_of(l.shot, l.at_shot));
  }
}

}  // namespace shooters_edge::effects
