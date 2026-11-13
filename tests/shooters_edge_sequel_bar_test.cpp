// Tests: the sequel's status bar from the server (games/shooters_edge/server/sequel_bar.cpp) in a
// hall built here: a fresh player's stats (health and its icon, the blaster selected, no ammo
// readout), a weapon picked up (its icon and name for 3 s, its ammo, selected), armour, a blow's
// flash, the deathmatch bar's frags, the item numbers, and the bar run through the layout language
// into the pictures a client draws.
//
// ge:summary Tests: the sequel's status bar stats, pictures, item names and programs
// ge:keywords test sequel status-bar stats pickup selected-item flash layout
// ge:skill shooter testing
// ge:test tests/shooters_edge_sequel_bar_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/hud/sequel_layout.hpp"
#include "shooters_edge/server/sequel_bar.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace hud = shooters_edge::hud;

using Pairs = std::vector<std::pair<std::string, std::string>>;

br::Entity entity(Pairs pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& things) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  for (const Pairs& m : things) w.entities.push_back(entity(m));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Hall {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::SequelServer& s() const { return *sv::sequel_of(r); }
  const int16_t* st() const { return s().world.clients[0].numbers.stats; }
  int image(const char* name) const {
    const auto& im = s().world.images;
    for (size_t i = 1; i < im.size(); i++)
      if (im[i] == name) return static_cast<int>(i);
    return -1;
  }
};

std::unique_ptr<Hall> open_hall(const std::vector<Pairs>& things, const char* dm = "0") {
  auto h = std::make_unique<Hall>();
  h->r = sv::sequel_runner(sv::make_sequel_server({}));
  h->r.console(std::string("deathmatch ") + dm);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall(things);
  std::string error;
  CHECK_MSG(h->r.start(map, error), error);
  h->r.connect(0, "Ana", false, false);
  return h;
}

void run(Hall& h, int frames) {
  for (int i = 0; i < frames * 8; i++) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++h.sequence;
    sh::MoveCmd c;
    c.msec = 13;
    pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
    h.r.frame(&pk, 1);
  }
}

}  // namespace

TEST(the_item_numbers_follow_the_games_list) {
  CHECK(sv::sq_item_number(sv::sq_item("item_armor_body")) == 1);
  CHECK(sv::sq_item_number(sv::sq_item("weapon_shotgun")) == 8);
  CHECK(sv::sq_item_number(sv::sq_item("ammo_grenades")) == 12);
  CHECK(sv::sq_item_number(sv::sq_item("item_health_mega")) == 41);
  CHECK(sv::sq_item_number(sv::sq_item("item_health_small")) == 41);
  CHECK(sv::sq_item_number(sv::sq_item("key_airstrike_target")) == 40);
  CHECK(sv::sq_weapon_number(0) == sv::kSqBlasterNumber);
  CHECK(sv::sq_icon(4) == "i_jacketarmor" && sv::sq_icon(41) == "i_health");
  CHECK(sv::sq_selects(23) && sv::sq_selects(12) && !sv::sq_selects(18) && !sv::sq_selects(1));
}

TEST(a_fresh_player_shows_health_and_the_blaster_selected) {
  auto h = open_hall({});
  run(*h, 2);
  const sv::RulesWorld& w = h->s().world;
  CHECK(!w.status_bar.empty() && w.status_bar.find("num 3 14") == std::string::npos);
  CHECK(w.item_names.size() == 42 && w.item_names[8] == "Shotgun");
  CHECK(h->st()[0] == h->image("i_health") && h->st()[0] > 0);
  CHECK(h->st()[1] == 100);
  CHECK(h->st()[2] == 0 && h->st()[3] == 0);  // the blaster takes no ammo
  CHECK(h->st()[4] == 0 && h->st()[5] == 0);
  CHECK(h->st()[6] == h->image("w_blaster") && h->st()[12] == 7);
  CHECK(h->st()[7] == 0);
}

TEST(a_weapon_picked_up_shows_for_three_seconds_and_is_selected) {
  auto h = open_hall({{{"classname", "weapon_shotgun"}, {"origin", "-600 0 24"}},
                      {{"classname", "item_armor_jacket"}, {"origin", "-600 0 24"}}});
  run(*h, 2);
  const int16_t* st = h->st();
  CHECK(st[7] == h->image("w_shotgun") || st[7] == h->image("i_jacketarmor"));
  CHECK(st[8] == 1056 + 8 || st[8] == 1056 + 3);
  CHECK(st[12] == 8 && st[6] == h->image("w_shotgun"));  // the armour has no use
  CHECK(st[4] == h->image("i_jacketarmor") && st[5] == 25);
  run(*h, 10);  // the blaster put away, the shotgun raised (not deathmatch)
  CHECK(st[2] == h->image("a_shells") && st[3] == 10);
  CHECK(st[7] != 0);
  run(*h, 22);
  CHECK(h->st()[7] == 0 && h->st()[8] == 0);
}

TEST(a_blow_flashes_for_its_frame) {
  auto h = open_hall({{{"classname", "item_armor_jacket"}, {"origin", "-600 0 24"}}});
  run(*h, 2);
  sv::fight_hurt(h->s(), 0, -1, {}, 20, 0, 0);
  run(*h, 1);  // the frame after the blow shows it
  CHECK((h->st()[15] & 1) != 0 && (h->st()[15] & 2) != 0);
  run(*h, 1);
  CHECK(h->st()[15] == 0);
  CHECK(h->st()[1] < 100 && h->st()[5] < 25);
}

TEST(the_deathmatch_bar_shows_the_frags_and_runs_into_pictures) {
  auto h = open_hall({}, "1");
  run(*h, 2);
  const sv::RulesWorld& w = h->s().world;
  CHECK(w.status_bar.find("num 3 14") != std::string::npos);
  hud::LayoutInput in;
  in.program = w.status_bar;
  in.stats = std::span<const int16_t>(h->st(), 32);
  in.width = 640;
  in.height = 480;
  in.ctx = &w;
  in.string = [](const void* ctx, int i) -> std::string_view {
    const auto& ww = *static_cast<const sv::RulesWorld*>(ctx);
    if (i > hud::kLayoutImages && i < hud::kLayoutImages + int(ww.images.size()))
      return ww.images[size_t(i - hud::kLayoutImages)];
    return {};
  };
  std::vector<hud::LayoutOp> ops;
  hud::run_layout(in, ops);
  std::string seen;
  for (const hud::LayoutOp& o : ops) seen += o.name + " ";
  CHECK_MSG(seen.find("num_1 num_0 num_0 i_health w_blaster num_0") != std::string::npos, seen);
}
