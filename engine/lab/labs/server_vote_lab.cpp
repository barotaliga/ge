// LAB `server-vote`: players voting on a server's settings (engine/server/vote.hpp) with no game
// under it. People join and leave a server over a seeded session; now and then one calls a vote on
// a setting (fraglimit, timelimit, teamplay, and once a round on one the deny list keeps out); each
// person answers after a seeded delay, yes or no by how much the room likes it, or not at all. The
// map changes every `--map` seconds and every change a vote made goes back. It answers how many
// votes pass, fail, run out of time and are dropped; how long one stays open; whether a leaver
// tips one; whether every change goes back at the map's end; and whether a session replays to the
// bit. The voters, the yes needed and each vote's yes and no over time are drawn as a chart.
//
// ge:summary Lab: players voting on settings for a map: called, answered, passed, put back
// ge:keywords lab server-vote vote majority per-map revert sv_vote_deny voters chart
// ge:skill net labs
// ge:test tests/server_vote_test.cpp
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "core/num.hpp"
#include "core/num_text.hpp"
#include "core/prng.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "server/vote.hpp"

namespace {

namespace gl = ge::lab;
namespace gs = ge::server;
namespace gc = ge::core;

constexpr double kStep = 0.05;  // the session's clock step, s

// The game's settings: each a whole number in its range, the server's own value at the start.
struct Setting {
  const char* name;
  int low, high, own;
};
constexpr Setting kSettings[] = {
    {"fraglimit", 0, 100, 20}, {"timelimit", 0, 60, 10}, {"teamplay", 0, 2, 0}};

struct Game {
  std::map<std::string, std::string> values;
};

bool check(void* ctx, std::string_view name, std::string_view value, gs::VoteCheck& out) {
  const Game& g = *static_cast<const Game*>(ctx);
  for (const Setting& s : kSettings) {
    if (name != s.name) continue;
    const int v = std::atoi(std::string(value).c_str());
    if (v < s.low || v > s.high) {
      out.error = std::string(name) + " " + std::string(value) + " is outside " +
                  std::to_string(s.low) + ".." + std::to_string(s.high);
      return false;
    }
    out.before = g.values.at(s.name);
    return true;
  }
  out.error = std::string(name) + " is no setting here";
  return false;
}

struct Vote {
  std::string what, outcome;
  double called = 0, closed = 0;
  int yes = 0, no = 0, voters = 0;
};

struct Session {
  std::vector<double> t, voters, needed, yes, no;
  std::vector<Vote> votes;
  int passed = 0, failed = 0, expired = 0, dropped = 0, refused = 0, reverted = 0, maps = 0;
  int left_mid_vote = 0;
  bool all_back = true;  // every map started on the server's own values
  uint64_t digest = 1469598103934665603ull;
};

Session run_session(uint32_t seed, int people, double seconds, double map_s) {
  Session s;
  gc::Rng rng = gc::make_rng(seed);
  Game game;
  for (const Setting& st : kSettings) game.values[st.name] = std::to_string(st.own);
  const gs::VoteTarget target{&game, check};
  gs::VoteBook b;
  b.settings.time_s = 20;
  b.settings.wait_s = 10;
  std::vector<double> here_until(static_cast<size_t>(people)), back_at(static_cast<size_t>(people));
  std::vector<double> answer_at(static_cast<size_t>(people), -1);
  std::vector<int> answer(static_cast<size_t>(people), 0);  // 1 yes, 2 no
  for (int i = 0; i < people; i++) {
    here_until[static_cast<size_t>(i)] = gc::range(rng, 20, seconds);
    back_at[static_cast<size_t>(i)] = -1;
    gs::vote_seat(b, i, "p" + std::to_string(i + 1), 0);
  }
  double next_call = gc::range(rng, 3, 8), next_map = map_s;
  bool denied_tried = false;
  size_t said = 0;
  int open_at = -1;  // the index in `votes` of the open one
  // Each line said since the last look: a closing one ends the vote it is about.
  const auto drain = [&](double now) {
    for (; said < b.said.size(); said++) {
      const gs::VoteNote& n = b.notes[said];
      if (n.kind == gs::VoteNoteKind::called) {
        s.votes.push_back({b.asked.name + " " + b.asked.value, "", now, 0, 0, 0, 0});
        open_at = static_cast<int>(s.votes.size()) - 1;
      }
      Vote* v = open_at >= 0 ? &s.votes[static_cast<size_t>(open_at)] : nullptr;
      const char* outcome = nullptr;
      switch (n.kind) {
        case gs::VoteNoteKind::passed:
          s.passed++, outcome = "passed";
          break;
        case gs::VoteNoteKind::failed:
          s.failed++, outcome = "failed";
          break;
        case gs::VoteNoteKind::expired:
          s.expired++, outcome = "time";
          break;
        case gs::VoteNoteKind::cancelled:
          s.dropped++, outcome = "dropped";
          break;
        case gs::VoteNoteKind::refused:
          s.refused++;
          break;
        default:
          break;
      }
      if (outcome != nullptr && v != nullptr && v->outcome.empty()) {
        v->outcome = outcome;
        v->closed = now;
        v->yes = b.last_yes;
        v->no = b.last_no;
        v->voters = b.last_voters;
        open_at = -1;
      }
      for (char ch : b.said[said])
        s.digest = (s.digest ^ static_cast<uint8_t>(ch)) * 1099511628211ull;
    }
  };
  const auto steps = static_cast<int64_t>(gc::num::round(seconds / kStep));
  for (int64_t k = 0; k <= steps; k++) {
    const double now = static_cast<double>(k) * kStep;
    const bool was_open = b.open;
    // People come and go.
    for (int i = 0; i < people; i++) {
      const auto at = static_cast<size_t>(i);
      if (back_at[at] < 0 && now >= here_until[at]) {
        if (b.open) s.left_mid_vote++;
        gs::vote_seat(b, i, "", now);
        back_at[at] = now + gc::range(rng, 5, 20);
      } else if (back_at[at] >= 0 && now >= back_at[at]) {
        gs::vote_seat(b, i, "p" + std::to_string(i + 1), now);
        back_at[at] = -1;
        here_until[at] = now + gc::range(rng, 20, seconds);
      }
    }
    drain(now);
    // A call: a seeded voter on a seeded setting; once a map, one the deny list keeps out.
    if (!b.open && now >= next_call && b.voters != 0) {
      int caller = static_cast<int>(gc::range(rng, 0, people - 0.001));
      while ((b.voters & (1u << caller)) == 0) caller = (caller + 1) % people;
      std::string line;
      if (!denied_tried) {
        line = "vote hostname mine";
        denied_tried = true;
      } else {
        const Setting& st = kSettings[static_cast<int>(gc::range(rng, 0, 2.999))];
        const int v = static_cast<int>(gc::range(rng, st.low, st.high + 0.999));
        line = std::string("vote ") + st.name + " " + std::to_string(v);
      }
      gs::vote_command(b, target, caller, line, now);
      drain(now);
      if (b.open) {
        // How much the room likes it, then each voter's answer and when.
        const double like = gc::range(rng, 0.1, 0.9);
        for (int i = 0; i < people; i++) {
          const auto at = static_cast<size_t>(i);
          answer[at] = gc::range(rng, 0, 1) < like ? 1 : 2;
          answer_at[at] = gc::range(rng, 0, 1) < 0.15 ? -1 : now + gc::range(rng, 1, 25);
        }
      }
      next_call = now + gc::range(rng, 6, 14);
    }
    for (int i = 0; i < people && b.open; i++) {
      const auto at = static_cast<size_t>(i);
      if (i == b.caller || answer_at[at] < 0 || now < answer_at[at]) continue;
      answer_at[at] = -1;
      if ((b.voters & (1u << i)) != 0)
        gs::vote_command(b, target, i, answer[at] == 1 ? "vote yes" : "vote no", now);
      drain(now);
    }
    gs::vote_tick(b, now);
    drain(now);
    for (const gs::VoteChange& c : gs::vote_take(b)) game.values[c.name] = c.value;
    // The map changes: every change goes back.
    if (now >= next_map) {
      for (const gs::VoteChange& c : gs::vote_map_over(b)) {
        game.values[c.name] = c.value;
        s.reverted++;
      }
      for (const Setting& st : kSettings)
        s.all_back = s.all_back && game.values[st.name] == std::to_string(st.own);
      drain(now);
      s.maps++;
      next_map += map_s;
      denied_tried = false;
    }
    if (was_open || b.open || k % 20 == 0) {
      s.t.push_back(now);
      s.voters.push_back(gs::vote_voter_count(b));
      s.needed.push_back(gs::vote_needed(b));
      // A vote that closed this step is drawn at its last count, then 0.
      const bool closed = was_open && !b.open;
      s.yes.push_back(b.open ? gs::vote_count(b, gs::Ballot::yes) : closed ? b.last_yes : 0);
      s.no.push_back(b.open ? gs::vote_count(b, gs::Ballot::no) : closed ? b.last_no : 0);
    }
    s.digest = (s.digest ^ (uint64_t{b.voters} << 8 | (b.open ? 1u : 0u))) * 1099511628211ull;
  }
  return s;
}

std::string hex(uint64_t v) {
  char out[20];
  std::snprintf(out, sizeof out, "%016llx", static_cast<unsigned long long>(v));
  return out;
}

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const int people = static_cast<int>(lab.arg_num("people"));
  const double seconds = lab.arg_num("seconds");
  const double map_s = lab.arg_num("map");
  if (people < 1 || people > gs::kVoteSeats) {
    lab.fail("--people=" + lab.arg("people") + " does not fit the server's slots",
             "Pick 1.." + std::to_string(gs::kVoteSeats) + "; skill: net.");
    return;
  }
  const Session a = run_session(seed, people, seconds, map_s);
  const Session again = run_session(seed, people, seconds, map_s);
  gl::LabTable votes{"each vote: what, when, how it ended, and the count when it did",
                     {"vote", "called (s)", "open (s)", "ended", "yes", "no", "voters"},
                     {}};
  for (const Vote& v : a.votes)
    votes.rows.push_back({v.what, gc::num::to_fixed(v.called, 1),
                          v.outcome.empty() ? "-" : gc::num::to_fixed(v.closed - v.called, 1),
                          v.outcome.empty() ? "open" : v.outcome, std::to_string(v.yes),
                          std::to_string(v.no), std::to_string(v.voters)});
  gl::LabTable runs{"the session: what the votes came to, and the session run twice",
                    {"called", "passed", "failed", "time", "dropped", "refused", "put back", "maps",
                     "all back", "digest", "again"},
                    {}};
  runs.rows.push_back({std::to_string(a.votes.size()), std::to_string(a.passed),
                       std::to_string(a.failed), std::to_string(a.expired),
                       std::to_string(a.dropped), std::to_string(a.refused),
                       std::to_string(a.reverted), std::to_string(a.maps),
                       a.all_back ? "yes" : "NO", hex(a.digest), hex(again.digest)});
  lab.metric("votes.called", static_cast<double>(a.votes.size()), "votes");
  lab.metric("votes.passed", a.passed, "votes");
  lab.metric("votes.failed", a.failed, "votes");
  lab.metric("votes.expired", a.expired, "votes");
  lab.metric("votes.dropped", a.dropped, "votes");
  lab.metric("votes.refused", a.refused, "acts");
  lab.metric("changes.put_back", a.reverted, "changes");
  lab.metric("maps.all_back", a.all_back ? 1 : 0, "");
  lab.metric("replays_to_the_bit", a.digest == again.digest ? 1 : 0, "");
  lab.table(std::move(votes));
  lab.table(std::move(runs));
  gl::LineChart line;
  line.frame = {"VOTES ON THE SETTINGS", "seed " + lab.arg("seed"), "t (s)", "people"};
  line.spikes = false;
  line.traces.push_back({"voters", a.t, a.voters});
  line.traces.push_back({"yes needed", a.t, a.needed});
  line.traces.push_back({"yes", a.t, a.yes});
  line.traces.push_back({"no", a.t, a.no});
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"votes",
            chart.title,
            "People come and go on a server; now and then one calls a vote on a setting and the "
            "others answer after a seeded delay, or not at all. A vote passes the moment its yes "
            "pass the needed line (more than half the voters), fails when half say no, or runs "
            "out after 20 s. The map changes every --map seconds and every change goes back.",
            "yes reaching the needed line closes a vote as passed; a leaver lowers the line",
            {{"people", lab.arg("people")}, {"map", lab.arg("map")}}},
           gl::to_picture(chart.plot));
}

const gl::LabSpec kSpec{
    "server-vote",
    gl::LabKind::sim,
    "engine",
    "engine/server/vote (players' votes on the settings for one map)",
    "net",
    "Do players' votes on a server's settings pass with more than half of them, fail or run out "
    "otherwise, follow people leaving and joining, go back when the map changes, and replay to "
    "the bit?",
    {
        {"people", "6", "people on the server, each coming and going at seeded moments"},
        {"seconds", "240", "the session's length, s"},
        {"map", "80", "each map's length before the next one, s"},
        {"seed", "1", "the session's stream"},
    },
    "--seconds=120",
    run,
};
GE_LAB(kSpec);

}  // namespace
