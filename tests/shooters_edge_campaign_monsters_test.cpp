// Tests: the monsters of our campaign mod (games/shooters_edge/rules/campaign_ai.cpp,
// campaign_monsters*.cpp) on the campaign host, in the arena's west room. Each counts at spawn; a
// rifleman facing the player notices him and shoots; one facing away notices only a player who made
// noise; a death counts, is told and fires the monster's targets; a hound bites a player beside it;
// a hound on a path walks it corner to corner. The kinds that fight up close and in the air: a
// swordsman and a brute hurt a player near them, a ghoul shrugs off a small hit and is knocked down
// by a big one and gets up again whole, and only its whole health in one blow ends it; a flyer
// keeps its height and spits at the player. The big and late kinds: each counts at spawn and hurts
// a player in front of it, the giant takes half of a blast, and a spawn bursts when it dies. The
// bosses: the lava boss rises when used, throws, and falls to three strikes; arriving inside the
// final boss kills it and ends the game; a teleport train rides its path. A rifleman shows its
// model's frames for standing, shooting and falling.
//
// ge:summary Tests: our campaign mod's monsters: counting, noticing, attacking, paths, each kind
// ge:keywords test campaign-mod monsters monster-ai rifleman hound path-corner killed-monsters
// ge:keywords notice show-hostile swordsman brute ghoul flyer eel knockdown leaper giant caster
// ge:keywords trooper spawn heavy-swordsman blast-share lava-boss final-boss telefrag
// ge:keywords teleport-train event-lightning animation-frames death-frames
// ge:skill progs shooter testing
#include <algorithm>
#include <string>

#include "check.hpp"
#include "shooters_edge_campaign_fixture.hpp"

using namespace campfix;

TEST(monsters_count_at_spawn) {
  Game g(R"({
"classname" "monster_army"
"origin" "-100 200 24"
}
{
"classname" "monster_dog"
"origin" "-100 -200 24"
}
)");
  CHECK(g.s().q.faults == 0);
  CHECK(g.global("total_monsters") == 2);
  const int army = g.find("monster_army"), dog = g.find("monster_dog");
  CHECK(army > 0 && dog > 0);
  CHECK(g.field(army, g.f().health) == 30);
  CHECK(g.field(dog, g.f().health) == 25);
  CHECK(g.ed(army).origin.z > 23.9f && g.ed(army).origin.z < 24.1f);  // dropped to the floor
}

TEST(a_rifleman_facing_the_player_notices_and_shoots_him) {
  Game g(R"({
"classname" "monster_army"
"origin" "-100 0 24"
"angle" "180"
}
)");
  const int army = g.find("monster_army");
  for (int i = 0; i < kFrames * 6 && g.field(1, g.f().health) == 100; i++)
    CHECK_MSG(g.frame(180), g.error);
  CHECK(g.entity_of(army, g.f().enemy) == 1);
  CHECK(g.field(1, g.f().health) < 100);
  CHECK(g.s().q.faults == 0);
}

TEST(a_rifleman_facing_away_notices_only_a_noisy_player) {
  Game g(R"({
"classname" "monster_army"
"origin" "-100 0 24"
"angle" "0"
}
)");
  const int army = g.find("monster_army");
  for (int i = 0; i < kFrames * 3; i++) CHECK_MSG(g.frame(), g.error);
  CHECK(g.entity_of(army, g.f().enemy) == 0);  // near, behind it and quiet: unseen
  // The player fires (away from it, into the west wall): the noise gives him away.
  for (int i = 0; i < kFrames && g.entity_of(army, g.f().enemy) == 0; i++)
    CHECK_MSG(g.frame(0, 1), g.error);
  CHECK(g.entity_of(army, g.f().enemy) == 1);
  CHECK(g.s().q.faults == 0);
}

TEST(a_death_counts_and_fires_the_monsters_targets) {
  Game g(R"({
"classname" "monster_army"
"origin" "-100 0 24"
"angle" "0"
"killtarget" "gone"
}
{
"classname" "info_notnull"
"origin" "0 300 24"
"targetname" "gone"
}
)");
  const int army = g.find("monster_army");
  CHECK(g.find("info_notnull") > 0);
  g.set(army, g.f().health, 1);
  for (int i = 0; i < kFrames && g.global("killed_monsters") == 0; i++)
    CHECK_MSG(g.frame(0, 1), g.error);  // the scattergun, at it
  CHECK(g.global("killed_monsters") == 1);
  CHECK(g.field(army, g.f().takedamage) == 0);
  CHECK(g.find("info_notnull") == 0);  // its killtarget removed
  CHECK(g.s().q.faults == 0);
}

TEST(a_hound_beside_the_player_bites_him) {
  Game g(R"({
"classname" "monster_dog"
"origin" "-300 0 24"
"angle" "180"
}
)");
  for (int i = 0; i < kFrames * 4 && g.field(1, g.f().health) == 100; i++)
    CHECK_MSG(g.frame(), g.error);
  CHECK(g.field(1, g.f().health) < 100);
  CHECK(g.s().q.faults == 0);
}

TEST(a_hound_on_a_path_walks_it_corner_to_corner) {
  Game g(R"({
"classname" "monster_dog"
"origin" "-100 -300 24"
"target" "p1"
}
{
"classname" "path_corner"
"origin" "-100 200 24"
"targetname" "p1"
"target" "p2"
}
{
"classname" "path_corner"
"origin" "-100 -300 24"
"targetname" "p2"
"target" "p1"
}
)");
  // The player, in, hides from monsters (no-target) so the hound only walks.
  CHECK_MSG(g.frame(), g.error);
  g.set(1, g.f().flags, g.field(1, g.f().flags) + 128);
  const int dog = g.find("monster_dog");
  float furthest = -1000;
  int goal = g.entity_of(dog, g.f().goalentity), turns = 0;
  for (int i = 0; i < kFrames * 12; i++) {
    CHECK_MSG(g.frame(), g.error);
    furthest = std::max(furthest, g.ed(dog).origin.y);
    if (g.entity_of(dog, g.f().goalentity) != goal) {
      goal = g.entity_of(dog, g.f().goalentity);
      turns++;
    }
  }
  CHECK_MSG(furthest > 150, std::to_string(furthest));  // reached the far corner
  CHECK_MSG(turns >= 2, std::to_string(turns));         // and turned back at each end
  CHECK(g.entity_of(dog, g.f().enemy) == 0);
  CHECK(g.s().q.faults == 0);
}

TEST(the_close_and_air_kinds_count_at_spawn) {
  Game g(R"({
"classname" "monster_knight"
"origin" "-100 200 24"
}
{
"classname" "monster_ogre"
"origin" "-100 -200 24"
}
{
"classname" "monster_zombie"
"origin" "0 300 24"
}
{
"classname" "monster_wizard"
"origin" "0 -300 120"
}
{
"classname" "monster_fish"
"origin" "-300 300 60"
}
)");
  CHECK(g.s().q.faults == 0);
  CHECK(g.global("total_monsters") == 5);
  CHECK(g.field(g.find("monster_knight"), g.f().health) == 75);
  CHECK(g.field(g.find("monster_ogre"), g.f().health) == 200);
  CHECK(g.field(g.find("monster_zombie"), g.f().health) == 60);
  CHECK(g.field(g.find("monster_wizard"), g.f().health) == 80);
  CHECK(g.field(g.find("monster_fish"), g.f().health) == 25);
  CHECK(g.ed(g.find("monster_wizard")).origin.z == 120);  // a flyer stays where it is placed
}

TEST(a_swordsman_and_a_brute_hurt_a_player_near_them) {
  for (const char* kind : {"monster_knight", "monster_ogre"}) {
    Game g(std::string(R"({
"classname" ")") +
           kind + R"("
"origin" "-200 0 24"
"angle" "180"
}
)");
    for (int i = 0; i < kFrames * 8 && g.field(1, g.f().health) == 100; i++)
      CHECK_MSG(g.frame(180), g.error);
    CHECK_MSG(g.field(1, g.f().health) < 100, kind);
    CHECK_MSG(g.s().q.faults == 0, kind);
  }
}

TEST(a_ghoul_shrugs_off_small_hits_and_is_knocked_down_by_big_ones) {
  Game g(R"({
"classname" "monster_zombie"
"origin" "-200 0 24"
"angle" "180"
}
)");
  const int ghoul = g.find("monster_zombie");
  // The scattergun's pellets, each a small hit: it keeps its health, and turns on the player.
  CHECK_MSG(g.frame(0, 1), g.error);
  for (int i = 0; i < 10; i++) CHECK_MSG(g.frame(0), g.error);
  CHECK(g.field(ghoul, g.f().health) == 60);
  CHECK(g.field(ghoul, g.f().takedamage) != 0);
  CHECK(g.entity_of(ghoul, g.f().enemy) == 1);
  // A blow of 40 at once: down, not dead.
  CHECK(g.hit(ghoul, 1, 40));
  CHECK(g.field(ghoul, g.f().takedamage) == 0);
  CHECK(g.global("killed_monsters") == 0);
  // It gets up again whole.
  for (int i = 0; i < kFrames * 7 && g.field(ghoul, g.f().takedamage) == 0; i++)
    CHECK_MSG(g.frame(180), g.error);
  CHECK(g.field(ghoul, g.f().takedamage) != 0);
  CHECK(g.field(ghoul, g.f().health) == 60);
  // A blow of its whole health at once: in pieces.
  CHECK(g.hit(ghoul, 1, 60));
  CHECK(g.global("killed_monsters") == 1);
  CHECK(g.s().q.faults == 0);
}

TEST(a_flyer_keeps_its_height_and_spits_at_the_player) {
  Game g(R"({
"classname" "monster_wizard"
"origin" "-150 0 100"
"angle" "180"
}
)");
  const int wizard = g.find("monster_wizard");
  float lowest = 1000;
  for (int i = 0; i < kFrames * 8 && g.field(1, g.f().health) == 100; i++) {
    CHECK_MSG(g.frame(180), g.error);
    lowest = std::min(lowest, g.ed(wizard).origin.z);
  }
  CHECK(g.field(1, g.f().health) < 100);
  CHECK_MSG(lowest > 40, std::to_string(lowest));  // flying, not fallen to the floor
  CHECK(g.s().q.faults == 0);
}

TEST(the_big_and_late_kinds_count_at_spawn) {
  Game g(R"({
"classname" "monster_demon1"
"origin" "-100 200 24"
}
{
"classname" "monster_shambler"
"origin" "-100 -200 24"
}
{
"classname" "monster_shalrath"
"origin" "100 0 24"
}
{
"classname" "monster_enforcer"
"origin" "0 300 24"
}
{
"classname" "monster_tarbaby"
"origin" "0 -300 24"
}
{
"classname" "monster_hell_knight"
"origin" "-300 300 24"
}
)");
  CHECK(g.s().q.faults == 0);
  CHECK(g.global("total_monsters") == 6);
  CHECK(g.field(g.find("monster_demon1"), g.f().health) == 300);
  CHECK(g.field(g.find("monster_shambler"), g.f().health) == 600);
  CHECK(g.field(g.find("monster_shalrath"), g.f().health) == 400);
  CHECK(g.field(g.find("monster_enforcer"), g.f().health) == 80);
  CHECK(g.field(g.find("monster_tarbaby"), g.f().health) == 80);
  CHECK(g.field(g.find("monster_hell_knight"), g.f().health) == 250);
}

TEST(each_big_and_late_kind_hurts_a_player_in_front_of_it) {
  for (const char* kind : {"monster_demon1", "monster_shambler", "monster_shalrath",
                           "monster_enforcer", "monster_tarbaby", "monster_hell_knight"}) {
    Game g(std::string(R"({
"classname" ")") +
           kind + R"("
"origin" "-200 0 24"
"angle" "180"
}
)");
    for (int i = 0; i < kFrames * 10 && g.field(1, g.f().health) == 100; i++)
      CHECK_MSG(g.frame(180), g.error);
    CHECK_MSG(g.field(1, g.f().health) < 100, kind);
    CHECK_MSG(g.s().q.faults == 0, kind);
  }
}

TEST(the_giant_takes_half_of_a_blast) {
  Game g(R"({
"classname" "monster_shambler"
"origin" "-100 0 24"
}
{
"classname" "monster_hell_knight"
"origin" "-100 200 24"
}
)");
  const int giant = g.find("monster_shambler"), knight = g.find("monster_hell_knight");
  // A blast of 110 on each one's own origin: the giant's centre is 20 units up (100, half of
  // it taken), the heavy swordsman's 8 (106, all of it).
  CHECK(g.blast(giant, 1, 110));
  CHECK(g.blast(knight, 1, 110));
  CHECK_MSG(g.field(giant, g.f().health) == 550, std::to_string(g.field(giant, g.f().health)));
  CHECK_MSG(g.field(knight, g.f().health) == 144, std::to_string(g.field(knight, g.f().health)));
  CHECK(g.s().q.faults == 0);
}

TEST(a_spawn_bursts_when_it_dies) {
  Game g(R"({
"classname" "monster_tarbaby"
"origin" "-340 0 24"
"spawnflags" "1"
}
)");
  CHECK_MSG(g.frame(180), g.error);
  const int spawn = g.find("monster_tarbaby");
  CHECK(spawn > 0);
  CHECK(g.hit(spawn, 1, 100));
  CHECK(g.global("killed_monsters") == 1);
  CHECK(g.find("monster_tarbaby") == 0);  // nothing left of it
  CHECK(g.field(1, g.f().health) < 100);  // the player beside it caught the burst
  CHECK(g.s().q.faults == 0);
}

TEST(the_lava_boss_rises_when_used_throws_and_falls_to_three_strikes) {
  Game g(R"({
"classname" "monster_boss"
"origin" "-150 0 24"
}
)");
  const int boss = g.find("monster_boss");
  CHECK(g.global("total_monsters") == 1);
  CHECK(g.ed(boss).solid == 0);  // under the lava until used
  CHECK_MSG(g.frame(0), g.error);
  CHECK(g.call("boss_use", {}, boss));
  CHECK(g.ed(boss).solid != 0);
  CHECK(g.field(boss, g.f().frame) <= 16);  // rising
  for (int i = 0; i < kFrames * 8 && g.field(1, g.f().health) == 100; i++)
    CHECK_MSG(g.frame(0), g.error);
  CHECK(g.field(1, g.f().health) < 100);  // its fire reached the player
  // The lightning with no doors to strike between does nothing.
  CHECK(g.call("lightning_use", {}, g.find("player")));
  CHECK(g.field(boss, g.f().health) == 3);
  for (int i = 0; i < 3; i++) CHECK(g.call("boss_struck", {}));
  CHECK(g.field(boss, g.f().health) == 0);
  CHECK(g.global("killed_monsters") == 1);
  CHECK(g.ed(boss).solid == 0);
  for (int i = 0; i < kFrames * 2; i++) CHECK_MSG(g.frame(0), g.error);
  CHECK(g.field(boss, g.f().frame) == 56);  // its death frames played through
  CHECK(g.s().q.faults == 0);
}

TEST(arriving_inside_the_final_boss_kills_it_and_ends_the_game) {
  Game g(R"({
"classname" "monster_oldone"
"origin" "-416 0 24"
}
)");
  const int boss = g.find("monster_oldone");
  CHECK(g.field(boss, g.f().health) == 40000);
  CHECK_MSG(g.frame(0), g.error);
  // The player stands inside it, as a teleporter would have put him there.
  CHECK(g.call("teleport_arrived", {g.ent(1)}));
  CHECK(g.global("killed_monsters") == 1);
  for (int i = 0; i < kFrames * 5 && g.global("intermission_running") == 0; i++)
    CHECK_MSG(g.frame(0), g.error);
  CHECK(g.global("intermission_running") == 1);
  CHECK(g.s().q.faults == 0);
}

TEST(a_teleport_train_rides_its_path) {
  Game g(R"({
"classname" "misc_teleporttrain"
"origin" "-300 0 100"
"target" "c1"
"speed" "200"
}
{
"classname" "path_corner"
"targetname" "c1"
"target" "c2"
"origin" "-300 0 100"
}
{
"classname" "path_corner"
"targetname" "c2"
"target" "c1"
"origin" "-100 0 100"
}
)");
  const int train = g.find("misc_teleporttrain");
  float most = -1000;
  for (int i = 0; i < kFrames * 2; i++) {
    CHECK_MSG(g.frame(0), g.error);
    most = std::max(most, g.ed(train).origin.x);
  }
  CHECK_MSG(most > -110, std::to_string(most));  // reached the far corner
  CHECK_MSG(g.ed(train).origin.x < most, std::to_string(g.ed(train).origin.x));  // and turned back
  CHECK(g.s().q.faults == 0);
}

TEST(a_rifleman_shows_its_kinds_frames_standing_shooting_and_falling) {
  Game g(R"({
"classname" "monster_army"
"origin" "-100 0 24"
"angle" "0"
}
)");
  const int army = g.find("monster_army");
  for (int i = 0; i < kFrames; i++) CHECK_MSG(g.frame(180), g.error);
  const float standing = g.field(army, g.f().frame);
  CHECK_MSG(standing >= 0 && standing <= 7, std::to_string(standing));  // its stand loop
  // Woken and angry: it runs or shoots, each with its own frames.
  CHECK(g.hit(army, 1, 1));
  bool shot = false;
  for (int i = 0; i < kFrames * 4; i++) {
    CHECK_MSG(g.frame(180), g.error);
    const float fr = g.field(army, g.f().frame);
    CHECK_MSG(fr <= 89, std::to_string(fr));
    shot = shot || (fr >= 81 && fr <= 89);
  }
  CHECK(shot);
  CHECK(g.hit(army, 1, 40));  // dead, not gibbed (-35 is the line)
  for (int i = 0; i < kFrames * 2; i++) CHECK_MSG(g.frame(180), g.error);
  // Its death frames played to the last, of one of its two sequences (8-17 or 18-28).
  const float lying = g.field(army, g.f().frame);
  CHECK_MSG(lying == 17 || lying == 28, std::to_string(lying));
  CHECK(g.s().q.faults == 0);
}
