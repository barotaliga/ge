// EVERY DEATH'S PACK: the weapon in it guessed by the player's level from the shots heard (a
// novice's launcher, an average player's last weapon, an expert's certainty inside the refire);
// a fire sound read as its weapon; a kill of its own that nobody saw (a rocket into the water)
// placed where its rocket's blast was heard; no pack after the kill command; a place after `@`; a
// bare `coming` right after a pack call claims the pack for the one who said it.
//
// ge:summary Tests: a pack's weapon guessed by level, fire sounds, a pack placed by its own blast
// ge:keywords test packs pack_guess fire_sound_weapon packs_obituary blast known-pack packinfer
// ge:skill bots testing
#include "check.hpp"
#include "shooter/locs.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/mind.hpp"
#include "shooters_edge/bots/packs.hpp"
#include "shooters_edge/bots/talk.hpp"

namespace bt = shooters_edge::bots;
namespace sh = ge::shooter;

namespace {

bt::ShotsHeard heard(std::initializer_list<std::pair<int32_t, double>> shots) {
  bt::ShotsHeard h;
  for (const auto& [n, t] : shots) {
    h.at[n] = t;
    h.last = n;
    h.last_t = t;
  }
  return h;
}

}  // namespace

TEST(a_fire_sound_names_its_weapon) {
  CHECK(bt::fire_sound_weapon("weapons/sgun1.wav") == 7);
  CHECK(bt::fire_sound_weapon("weapons/grenade.wav") == 6);
  CHECK(bt::fire_sound_weapon("weapons/lhit.wav") == 8);
  CHECK(bt::fire_sound_weapon("weapons/lstart.wav") == 8);
  CHECK(bt::fire_sound_weapon("weapons/guncock.wav") == 2);
  CHECK(bt::fire_sound_weapon("player/pain1.wav") == 0);
}

TEST(each_level_reads_the_shots_its_own_way) {
  // A rocket, then the scattergun a second later; he dies 0.3 s after the scattergun.
  const bt::ShotsHeard rl_then_sg = heard({{7, 10.0}, {2, 11.0}});
  const bt::PackGuess novice = bt::pack_guess(rl_then_sg, bt::MapSense::novice, 11.3);
  const bt::PackGuess average = bt::pack_guess(rl_then_sg, bt::MapSense::average, 11.3);
  const bt::PackGuess expert = bt::pack_guess(rl_then_sg, bt::MapSense::expert, 11.3);
  CHECK(novice.weapon == 7);   // a rocket heard: a launcher
  CHECK(average.weapon == 0);  // the scattergun last: nothing but ammunition
  CHECK(expert.weapon == 0);
  CHECK(expert.sure);  // inside the scattergun's refire: no select can have gone through
  CHECK(novice.rockets && average.rockets && expert.rockets);  // rockets were fired: worth more
  // A rocket alone: inside its 0.8 s refire the launcher is surely in hand; after, believed.
  const bt::ShotsHeard rl = heard({{7, 10.0}});
  CHECK(bt::pack_guess(rl, bt::MapSense::expert, 10.5).sure);
  CHECK(bt::pack_guess(rl, bt::MapSense::expert, 10.5).weapon == 7);
  CHECK(!bt::pack_guess(rl, bt::MapSense::expert, 12.0).sure);
  CHECK(bt::pack_guess(rl, bt::MapSense::expert, 12.0).weapon == 7);
  // Out of the fight's memory: nothing believed.
  CHECK(bt::pack_guess(rl, bt::MapSense::novice, 30.0).weapon == 0);
  CHECK(!bt::pack_guess(rl, bt::MapSense::novice, 30.0).rockets);
  // A scattergun fight: no rockets, nothing worth more.
  CHECK(!bt::pack_guess(heard({{2, 10.0}}), bt::MapSense::average, 10.2).rockets);
}

TEST(its_own_rocket_into_the_water_places_the_pack_where_it_burst) {
  shooters_edge::server::RulesCommon s;
  s.world.entities.resize(8);
  s.world.clients[0].name = "me";
  s.world.clients[1].name = "foe";
  bt::Fighter f;
  f.name = "me";
  f.slot = 0;
  f.view_sent[0] = 0;  // level
  f.view_sent[1] = 0;  // along +x
  // Its rocket, then a blast near that line 900 units ahead, then the kill: nobody seen.
  f.bot.view.sounds = {{1, {0, 0, 0}, 1, "weapons/sgun1.wav"}};
  bt::packs_hear(f, s, 1, 20.0);
  f.bot.view.sounds = {{5, {900, 40, -60}, 1, "weapons/r_exp3.wav"}};
  bt::packs_hear(f, s, 1, 20.9);
  f.bot.view.sounds.clear();
  bt::packs_obituary(f, s, "foe rides me's rocket", 21.0);
  CHECK(f.known_packs.size() == size_t{1});
  const bt::KnownPack& p = f.known_packs[0];
  CHECK(p.how == static_cast<uint8_t>(bt::PackFrom::blast));
  CHECK(p.at.x == 900.f);
  CHECK(p.victim == 2);
  // A blast far off its line is not its rocket's.
  bt::Fighter g;
  g.name = "me";
  g.bot.view.sounds = {{1, {0, 0, 0}, 1, "weapons/sgun1.wav"}};
  bt::packs_hear(g, s, 1, 20.0);
  g.bot.view.sounds = {{5, {0, 900, 0}, 1, "weapons/r_exp3.wav"}};
  bt::packs_hear(g, s, 1, 20.5);
  g.bot.view.sounds.clear();
  bt::packs_obituary(g, s, "foe rides me's rocket", 21.0);
  CHECK(g.known_packs.empty());  // placed nowhere: not known
}

TEST(the_kill_command_drops_nothing) {
  shooters_edge::server::RulesCommon s;
  s.world.entities.resize(8);
  s.world.clients[0].name = "me";
  s.world.clients[1].name = "foe";
  bt::Fighter f;
  f.name = "me";
  f.last_seen_t[2] = 5.0;
  bt::packs_obituary(f, s, "foe suicides", 5.5);
  CHECK(f.known_packs.empty());
  bt::packs_obituary(f, s, "foe was gibbed by me", 5.5);  // seen fall: known there
  CHECK(f.known_packs.size() == size_t{1});
  CHECK(f.known_packs[0].how == static_cast<uint8_t>(bt::PackFrom::seen));
}

TEST(a_place_after_an_at_sign_reads_as_the_place) {
  const bt::Said s = bt::parse_say("rl pack @ ya");
  CHECK(s.kind == bt::SayKind::pack);
  CHECK(s.item == "rl");
  CHECK(s.loc == "ya");
  CHECK(bt::parse_say("coming @ga").loc == "ga");
  CHECK(bt::parse_say("coming @ga").kind == bt::SayKind::coming);
}

// The call `rl pack [ya]`, then a mate's bare `coming [ga]`: he takes it, the rest let it be. A
// `coming` long after the call is an ordinary one.
TEST(a_bare_coming_after_a_pack_call_claims_the_pack) {
  sh::Locs locs;
  locs.points = {{"ya", {1340, -544, 376}, {}}, {"ga", {784, 56, 304}, {}}};
  bt::Fighter f;
  f.name = "me";
  f.locs = &locs;
  bt::mind_hear(f, "anna", "rl pack [ya]", 30.0);
  CHECK(f.mind.pack_t == 30.0);
  bt::mind_hear(f, "bert", "coming [ga]", 31.0);
  CHECK(f.mind.pack_claim_t == 31.0);
  bt::Fighter g;
  g.name = "me";
  g.locs = &locs;
  bt::mind_hear(g, "anna", "rl pack [ya]", 30.0);
  bt::mind_hear(g, "bert", "coming [ga]", 40.0);
  CHECK(g.mind.pack_claim_t < g.mind.pack_t);
}

// A PLACE HEARD BY A NAME A LOC FILE REPEATS (`placeheard`, mind.cpp `place_heard`): two rooms
// named `tele` far apart; a mate last heard of beside the far one says `lost [tele]`. Off, the
// enemy is put at the middle of every `tele` point, in neither room; on, in the far room, at the
// red armour standing there.
TEST(a_place_name_a_loc_file_repeats_is_heard_as_the_room_round_the_speaker) {
  sh::Locs locs;
  locs.points = {{"tele", {0, 0, 0}, {}},
                 {"tele", {200, 0, 0}, {}},
                 {"tele", {3000, 0, 0}, {}},
                 {"tele", {3200, 0, 0}, {}},
                 {"big", {3100, 900, 0}, {}}};
  const auto heard = [&](bool on, bool armour) {
    bt::Fighter f;
    f.name = "me";
    f.locs = &locs;
    f.features.set(sh::Feature::placeheard, on);
    if (armour) {
      f.items.emplace_back();
      f.items.back().name = "item_armorInv";
      f.items.back().kind = bt::ItemKind::armor3;
      f.items.back().at = {3150, 40, 0};
    }
    bt::mind_hear(f, "anna", "safe [big]", 10.0);  // where she is
    bt::mind_hear(f, "anna", "lost [tele]", 11.0);
    return f.mind.enemy_at;
  };
  const sh::Vec3f off = heard(false, false), room = heard(true, false), ra = heard(true, true);
  CHECK(off.x == 1600);                     // the middle of both rooms: in neither
  CHECK(room.x == 3000 || room.x == 3200);  // a point of the far room
  CHECK(ra.x == 3150 && ra.y == 40);        // the red armour standing in it
}
