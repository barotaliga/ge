// What a campaign program's pickups do once taken (the `se-campaign` lab's `--pickups`): on a
// map the program has spawned, every item and weapon lying there is touched by the player in turn
// (its `touch` called as `self` with the player as `other`, the player first brought low so a
// health box or armour is wanted), then the map runs on for the given seconds. Each item is
// asked, right after its touch and at the end, whether it is still SHOWN: drawn by a window and
// sent to clients (`server::model_shown`: a model index and a model text). In single player and
// co-op a taken item is gone for good.
// Header-only, beside the labs (a lab's helper never goes into a library: the labs skill).
//
// ge:summary The campaign lab's pickups: every item touched, shown after and long after or not
// ge:keywords lab pickups items taken item-respawn model-shown campaign-lab single-player coop
// ge:skill progs shooter labs
#pragma once

#include <map>
#include <string>
#include <vector>

#include "campaign_movers.hpp"
#include "lab/lab.hpp"
#include "progs/file.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/rules_world.hpp"

namespace shooters_edge::labs {

// The counts the table folds to (the lab's metrics).
struct PickupCounts {
  int items = 0;      // items and weapons on the map at the start
  int taken = 0;      // hidden right after the touch
  int back = 0;       // shown again at the end (a taken one that came back)
  int text_only = 0;  // taken ones hidden by their model text alone (the index kept)
};

// Every item and weapon of the map touched in turn, then `seconds` of the map run on.
inline PickupCounts pickups_table(ge::lab::Lab& lab, server::Campaign& c,
                                  const ge::progs::Progs& progs, double seconds) {
  using namespace mv;
  PickupCounts n;
  if (!c.server) return n;
  sv::QcServer& s = *sv::qc_of(c.server);
  Fields f;
  f.classname = field_of(progs, "classname");
  f.touch = field_of(progs, "touch");
  f.self = global_of(progs, "self");
  f.other = global_of(progs, "other");
  f.activator = global_of(progs, "activator");
  const int health = field_of(progs, "health"), armor = field_of(progs, "armorvalue");
  std::vector<int> items;
  for (int e = 1; e < s.sv.num_edicts; e++) {
    if (s.sv.edicts[static_cast<size_t>(e)].free) continue;
    const std::string cn = string_at(s, e, f.classname);
    if ((cn.rfind("item_", 0) == 0 || cn.rfind("weapon_", 0) == 0) && word_at(s, e, f.touch) > 0)
      items.push_back(e);
  }
  n.items = static_cast<int>(items.size());
  std::map<int, bool> hidden, indexed;
  for (int e : items) {
    // Brought low each time, so every health box and armour is wanted.
    s.sv.edicts[1].health = 10;  // the loop's copy is what the program is handed
    if (health >= 0) *pg::entity_field(s.q.vm, 1, health) = pg::as_word(10.0f);
    if (armor >= 0) *pg::entity_field(s.q.vm, 1, armor) = pg::as_word(0.0f);
    call_as(s, f, word_at(s, e, f.touch), e);
    hidden[e] = !server::model_shown(s, e);
    indexed[e] = s.sv.edicts[static_cast<size_t>(e)].modelindex > 0;
  }
  std::string error;
  const int frames = static_cast<int>(seconds / sv::kCampaignStep);
  for (int i = 0; i < frames && c.server; i++)
    if (!sv::campaign_frame(c, nullptr, 0, error)) break;
  if (!c.server) return n;
  ge::lab::LabTable rows{"every item touched by the player, and whether it is still shown",
                         {"entity", "class", "at", "shown after the touch",
                          "model index kept (the text emptied alone)",
                          "shown " + ge::core::num::to_fixed(seconds, 0) + " s later"},
                         {}};
  for (int e : items) {
    const bool gone = s.sv.edicts[static_cast<size_t>(e)].free;
    const bool shown_end = !gone && server::model_shown(s, e);
    if (hidden[e]) n.taken++;
    if (hidden[e] && shown_end) n.back++;
    if (hidden[e] && indexed[e]) n.text_only++;
    const auto o = s.sv.edicts[static_cast<size_t>(e)].origin;
    rows.rows.push_back({std::to_string(e), string_at(s, e, f.classname),
                         std::to_string(static_cast<int>(o.x)) + " " +
                             std::to_string(static_cast<int>(o.y)) + " " +
                             std::to_string(static_cast<int>(o.z)),
                         hidden[e] ? "no" : "yes", hidden[e] && indexed[e] ? "yes" : "-",
                         gone        ? "no (removed)"
                         : shown_end ? "yes"
                                     : "no"});
  }
  lab.table(rows);
  return n;
}

}  // namespace shooters_edge::labs
