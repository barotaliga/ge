// A party of bots splitting a campaign map's work (plugins/shooterbots/party.hpp): each stop to
// the nearest bot, a stop a bot is on kept by him, a held stop skipped by the others, the people's
// leash, and what one bot saw or walked through told to the rest.
//
// ge:summary The party's share: nearest takes the stop, knowledge shared, the people's leash
// ge:keywords party party_share co-op split mates leash test
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
