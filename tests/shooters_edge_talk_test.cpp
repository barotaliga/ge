// THE TEAM'S WORDS: every kind written and read back the same; the messages people type read as
// what they mean (colour codes and braces, other spellings, the old "enemy at mega" form, a timer
// with "delayed"); a server's team line split into its speaker and text.
//
// ge:summary Tests: team messages written and read back, players' spellings, team lines split
// ge:keywords test talk team-message parse_say say_text split_team_line timer status
// ge:skill shooter testing
#include "check.hpp"
#include "shooters_edge/bots/talk.hpp"

namespace bt = shooters_edge::bots;

TEST(every_kind_reads_back_as_written) {
  bt::SayStatus st;
  st.armor_kind = "ra";
  st.armor = 150;
  st.health = 100;
  st.weapon = "rl";
  const bt::SayKind kinds[] = {bt::SayKind::report, bt::SayKind::took,        bt::SayKind::safe,
                               bt::SayKind::lost,   bt::SayKind::help,        bt::SayKind::coming,
                               bt::SayKind::point,  bt::SayKind::enemy,       bt::SayKind::have,
                               bt::SayKind::get,    bt::SayKind::enemy_power, bt::SayKind::over,
                               bt::SayKind::timer,  bt::SayKind::attack};
  for (bt::SayKind k : kinds) {
    bt::Said s;
    s.kind = k;
    s.loc = "quad-low";
    s.item = k == bt::SayKind::enemy ? "" : "quad";
    if (k == bt::SayKind::took || k == bt::SayKind::point || k == bt::SayKind::coming ||
        k == bt::SayKind::attack)
      s.item = "ra";
    s.secs = k == bt::SayKind::timer ? 15 : -1;
    if (k == bt::SayKind::report || k == bt::SayKind::safe || k == bt::SayKind::help) s.status = st;
    const std::string text = bt::say_text(s);
    const bt::Said r = bt::parse_say(text);
    CHECK_MSG(r.kind == k, std::string(bt::say_kind_name(k)) + ": \"" + text + "\" read as " +
                               bt::say_kind_name(r.kind));
    if (k != bt::SayKind::get && k != bt::SayKind::over && k != bt::SayKind::timer)
      CHECK_MSG(r.loc == "quad-low", text);
    if (k == bt::SayKind::timer) CHECK(r.item == "quad" && r.secs == 15);
    if (k == bt::SayKind::point || k == bt::SayKind::took || k == bt::SayKind::attack)
      CHECK_MSG(r.item == "ra", text);
    if (k == bt::SayKind::safe || k == bt::SayKind::report)
      CHECK_MSG(r.status.armor == 150 && r.status.health == 100 && r.status.weapon == "rl" &&
                    r.status.armor_kind == "ra",
                text);
  }
}

TEST(what_people_type_reads_as_meant) {
  CHECK(bt::parse_say("{&c0b0safe&cfff} $[{ra}$] ra:200 h:100 rl").kind == bt::SayKind::safe);
  CHECK(bt::parse_say("GOT RA").kind == bt::SayKind::took);
  CHECK(bt::parse_say("going quad").kind == bt::SayKind::coming);
  CHECK(bt::parse_say("going quad").item == "quad");
  const bt::Said e = bt::parse_say("enemy at mega");
  CHECK(e.kind == bt::SayKind::enemy && e.loc == "mh");
  CHECK(bt::parse_say("enemy quad [rl]").kind == bt::SayKind::enemy_power);
  const bt::Said d = bt::parse_say("quad delayed 12 secs");
  CHECK(d.kind == bt::SayKind::timer && d.secs == 12 && d.item == "quad");
  CHECK(bt::parse_say("ra in 10").item == "ra");
  CHECK(bt::parse_say("quad over").kind == bt::SayKind::over);
  CHECK(bt::parse_say("get pent!").kind == bt::SayKind::get);
  CHECK(bt::parse_say("150/100 rl").kind == bt::SayKind::report);
  CHECK(bt::parse_say("gg").kind == bt::SayKind::none);
  const bt::Said a = bt::parse_say("push ya [gl]");
  CHECK(a.kind == bt::SayKind::attack && a.item == "ya" && a.loc == "gl");
}

TEST(a_team_line_splits_into_speaker_and_text) {
  std::string name, text;
  CHECK(bt::split_team_line("(red2): safe [ra] ra:200 h:100 rl\n", name, text));
  CHECK(name == "red2" && text == "safe [ra] ra:200 h:100 rl");
  CHECK(!bt::split_team_line("red2: gg\n", name, text));
}

TEST(a_plan_reads_as_the_strategy_its_words_mean) {
  const auto plan = [](const char* text) { return bt::parse_say(text); };
  for (const char* t : {"lets take quad", "all quad", "rush quad together", "Lets TAKE quad!"}) {
    const bt::Said s = plan(t);
    CHECK(s.kind == bt::SayKind::plan && s.strategy == bt::Strategy::take && s.item == "quad");
  }
  for (const char* t : {"camp ra and rl", "lets hold ra rl", "control ra + rl"}) {
    const bt::Said s = plan(t);
    CHECK(s.kind == bt::SayKind::plan && s.strategy == bt::Strategy::camp);
    CHECK(s.item == "ra" && s.item2 == "rl");
  }
  CHECK(plan("camp ra").strategy == bt::Strategy::camp && plan("camp ra").item2.empty());
  for (const char* t : {"free play", "go free", "play free now"})
    CHECK(plan(t).kind == bt::SayKind::plan && plan(t).strategy == bt::Strategy::free);
  for (const char* t : {"ok", "k", "np", "ok!"}) CHECK(plan(t).kind == bt::SayKind::ack);
  // Not plans: the old kinds keep their words.
  CHECK(plan("took quad").kind == bt::SayKind::took);
  CHECK(plan("enemy quad [ra]").kind == bt::SayKind::enemy_power);
  // Every wording a bot says reads back as its plan.
  for (int v = 0; v < 3; v++) {
    bt::Said s;
    s.kind = bt::SayKind::plan;
    s.strategy = bt::Strategy::camp;
    s.item = "ya";
    s.item2 = "gl";
    s.variant = v;
    const bt::Said r = bt::parse_say(bt::say_text(s));
    CHECK(r.strategy == bt::Strategy::camp && r.item == "ya" && r.item2 == "gl");
    s.strategy = bt::Strategy::take;
    CHECK(bt::parse_say(bt::say_text(s)).strategy == bt::Strategy::take);
    s.strategy = bt::Strategy::free;
    CHECK(bt::parse_say(bt::say_text(s)).strategy == bt::Strategy::free);
  }
}
