// The map lab helpers: the build under the lab's flags.
//
// ge:summary The map labs' build-from-flags: the region, the versions, the map and its inputs
// ge:keywords lab level map build-map region version
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include "mountain/labs/level_map.hpp"

#include <utility>

#include "mountain/mapgen/generate.hpp"

namespace ge::mountain::labs {

std::optional<mapgen::RegionId> region_named(const std::string& name) {
  if (name == "alpine") return mapgen::RegionId::alpine;
  if (name == "fell") return mapgen::RegionId::fell;
  if (name == "continental") return mapgen::RegionId::continental;
  if (name == "maritime") return mapgen::RegionId::maritime;
  return std::nullopt;
}

std::vector<int> map_versions(const ge::lab::Lab& lab) {
  std::vector<int> out;
  for (const double v : lab.arg_nums("version")) out.push_back(static_cast<int>(v));
  return out;
}

std::string map_id(double seed, int version) {
  const std::string id = "seed-" + std::to_string(static_cast<long long>(seed));
  return version == 1 ? id : id + "-v" + std::to_string(version);
}

std::optional<mapgen::Level> build_map(ge::lab::Lab& lab, double seed, std::optional<int> version,
                                       std::optional<mapgen::PisteGrade> grade,
                                       std::optional<std::string> course, bool tricks) {
  mapgen::GenerateOptions opts;
  opts.tricks = tricks;
  const int v = version ? *version : map_versions(lab).front();
  opts.version = v;
  if (v != 1) {
    opts.grade = grade;
    opts.course = std::move(course);
  }
  opts.region = region_named(lab.arg("region"));
  if (!opts.region) {
    lab.fail("--region=" + lab.arg("region") + " is not a region",
             "next: rerun with alpine, fell, continental or maritime (mapgen/regions.hpp).");
    return std::nullopt;
  }
  try {
    return mapgen::generate_level(seed, opts);
  } catch (const mapgen::GenerateError& e) {
    lab.fail("seed " + std::to_string(static_cast<long long>(seed)) + " did not build: " + e.what(),
             "Run `make lab LAB=analyze ARGS=\"--seeds=" +
                 std::to_string(static_cast<long long>(seed)) + " --version=" + std::to_string(v) +
                 " --refusals\"` to see each attempt's refusal. Skill: mountain.");
    return std::nullopt;
  }
}

std::vector<std::pair<std::string, std::string>> map_inputs(const ge::lab::Lab& lab,
                                                            const mapgen::Level& level) {
  return {{"seed", std::to_string(static_cast<long long>(level.seed))},
          {"version", std::to_string(level.version)},
          {"region", lab.arg("region")},
          {"attempt", std::to_string(level.attempt)}};
}

}  // namespace ge::mountain::labs
