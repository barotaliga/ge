// A party of bots splitting a campaign map's work (plugins/shooterbots/party.hpp): each stop to
// the nearest bot, a stop a bot is on kept by him, a held stop skipped by the others, the people's
// leash, what one bot saw or walked through told to the rest, and what heals left to a hurt person.
//
// ge:summary The party's share: nearest takes the stop, knowledge shared, the people's leash
// ge:keywords party party_share co-op split mates leash test party_leave_items heal-tally
// ge:skill shooterbots bots testing
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooter/rules_world.hpp"
#include "shooter/server.hpp"
#include "shooterbots/party.hpp"

namespace sh = ge::shooter;
namespace sb = ge::shooterbots;

namespace {

// The least a BotView reads: the server's entities and the names.
struct Rules {
  sh::Server sv;
  sh::RulesWorld world;
  std::vector<std::string> models{""}, sounds{""};
  std::string map_name = "test";
};

std::unique_ptr<Rules> rules_with(int clients, int ents) {
  auto r = std::make_unique<Rules>();
  r->sv.set.maxclients = clients;
  r->sv.edicts.assign(static_cast<size_t>(ents), sh::Edict{});
  r->sv.num_edicts = ents;
  for (sh::Edict& e : r->sv.edicts) e.free = true;
  return r;
}

sb::WayStep step_at(sh::Vec3f p, sb::WayKind kind) {
  sb::WayStep st;
  st.lo = p - sh::Vec3f{8, 8, 8};
  st.hi = p + sh::Vec3f{8, 8, 8};
  st.kind = kind;
  return st;
}

// Two bots at x = -1000 and x = +1000; steps near each, a monster near the second, the exit.
struct Two {
  std::unique_ptr<Rules> r = rules_with(2, 8);
  sb::MapNav nav;
  sb::WayThrough a, b;
  sb::Party party;
  std::vector<sb::PartyMember> members;
  Two() {
    r->sv.edicts[1].free = false;
    r->sv.edicts[1].origin = {-1000, 0, 0};
    r->sv.edicts[2].free = false;
    r->sv.edicts[2].origin = {1000, 0, 0};
    sh::Edict& m = r->sv.edicts[5];  // a monster by the second
    m.free = false;
    m.flags = sh::kFlagMonster;
    m.solid = sh::kSolidSlidebox;
    m.health = 30;
    m.origin = {900, 0, 0};
    for (sb::WayThrough* w : {&a, &b}) {
      w->complete = true;
      w->steps = {step_at({-900, 0, 0}, sb::WayKind::button),
                  step_at({800, 0, 0}, sb::WayKind::secret), step_at({0, 0, 0}, sb::WayKind::exit)};
      w->seen.assign(4, 0);
    }
    members = {{&a, 1, true}, {&b, 2, true}};
  }
  void share(const sb::PartyOptions& opt = {}) {
    sb::party_share(party, members, nav, sb::BotView(*r), opt);
  }
};

}  // namespace

TEST(each_stop_goes_to_the_nearest_bot_and_the_exit_to_everyone) {
  Two t;
  t.share();
  CHECK(t.a.mates_steps.size() == 3 && t.b.mates_steps.size() == 3);
  CHECK(t.a.mates_steps[0] == 0 && t.b.mates_steps[0] == 1);  // the button: the first's
  CHECK(t.a.mates_steps[1] == 1 && t.b.mates_steps[1] == 0);  // the secret: the second's
  CHECK(t.a.mates_steps[2] == 0 && t.b.mates_steps[2] == 0);  // the exit: no one holds it
  CHECK(t.a.mates_prey[5] == 1 && t.b.mates_prey[5] == 0);    // the monster: the second's
  CHECK(t.a.mates_prey[3] == 0 && t.b.mates_prey[3] == 0);    // a free slot: nothing
  CHECK(t.party.stats.shares == 1);
}

TEST(a_stop_a_bot_is_on_stays_his_and_a_dead_bot_holds_nothing) {
  Two t;
  t.b.step = 0;  // the second is already on his way to the first's button
  t.b.kind = sb::WayKind::button;
  t.share();
  CHECK(t.a.mates_steps[0] == 1 && t.b.mates_steps[0] == 0);
  // Off it, the last holder keeps it until another is kPartyKeep nearer.
  t.b.step = -1;
  t.r->sv.edicts[2].origin = {-1050, 0, 0};  // 150 from the button, the first 100: kept
  t.share();
  CHECK(t.a.mates_steps[0] == 1 && t.b.mates_steps[0] == 0);
  t.r->sv.edicts[2].origin = {-700, 0, 0};  // now 200 from it: the first takes it back
  t.share();
  CHECK(t.a.mates_steps[0] == 0 && t.b.mates_steps[0] == 1);
  t.members[1].alive = false;
  t.share();
  CHECK(t.a.mates_steps[1] == 0 && t.a.mates_prey[5] == 0);  // all the first's now
}

TEST(what_one_bot_saw_or_walked_through_the_others_know) {
  Two t;
  t.a.seen[2] = 1;
  t.b.steps[1].ever = true;  // the second found the secret
  t.b.steps[0].ever = true;  // and pressed... a button is read from the world, not told
  t.share();
  CHECK(t.b.seen[2] == 1);
  CHECK(t.a.steps[1].ever);
  CHECK(!t.a.steps[0].ever);
  CHECK(t.party.stats.seen_told == 1 && t.party.stats.ever_told == 1);
}

TEST(with_people_playing_a_stop_far_from_all_of_them_is_no_ones) {
  Two t;
  sb::PartyOptions opt;
  opt.leash = 400;
  opt.humans = {{-1100, 0, 0}};  // a person by the first bot
  t.share(opt);
  CHECK(t.a.mates_steps[0] == 0);                             // the button near him: the first's
  CHECK(t.a.mates_steps[1] == 2 && t.b.mates_steps[1] == 2);  // the far secret: no one's
  CHECK(t.a.mates_prey[5] == 2 && t.b.mates_prey[5] == 2);    // and the far monster
  CHECK(t.party.stats.leashed == 2);
}

TEST(by_the_walk_a_stop_behind_a_wall_goes_to_the_bot_who_reaches_it_first) {
  Two t;
  // Every step on a node of its own; each bot's search from his last plan times the walk there.
  for (sb::WayThrough* w : {&t.a, &t.b})
    for (size_t i = 0; i < w->steps.size(); i++) w->steps[i].node = static_cast<int32_t>(i);
  t.a.search.reset(4);
  t.b.search.reset(4);
  t.a.search.seed(3);
  t.b.search.seed(3);
  // The button by the first bot is 0.3 s from him but behind a wall he must walk round: 9 s; the
  // second bot reaches it by a door in 6 s. The secret by the second only he reaches at all.
  t.a.search.reach(0, 9.0, 3);
  t.b.search.reach(0, 6.0, 3);
  t.b.search.reach(1, 0.5, 3);
  t.share();
  CHECK(t.a.mates_steps[0] == 1 && t.b.mates_steps[0] == 0);  // the button: the second's now
  CHECK(t.a.mates_steps[1] == 1 && t.b.mates_steps[1] == 0);  // the secret: his, the only one
  CHECK(t.party.stats.walked == 2);
  // Within kPartyKeep_s the holder keeps it: the first's walk now 6.1 s.
  t.a.search.reach(0, 6.1, 3);
  t.share();
  CHECK(t.b.mates_steps[0] == 0);
}

TEST(with_people_playing_the_exit_is_theirs_and_no_bots) {
  Two t;
  sb::PartyOptions opt;
  opt.leash = 4000;
  opt.humans = {{0, 0, 0}};
  opt.exit_held = true;
  t.share(opt);
  CHECK(t.a.mates_steps[2] == 2 && t.b.mates_steps[2] == 2);
  CHECK(t.a.mates_steps[0] == 0 && t.b.mates_steps[1] == 0);  // the work still split
}

namespace {

// A bot (entity 2) with three items before him: a health box, a mega health and an armour; the
// person (entity 1) beside them.
struct Heals {
  std::unique_ptr<Rules> r = rules_with(2, 4);
  sb::BotArsenal arsenal;
  sb::Fighter f;
  sb::PartyOptions opt;
  Heals() {
    arsenal.read = [](const sh::PlayerNumbers&, sb::Stats&) {};
    f.arsenal = &arsenal;
    r->world.entities.resize(4);
    for (int e : {1, 2}) {
      r->sv.edicts[static_cast<size_t>(e)].free = false;
      r->world.entities[static_cast<size_t>(e)].health = 100;
    }
    const sb::ItemKind kinds[] = {sb::ItemKind::health, sb::ItemKind::mega, sb::ItemKind::armor2,
                                  sb::ItemKind::shells};
    for (sb::ItemKind k : kinds) {
      sb::ItemSpot it;
      it.kind = k;
      it.at = {static_cast<float>(100 * f.items.size()), 0, 0};
      f.items.push_back(it);
    }
    opt.leash = sb::kPartyLeash;
    opt.humans = {{0, 0, 0}};
    opt.facing = {0};
  }
  void hold(int ent, float health, float armour, float type) {
    sh::PlayerNumbers& n = r->world.clients[static_cast<size_t>(ent) - 1].numbers;
    n.health = health;
    n.armour = armour;
    n.armour_type = type;
    r->world.entities[static_cast<size_t>(ent)].health = health;
  }
  int leave() {
    const sb::BotView s(*r);
    const sb::PartyHeld p = sb::party_held(s, 1);
    opt.health = {p.health};
    opt.armour = {p.armour};
    return sb::party_leave_items(f, s, 2, opt);
  }
};

}  // namespace

TEST(what_heals_is_left_to_a_person_who_needs_it_more) {
  Heals t;
  t.hold(1, 40, 0, 0);  // the person hurt, no armour
  t.hold(2, 90, 50, 0.6f);
  CHECK(t.leave() == 3);  // the box, the mega and the armour: his; the shells nobody's to keep
  CHECK(t.f.left_to_people[0] == 1 && t.f.left_to_people[1] == 1 && t.f.left_to_people[2] == 1);
  CHECK(t.f.left_to_people[3] == 0);
  // Both whole: the box heals nobody, the mega and the armour still the person's first.
  t.hold(1, 100, 200, 0.8f);
  t.hold(2, 100, 0, 0);
  CHECK(t.leave() == 1);  // the mega alone: the bot's armour stops less, so the armour is his
  CHECK(t.f.left_to_people[0] == 0 && t.f.left_to_people[1] == 1 && t.f.left_to_people[2] == 0);
}

TEST(a_bot_near_death_heals_and_a_dead_or_far_person_is_left_nothing) {
  Heals t;
  t.hold(1, 30, 0, 0);
  t.hold(2, sb::kPartyOwnNeed, 0, 0);  // down to kPartyOwnNeed: he heals beside the hurt person
  CHECK(t.leave() == 0);
  t.hold(2, 80, 0, 0);
  t.f.goal_item = 0;  // he was going for the box: let go
  CHECK(t.leave() == 3);
  CHECK(t.f.goal_item == -1);
  t.hold(1, 0, 0, 0);  // the person lying dead: nothing kept for him
  CHECK(t.leave() == 0);
  t.hold(1, 30, 0, 0);
  t.opt.humans = {{5000, 0, 0}};  // beyond the leash from every item
  CHECK(t.leave() == 0);
}

TEST(the_tally_counts_healing_a_member_took_from_a_person_in_need) {
  sb::PartyHealTally tally;
  const std::vector<sb::PartyHeld> before = {{40, 0}, {60, 10}, {0, 0}};
  // The person healed 15; member 1 took 25 health and 30 armour while the person had less; member
  // 2 came back from the dead (no pickup).
  const std::vector<sb::PartyHeld> after = {{55, 0}, {85, 40}, {100, 0}};
  sb::party_heal_tally(tally, before, after);
  CHECK(tally.person_healed == 15);
  CHECK(tally.health_in_need == 25);
  CHECK(tally.armour_in_need == 30);
  // The person whole: a member healing takes nothing from him.
  sb::PartyHealTally whole;
  sb::party_heal_tally(whole, std::vector<sb::PartyHeld>{{100, 50}, {60, 100}},
                       std::vector<sb::PartyHeld>{{100, 50}, {85, 100}});
  CHECK(whole.health_in_need == 0 && whole.armour_in_need == 0);
}
