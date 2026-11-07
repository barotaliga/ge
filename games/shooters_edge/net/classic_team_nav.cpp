// A team game's map shared by its fighters (classic_team_nav.hpp).
//
// ge:summary A team game's map shared by its fighters: nav, dives, place names, a fighter's setup
// ge:keywords team-nav team_fighter_setup dives locs_from_items
// ge:skill bots net shooter
#include "shooters_edge/net/classic_team_nav.hpp"

#include "ai/path.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/bots/knowledge.hpp"

namespace shooters_edge::net {

namespace {

// Place names off the items, for a map with no place names file: each key item, weapon and mega
// health's spot by its callout ("ra", "rl", "mh").
void locs_from_items(const std::vector<bt::ItemSpot>& items, sh::Locs& out) {
  for (const bt::ItemSpot& it : items) {
    if (!bt::key_item(it) && it.kind != bt::ItemKind::weapon && it.kind != bt::ItemKind::mega)
      continue;
    out.points.push_back({bt::item_call(it), it.at});
  }
  out.from = "the map's items";
}

}  // namespace

void team_fighter_setup(TeamNav& tn, bt::Fighter& f, const server::RulesCommon& seen, int slot,
                        const char* entities, sh::AimSkill skill, const sh::Traits& traits,
                        double hz, uint32_t seed, const std::string& name) {
  bt::MapNav& nav = tn.nav;
  const sh::NavGraph& graph = nav.graph;
  sh::MoveWorld& world = nav.world;
  const sh::SwimField& sw = nav.swim;
  const std::vector<uint8_t>& wet = nav.wet;
  const std::vector<sh::NavExtraLink>& entries = nav.entries;
  bt::fighter_init(f, graph, seen, slot, entities, skill, traits, hz, seed);
  f.bot.yaw = seen.sv.edicts[static_cast<size_t>(slot) + 1].v_angle.y;
  f.bot.pilot.world = &world;  // the hop looks ahead through the map
  f.bridges.clear();
  for (const bt::NavBridge& br : nav.bridges)
    f.bridges.push_back({br.ent, br.spawn, br.nodes, true});
  f.name = name;
  if (tn.locs.points.empty()) {
    if (!tn.given.points.empty()) {
      tn.locs = tn.given;
    } else {
      locs_from_items(f.items, tn.locs);
    }
  }
  f.locs = &tn.locs;
  if (!tn.plan_made) {
    bt::PlanSettings ps;
    ps.order = bt::opening_order_of(seen.map_name);  // the map's written opening, if it has one
    tn.plan = bt::make_map_plan(graph, f.items, f.spawns, &tn.locs, ps);
    std::string map = seen.map_name;
    if (map.rfind("maps/", 0) == 0) map.erase(0, 5);
    if (map.size() > 4 && map.ends_with(".bsp")) map.resize(map.size() - 4);
    tn.goals = bt::map_goals(tn.plan, f.items, map);
    tn.plan_made = true;
  }
  f.plan = &tn.plan;
  f.trap = &tn.nav.trap;
  if (!tn.dives_made && sw.leading > 0 && nav.main_n > 0) {
    tn.dives_made = true;
    const std::vector<bt::ItemSpot>& items = f.items;
    tn.item_dive.assign(items.size(), -1);
    for (size_t i = 0; i < items.size(); i++) {
      const int32_t node = items[i].node;
      if (node < 0 || nav.main.reached(node)) continue;
      for (size_t d = 0; d < tn.dives.size() && tn.item_dive[i] < 0; d++)
        if (tn.dives[d]->pocket[static_cast<size_t>(node)])
          tn.item_dive[i] = static_cast<int32_t>(d);
      if (tn.item_dive[i] >= 0) continue;
      auto dv = std::make_unique<TeamDive>();
      ge::ai::Search pr;
      sh::nav_reach(graph, node, pr);
      dv->pocket.assign(graph.nodes.size(), 0);
      for (size_t m = 0; m < graph.nodes.size(); m++)
        dv->pocket[m] = pr.reached(static_cast<int32_t>(m)) && !wet[m] &&
                                !nav.main.reached(static_cast<int32_t>(m))
                            ? 1
                            : 0;
      dv->pocket[static_cast<size_t>(node)] = 1;
      sh::build_swim(world, graph, dv->pocket, sh::SwimSettings{}, dv->field, &nav.swim);
      // Entered by a way into the water the graph closed, from a node of the main part that
      // reaches most of it again (not a pit dropped into), the one nearest the pocket by water.
      int32_t steps = -1;
      ge::ai::Search back;
      for (const sh::NavExtraLink& e : entries) {
        if (!nav.main.reached(e.from)) continue;
        const int32_t way =
            sh::swim_steps(dv->field, graph.nodes[static_cast<size_t>(e.to)].origin);
        if (way < 0 || (steps >= 0 && way >= steps)) continue;
        if (sh::nav_reach(graph, e.from, back) * 10 < nav.main_n * 9) continue;
        steps = way;
        dv->node = e.from;
        dv->entry = e.to;
      }
      if (dv->node < 0) continue;
      dv->swim_s = static_cast<float>(steps) * dv->field.cell / 200.f;  // a swimmer's pace
      tn.item_dive[i] = static_cast<int32_t>(tn.dives.size());
      tn.dives.push_back(std::move(dv));
    }
  }
  for (size_t i = 0; i < tn.item_dive.size() && i < f.items.size(); i++) {
    if (tn.item_dive[i] < 0) continue;
    const TeamDive& dv = *tn.dives[static_cast<size_t>(tn.item_dive[i])];
    f.items[i].dive = &dv.field;
    f.items[i].dive_node = dv.node;
    f.items[i].dive_entry = dv.entry;
    f.items[i].dive_s = dv.swim_s;
  }
  if (sw.leading > 0) f.swim = &nav.swim;
}

}  // namespace shooters_edge::net
