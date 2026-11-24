// A recipe's entity tools (entity_debug.hpp): the list, the hide set, the commands.
//
// ge:summary The entities in view listed, hidden and shown again for a recipe
// ge:keywords recipe entlist enthide entshow entities-in-view debug-entities hide-entity
// ge:skill window-recipes shooter
// ge:test tests/shooters_edge_entity_debug_test.cpp
#include "shooters_edge/client/entity_debug.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/place_seen.hpp"

namespace shooters_edge::client {

// Whether `eye` lies in a standing player's box at `origin` (16 units either way, 24 down to the
// feet, 32 up to the crown). A model drawn round the eye shows its open insides as stray
// see-through wedges; nothing a player means to see stands there.
bool encloses_eye(const ge::shooter::Vec3f& origin, const ge::shooter::Vec3f& eye) {
  constexpr float kHalf = 16, kDown = 24, kUp = 32;
  return eye.x > origin.x - kHalf && eye.x < origin.x + kHalf && eye.y > origin.y - kHalf &&
         eye.y < origin.y + kHalf && eye.z > origin.z - kDown && eye.z < origin.z + kUp;
}

HiddenEntities& hidden_entities() {
  static HiddenEntities h;
  return h;
}

bool entity_hidden(const HiddenEntities& h, int number, const std::string& model) {
  if (h.numbers.empty() && h.models.empty()) return false;
  return std::find(h.numbers.begin(), h.numbers.end(), number) != h.numbers.end() ||
         std::find(h.models.begin(), h.models.end(), model) != h.models.end();
}

std::string entity_list(const DemoState& s, float radius) {
  struct Row {
    float distance;
    std::string line;
  };
  std::vector<Row> rows;
  const auto add = [&](const std::string& name, const DemoEntity& e) {
    if (!e.present || e.model <= 0) return;
    const float dx = e.origin.x - s.eye.x, dy = e.origin.y - s.eye.y, dz = e.origin.z - s.eye.z;
    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (radius > 0 && d > radius) return;
    const std::string model = static_cast<size_t>(e.model) < s.models.size()
                                  ? s.models[static_cast<size_t>(e.model)]
                                  : "#" + std::to_string(e.model);
    char buf[256];
    std::snprintf(buf, sizeof buf,
                  "%-4s %-24s (%.0f %.0f %.0f) d=%.0f frame=%d skin=%d fx=%d alpha=%.2f scale=%.2f",
                  name.c_str(), model.c_str(), e.origin.x, e.origin.y, e.origin.z, d, e.frame,
                  e.skin, e.effects, e.alpha, e.scale);
    rows.push_back({d, buf});
  };
  for (size_t n = 1; n < s.entities.size(); n++) add(std::to_string(n), s.entities[n]);
  if (s.format != DemoFormat::single)
    for (size_t k = 0; k < s.players.size(); k++) add("p" + std::to_string(k), s.players[k]);
  std::stable_sort(rows.begin(), rows.end(),
                   [](const Row& a, const Row& b) { return a.distance < b.distance; });
  std::string out = std::to_string(rows.size()) + " entities in view";
  for (const Row& r : rows) out += "\n" + r.line;
  return out;
}

void add_entity_debug_commands(ge::console::Console& con, const PlaceSeen& seen) {
  using Con = ge::console::Console;
  using Args = ge::console::Args;
  con.add_command(
      "entlist",
      "entlist [radius]: the entities in view, nearest first: number, model, origin, "
      "distance, frame, skin, effects, alpha, scale",
      [&seen](Con& c, const Args& a) {
        if (!seen.on || seen.drawn == nullptr) {
          c.print("entlist: no server joined and no demo playing");
          return;
        }
        c.print(entity_list(*seen.drawn, a.size() > 1 ? std::strtof(a[1].c_str(), nullptr) : 0));
      });
  con.add_command("enthide", "enthide <number|model>...: those entities not drawn",
                  [](Con& c, const Args& a) {
                    if (a.size() < 2) {
                      c.print("Usage: enthide <number|model>...");
                      return;
                    }
                    HiddenEntities& h = hidden_entities();
                    for (size_t i = 1; i < a.size(); i++) {
                      char* end = nullptr;
                      const long n = std::strtol(a[i].c_str(), &end, 10);
                      if (end != a[i].c_str() && *end == '\0')
                        h.numbers.push_back(static_cast<int>(n));
                      else
                        h.models.push_back(a[i]);
                    }
                  });
  con.add_command("entshow",
                  "entshow [<number|model>...]: those entities drawn again (none named: all)",
                  [](Con&, const Args& a) {
                    HiddenEntities& h = hidden_entities();
                    if (a.size() < 2) {
                      h = {};
                      return;
                    }
                    for (size_t i = 1; i < a.size(); i++) {
                      std::erase(h.models, a[i]);
                      std::erase(h.numbers, std::atoi(a[i].c_str()));
                    }
                  });
}

}  // namespace shooters_edge::client
