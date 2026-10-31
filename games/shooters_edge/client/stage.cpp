// The staged packs (stage.hpp): the fixture's stand-ins, made from the morph plugin's fixture
// body and written into a pack added over the search path.
//
// ge:summary The staged packs: the fixture body scaled to stand in for players, heads, rockets
// ge:keywords stage-pak fixture stand-in player model missile gib pack
// ge:skill shooter labs
// ge:test tests/shooters_edge_stage_test.cpp
#include "shooters_edge/client/stage.hpp"

#include <algorithm>
#include <cstdint>

#include "morph/write.hpp"
#include "vfs/pak.hpp"

namespace shooters_edge::client {

namespace {

// The fixture body (an octahedron 32 units across, z up) stretched to `lo`..`hi` on z and `half`
// across, as a model file.
std::vector<uint8_t> stand_in(float half, float lo, float hi) {
  ge::morph::Model m = ge::morph::sample_model();
  for (ge::morph::Frame& f : m.frames) {
    for (size_t i = 0; i + 2 < f.positions.size(); i += 3) {
      f.positions[i] *= half / 16.0f;
      f.positions[i + 1] *= half / 16.0f;
      f.positions[i + 2] = (lo + hi) / 2 + f.positions[i + 2] * (hi - lo) / 32.0f;
    }
  }
  m.radius = std::max({half, -lo, hi});
  return ge::morph::write_model(m);
}

}  // namespace

std::vector<std::string> stage_pak_names() { return {"fixture"}; }

bool stage_pak(SearchPath& path, std::string_view name, std::vector<std::string>* staged,
               std::string& error) {
  if (name != "fixture") {
    std::string known;
    for (const std::string& n : stage_pak_names()) known += (known.empty() ? "" : ", ") + n;
    error = "no staged pack named '" + std::string(name) + "'; the staged packs: " + known +
            ". Pass --stage-pak=fixture";
    return false;
  }
  const std::vector<std::pair<std::string, std::vector<uint8_t>>> all = {
      {"progs/player.mdl", stand_in(16, -24, 32)},  // the player's hull
      {"progs/h_player.mdl", stand_in(8, -8, 10)}, {"progs/missile.mdl", stand_in(3, -3, 3)},
      {"progs/grenade.mdl", stand_in(4, -4, 4)},   {"progs/gib1.mdl", stand_in(4, -3, 3)},
      {"progs/gib2.mdl", stand_in(5, -4, 4)},      {"progs/gib3.mdl", stand_in(6, -4, 5)},
  };
  std::vector<std::pair<std::string, std::vector<uint8_t>>> missing;
  std::vector<uint8_t> store;
  for (const auto& [file, bytes] : all) {
    if (find_file(path, file, store).empty()) missing.emplace_back(file, bytes);
  }
  if (staged) {
    staged->clear();
    for (const auto& m : missing) staged->push_back(m.first);
  }
  if (missing.empty()) return true;
  std::vector<uint8_t> pak;
  if (!ge::vfs::write_pak(missing, &pak, &error)) return false;
  // Searched newest first, so it goes under nothing: it holds only what nothing else has.
  return add_archive(path, "stage-" + std::string(name) + ".pak", std::move(pak), error);
}

}  // namespace shooters_edge::client
