// LAB `se-q2-cut`: WHAT EACH CLIENT OF QUAKE II'S SERVER IS SENT (server/q2_wire.hpp §
// the viewer's cut). Quake II's rules start on a map (a split hall built here, or a map of
// the player's own packs with `--game` and `--map`), every entity is linked by its box, and an eye
// is put in every cluster of the map in turn: how many of the entities shown the cut keeps for it,
// and what a whole frame of them costs on the wire, against sending every entity to everyone.
// Every view drawn on the map's plan, coloured by the share it is sent. Every number is
// deterministic.
//
// ge:summary Lab: Quake II server's frame cut to what each eye may see, measured over a map
// ge:keywords lab se-q2-cut pvs-cut view-cut frame-cut q2 visible-entities bandwidth
// ge:skill net shooter labs
// ge:test tests/shooters_edge_q2_net_test.cpp
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_wire.hpp"

namespace {

namespace br = ge::brush;
namespace gl = ge::lab;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, 1, "world/wall");
}

// A hall split in two by a wall at x 0, an item or two in each half, with visibility put in by
// hand (a brush world is written with none): every open leaf west of the wall is cluster 0, east of
// it cluster 1, and neither sees the other.
std::shared_ptr<const br::Map> halls(std::string& error) {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  m.push_back(box(-8, -1024, 0, 8, 1024, 256));
  m.push_back(box(-1040, -1040, 256, 1040, 1040, 272));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "0"}}));
  const char* items[][2] = {{"item_armor_body", "-300 300 16"},
                            {"weapon_railgun", "-600 -200 16"},
                            {"item_health_large", "300 -300 16"},
                            {"weapon_rocketlauncher", "600 200 16"},
                            {"ammo_rockets", "500 500 16"}};
  for (const auto& i : items) w.entities.push_back(entity({{"classname", i[0]}, {"origin", i[1]}}));
  std::vector<uint8_t> bytes;
  auto map = std::make_shared<br::Map>();
  if (!br::write_brush_world(w, &bytes, &error) || !br::read_bsp(bytes, map.get(), &error))
    return nullptr;
  for (br::Leaf& l : map->leaves) {
    const float mid = (l.mins[0] + l.maxs[0]) / 2;
    l.cluster = l.solid || (l.mins[0] < 0 && l.maxs[0] > 0) ? -1 : mid < 0 ? 0 : 1;
  }
  map->clusters = 2;
  map->row_bytes = 1;
  map->pvs = {1, 2};  // each half sees itself alone
  map->phs.clear();
  br::build_phs(map.get());
  return map;
}

std::shared_ptr<const br::Map> lab_map(gl::Lab& lab, std::string& name, std::string& error) {
  if (lab.arg("game").empty()) {
    name = "halls";
    return halls(error);
  }
  const pf::GameProfile& game = *pf::find_game("q2");
  const ge::classicenhance::Profile& content = pf::content_of(game);
  cl::SearchPath files;
  if (!cl::open_game_dir(lab.arg("game"), std::string(content.base_dir), files, error, content))
    return nullptr;
  name = lab.arg("map");
  std::unique_ptr<cl::LoadedMap> m = cl::load_named_map(files, name, error);
  auto map = std::make_shared<br::Map>();
  if (!m || !br::read_bsp(m->file, map.get(), &error)) return nullptr;
  return map;
}

std::string fixed(double v, int places) { return ge::core::num::to_fixed(v, places); }

void run(gl::Lab& lab) {
  std::string error, name;
  std::shared_ptr<const br::Map> map = lab_map(lab, name, error);
  if (map == nullptr)
    return lab.fail("the map did not load: " + error,
                    "Read the reason above; --game is the folder holding Quake II's base folder, "
                    "--map a map in it. Skill: brush.");
  sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
  r.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = name;
  spec.brushes = map;
  if (!r.start(spec, error))
    return lab.fail("Quake II's rules did not start " + name + ": " + error,
                    "Read the reason above. Skill: shooter.");
  // The rules' map, its hearing sets built (the server's own copy).
  const br::Map& m = *sv::q2_of(r)->map;
  sv::Q2Sight sight;
  sv::q2_sight_reset(sight, &m);
  sv::q2_sight_link_rules(sight, *r);
  std::vector<int> shown;
  std::vector<sv::Q2EntityState> states;
  for (int n = 1; n < sv::kQ2Entities; n++) {
    sv::Q2EntityState s = sv::q2_entity_state(*r, n);
    if (!s.present) continue;
    shown.push_back(n);
    states.push_back(s);
  }
  // A whole frame's entity list, as bytes: each entity from nothing.
  const auto bytes_of = [&](size_t i) {
    std::vector<uint8_t> out;
    const sv::Q2EntityState zero;
    sv::q2_write_entity(out, sv::q2_entity_flags(zero, states[i], shown[i], true), shown[i],
                        states[i]);
    return static_cast<double>(out.size());
  };
  double all_bytes = 0;
  for (size_t i = 0; i < shown.size(); i++) all_bytes += bytes_of(i);

  // One eye a cluster: the middle of its first open leaf.
  sv::Q2Wire w;
  w.slot = -1;  // no entity of its own
  w.sight = &sight;
  std::vector<bool> done(size_t(std::max(m.clusters, 0)), false);
  gl::Trace buckets[4] = {{"under a quarter sent", {}, {}},
                          {"a quarter to a half", {}, {}},
                          {"a half to three quarters", {}, {}},
                          {"three quarters or more", {}, {}}};
  gl::Sample shares{"share sent", {}};
  double kept_sum = 0, bytes_sum = 0, most = 0;
  int views = 0;
  for (const br::Leaf& leaf : m.leaves) {
    if (leaf.cluster < 0 || leaf.solid || size_t(leaf.cluster) >= done.size() ||
        done[size_t(leaf.cluster)])
      continue;
    done[size_t(leaf.cluster)] = true;
    const float eye[3] = {(leaf.mins[0] + leaf.maxs[0]) / 2, (leaf.mins[1] + leaf.maxs[1]) / 2,
                          (leaf.mins[2] + leaf.maxs[2]) / 2};
    if (!sv::q2_sight_view(w, eye)) break;
    int kept = 0;
    double bytes = 0;
    for (size_t i = 0; i < shown.size(); i++)
      if (sv::q2_sight_keeps(w, shown[i], states[i], eye)) {
        kept++;
        bytes += bytes_of(i);
      }
    const double share = shown.empty() ? 1.0 : double(kept) / double(shown.size());
    const int b = std::min(3, static_cast<int>(share * 4));
    buckets[b].x.push_back(eye[0]);
    buckets[b].y.push_back(eye[1]);
    shares.values.push_back(share * 100);
    kept_sum += kept;
    bytes_sum += bytes;
    most = std::max(most, share);
    views++;
  }
  if (views == 0)
    return lab.fail("the map " + name + " has no cluster to put an eye in",
                    "A map with no visibility sends every entity everywhere; pick a map compiled "
                    "with it. Skill: brush.");
  const double mean_kept = kept_sum / views, mean_bytes = bytes_sum / views;
  lab.metric("cut.entities_shown", double(shown.size()), "entities");
  lab.metric("cut.views", views, "clusters");
  lab.metric("cut.mean_sent", mean_kept, "entities");
  lab.metric("cut.mean_share", shown.empty() ? 1 : mean_kept / double(shown.size()), "share");
  lab.metric("cut.most_share", most, "share");
  lab.metric("cut.bytes_all", all_bytes, "bytes");
  lab.metric("cut.mean_bytes", mean_bytes, "bytes");
  gl::LabTable t{"a whole frame's entities, every entity against the cut", {"what", "value"}, {}};
  t.rows.push_back({"map", name});
  t.rows.push_back({"entities shown", std::to_string(shown.size())});
  t.rows.push_back({"eyes (one a cluster)", std::to_string(views)});
  t.rows.push_back({"sent to an eye, on average", fixed(mean_kept, 1)});
  t.rows.push_back({"bytes of a whole frame's entities, every entity", fixed(all_bytes, 0)});
  t.rows.push_back({"bytes of a whole frame's entities, cut, on average", fixed(mean_bytes, 0)});
  for (const gl::Trace& b : buckets)
    t.rows.push_back({"eyes sent " + b.label, std::to_string(b.x.size())});
  lab.table(t);
  if (most >= 1.0 && shown.size() > 4 && m.clusters > 1)
    lab.fail("some eye on " + name + " is sent every entity although the map has visibility",
             "Read q2_sight_keeps and brush::clusters_seen_from (an eye outside every cluster "
             "sees all). Skill: net.");

  std::string upper = name;
  for (char& c : upper) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
  gl::LineChart plan;
  plan.points = true;
  plan.spikes = false;
  plan.frame = {"WHAT EACH EYE ON " + upper + " IS SENT, FROM ABOVE",
                "one eye a cluster; the share of the map's " + std::to_string(shown.size()) +
                    " entities its frame holds",
                "x (units)", "y (units)"};
  for (const gl::Trace& b : buckets)
    if (!b.x.empty()) plan.traces.push_back(b);
  lab.shot({"plan",
            plan.frame.title,
            "Every cluster of " + name +
                " with an eye in its first open leaf, coloured by the "
                "share of the entities shown that Quake II's server sends it (the visible rows "
                "round the eye; beams by the hearable row; effects within 400 units).",
            "few eyes in the top colour; rooms far from the rest in the lowest",
            {{"entities", std::to_string(shown.size())},
             {"mean sent", fixed(mean_kept, 1)},
             {"bytes, all / cut", fixed(all_bytes, 0) + " / " + fixed(mean_bytes, 0)}}},
           gl::to_picture(gl::line_chart(plan).plot));
}

const gl::LabSpec kSpec{
    "se-q2-cut",
    gl::LabKind::sim,
    "game:shooters_edge",
    "Quake II server's frame cut to what each client may see (server/q2_wire.hpp)",
    "net",
    "How many of a map's entities does Quake II's server send an eye in each of its clusters, "
    "and what does a whole frame of them cost against sending every entity?",
    {{"game", "", "the folder holding Quake II's base folder (none: the split hall built here)"},
     {"map", "q2dm1", "the map measured with --game"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
