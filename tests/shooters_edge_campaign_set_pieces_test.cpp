// Tests: the set pieces of our campaign (games/shooters_edge/rules/campaign_set_pieces.cpp) and
// the words at an episode's end (campaign_world.cpp), on the campaign host in the arena's west
// room. A train rides its path of corners and back; a secret door shot moves back, then aside, and
// closes again; an exploding box shot blows and hurts a player beside it, the shotgun hits it, and
// a box in another's blast goes up in the same blow; a single-player setmodel sizes a brush model
// file; a spike trap used fires at the player; a fireball spout throws; bubbles rise; an ambient
// sound leaves nothing behind; a rune taken holds its episode's closing words, and the intermission
// shows them before it leaves.
//
// ge:summary Tests: our campaign mod's set pieces and an episode's closing words
// ge:keywords test campaign-mod set-pieces func-train func-door-secret explobox trap-spikeshooter
// ge:keywords misc-fireball air-bubbles ambient-sound item-sigil finale episode-end
// ge:skill progs shooter testing
#include <algorithm>
#include <cmath>
#include <string>

#include "check.hpp"
#include "shooters_edge_campaign_fixture.hpp"

using namespace campfix;

namespace {

std::string text(Game& g, const char* global) {
  const pg::Def* d = pg::find_global(mod(), global);
  if (!d) return "?";
  const char* s = pg::vm_string(g.s().q.vm, static_cast<int32_t>(g.s().q.vm.globals[d->offset]));
  return s ? s : "";
}

}  // namespace

TEST(a_train_rides_its_path_of_corners_and_back) {
  // The doorway's door brush (x 128..144, y -64..64, z 0..128) as the train: its lowest corner
  // rides the path.
  Game g(R"({
"classname" "func_train"
"model" "*1"
"target" "t1"
"speed" "200"
}
{
"classname" "path_corner"
"targetname" "t1"
"target" "t2"
"origin" "128 -64 0"
}
{
"classname" "path_corner"
"targetname" "t2"
"target" "t1"
"origin" "128 -64 100"
"wait" "0.5"
}
)");
  const int train = g.find("func_train");
  CHECK(train > 0);
  float most = 0, least_after = 1000;
  for (int i = 0; i < kFrames * 2; i++) {
    CHECK_MSG(g.frame(180), g.error);
    most = std::max(most, g.ed(train).origin.z);
    if (most > 99) least_after = std::min(least_after, g.ed(train).origin.z);
  }
  CHECK_MSG(std::fabs(most - 100) < 1, std::to_string(most));  // up to the far corner
  CHECK_MSG(least_after < 1, std::to_string(least_after));     // and back down to the first
  CHECK(g.ed(train).origin.x == 0);
  CHECK_MSG(g.s().q.faults == 0, g.s().q.last_fault);
}

TEST(a_secret_door_shot_moves_back_then_aside_and_closes_again) {
  Game g(R"({
"classname" "func_door_secret"
"model" "*1"
"angle" "0"
"wait" "2"
}
)");
  const int door = g.find("func_door_secret");
  CHECK(door > 0);
  CHECK_MSG(g.frame(0), g.error);
  CHECK(g.ed(door).origin.x == 0);
  CHECK(g.hit(door, 1, 10));  // a shot opens it
  float back = 0, aside = 0;
  for (int i = 0; i < kFrames * 4; i++) {
    CHECK_MSG(g.frame(0), g.error);
    back = std::max(back, g.ed(door).origin.x);
    aside = std::min(aside, g.ed(door).origin.y);
  }
  CHECK_MSG(std::fabs(aside + 128) < 1,
            std::to_string(aside));                           // back to its right, its size there
  CHECK_MSG(std::fabs(back - 16) < 1, std::to_string(back));  // then along its angle
  for (int i = 0; i < kFrames * 7; i++) CHECK_MSG(g.frame(0), g.error);
  CHECK_MSG(std::fabs(g.ed(door).origin.x) < 0.5f && std::fabs(g.ed(door).origin.y) < 0.5f,
            std::to_string(g.ed(door).origin.x) + " " + std::to_string(g.ed(door).origin.y));
  CHECK(g.s().q.faults == 0);
}

// Each leg sounds as it starts and stops, and the door rests a second at its first stop.
TEST(a_secret_door_sounds_each_leg_and_rests_between_them) {
  Game g(R"({
"classname" "func_door_secret"
"model" "*1"
"angle" "0"
"targetname" "s"
}
)");
  const int door = g.find("func_door_secret");
  CHECK_MSG(g.frame(0), g.error);
  std::string played;
  const auto take = [&] {
    for (int i = 0; i < g.s().sv.sound_count; i++)
      if (g.s().sv.sounds[i].ent == door) played += std::string(g.s().sv.sounds[i].sample) + " ";
  };
  CHECK(g.call("secret_use", {}, door));
  take();
  int rest = 0;  // frames spent still at the first stop
  for (int i = 0; i < kFrames * 5; i++) {
    CHECK_MSG(g.frame(0), g.error);
    take();
    if (std::fabs(g.ed(door).origin.x) < 0.5f && std::fabs(g.ed(door).origin.y + 128) < 0.5f)
      rest++;
  }
  CHECK_MSG(played ==
                "doors/basesec2.wav doors/basesec1.wav doors/basesec2.wav doors/basesec1.wav "
                "doors/basesec2.wav ",
            played);
  CHECK_MSG(rest >= kFrames && rest <= kFrames + 4,
            std::to_string(rest));  // a second, and the frames either side
  CHECK(g.s().q.faults == 0);
}

TEST(an_exploding_box_shot_blows_and_hurts_a_player_beside_it) {
  Game g(R"({
"classname" "misc_explobox"
"origin" "-380 0 0"
}
)");
  const int box = g.find("misc_explobox");
  CHECK(box > 0);
  CHECK_MSG(g.frame(0), g.error);
  CHECK(g.hit(box, 1, 5));
  CHECK(g.find("misc_explobox") == box);  // 5 of its 20
  CHECK(g.hit(box, 1, 20));
  for (int i = 0; i < kFrames; i++) CHECK_MSG(g.frame(0), g.error);
  CHECK(g.find("misc_explobox") == 0);
  CHECK(g.field(1, g.f().health) < 100);
  CHECK(g.s().q.faults == 0);
}

TEST(an_exploding_box_stands_on_its_corner_and_the_shotgun_blows_it) {
  // The player at -416 0 24 looks along +x at the box 40 units off: a shot's pellets are line
  // traces, so they hit it only because it has a size.
  Game g(R"({
"classname" "misc_explobox"
"origin" "-360 -16 0"
}
)");
  const int box = g.find("misc_explobox");
  CHECK(box > 0);
  CHECK(g.ed(box).mins.x == 0 && g.ed(box).mins.y == 0 && g.ed(box).mins.z == 0);
  CHECK(g.ed(box).maxs.x == 32 && g.ed(box).maxs.y == 32 && g.ed(box).maxs.z == 64);
  for (int i = 0; i < kFrames && g.find("misc_explobox") == box; i++)
    CHECK_MSG(g.frame(0, 1), g.error);  // the attack button held
  CHECK(g.find("misc_explobox") == 0);
  CHECK(g.find("explo_box") == 0);  // gone in the frame it died
  CHECK(g.s().q.faults == 0);
}

TEST(a_box_in_another_boxes_blast_goes_up_in_the_same_blow) {
  Game g(R"({
"classname" "misc_explobox"
"origin" "-200 0 0"
}
{
"classname" "misc_explobox2"
"origin" "-140 0 0"
}
)");
  const int big = g.find("misc_explobox"), small = g.find("misc_explobox2");
  CHECK(big > 0 && small > 0);
  CHECK(g.ed(small).maxs.z == 32);
  CHECK_MSG(g.frame(0), g.error);
  CHECK(g.hit(big, 1, 20));  // no frame between: both gone in the one call
  CHECK(g.find("misc_explobox") == 0);
  CHECK(g.find("misc_explobox2") == 0);
  CHECK(g.s().q.faults == 0);
}

namespace {

int g_asked = 0;
bool box_bounds(void*, const std::string& name, sh::Vec3f& mins, sh::Vec3f& maxs) {
  g_asked++;
  if (name != "maps/b_explob.bsp") return false;
  mins = {0, 0, 0};
  maxs = {32, 32, 64};
  return true;
}

}  // namespace

TEST(a_single_player_setmodel_sizes_a_brush_model_file_from_its_bounds_one_wider) {
  g_asked = 0;
  Game g(R"({
"classname" "misc_explobox"
"origin" "-200 0 0"
}
)",
         {}, &box_bounds);
  const int box = g.find("misc_explobox");
  CHECK(box > 0);
  CHECK(g_asked == 1);  // the spawn's setmodel asked; its setsize then set 0..32 x 0..64
  const uint32_t model = *pg::entity_field(g.s().q.vm, box, g.f().model);
  CHECK(g.builtin(3, {g.ent(box), model}));  // setmodel
  CHECK(g_asked == 2);
  CHECK(g.ed(box).mins.x == -1 && g.ed(box).mins.z == -1);
  CHECK(g.ed(box).maxs.x == 33 && g.ed(box).maxs.y == 33 && g.ed(box).maxs.z == 65);
  CHECK(g.s().q.faults == 0);
}

TEST(a_spike_trap_used_fires_along_its_angle) {
  Game g(R"({
"classname" "trap_spikeshooter"
"origin" "-200 0 40"
"angle" "180"
"targetname" "trap"
}
)");
  const int trap = g.find("trap_spikeshooter");
  CHECK(trap > 0);
  CHECK_MSG(g.frame(0), g.error);
  CHECK(g.call("trap_fire", {}, trap));
  CHECK(g.find("spike") > 0);
  for (int i = 0; i < kFrames && g.field(1, g.f().health) == 100; i++)
    CHECK_MSG(g.frame(0), g.error);
  CHECK(g.field(1, g.f().health) == 91);  // one nail, 9
  CHECK(g.s().q.faults == 0);
}

TEST(a_fireball_spout_throws_and_bubbles_and_sounds_leave_no_fault) {
  Game g(R"({
"classname" "misc_fireball"
"origin" "-200 200 8"
}
{
"classname" "air_bubbles"
"origin" "-200 -200 8"
}
{
"classname" "ambient_drip"
"origin" "-300 0 64"
}
)");
  CHECK(g.find("ambient_drip") == 0);  // the clients loop it; nothing stays on the server
  bool thrown = false;
  for (int i = 0; i < kFrames * 6 && !thrown; i++) {
    CHECK_MSG(g.frame(0), g.error);
    thrown = g.find("fireball") > 0;
  }
  CHECK(thrown);
  CHECK(g.s().q.faults == 0);
}

TEST(a_rune_holds_its_episodes_words_and_the_intermission_shows_them_first) {
  Game g(R"({
"classname" "item_sigil"
"origin" "-416 0 24"
"spawnflags" "2"
}
)");
  for (int i = 0; i < 4; i++) CHECK_MSG(g.frame(0), g.error);
  CHECK(g.find("item_sigil") == 0);  // taken
  const std::string words = text(g, "finale_words");
  CHECK_MSG(words.find("second rune") != std::string::npos, words);
  CHECK(g.call("intermission_start", {}));
  for (int i = 0; i < kFrames * 3; i++) CHECK_MSG(g.frame(0), g.error);
  CHECK_MSG(g.frame(0, 1), g.error);  // the first button shows the words
  CHECK(text(g, "finale_words").empty());
  CHECK(g.global("intermission_running") == 1);
  CHECK(g.s().q.faults == 0);
}
