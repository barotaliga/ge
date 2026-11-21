// LAB `se-quake-dm`: DOES THE ARENA PLAY QUAKE'S OWN DEATHMATCH WHEN THE GAME ASKS FOR IT? The
// arena's rules in C++ (games/shooters_edge/rules) on the mod server, played by bots on the test
// arena twice from the same seed: once by the arena's own rules and once with the local info
// `rules q1` that Quake's packs write (rules/common_quake.cpp). Every server frame is read: each
// bot's model frame and health, every sound the rules start, every print. Tables: the frags and
// deaths, the sounds by sample side by side (pain and death cries, gibs, landings, spawn sounds,
// the arc gun's), the obituaries, and the frames each bot showed by sequence (run, stand, pain,
// firing, death). Pictures: one bot's model frame against the clock with the sequences as bands,
// its health under it, and the frags against the clock with the intermission marked. The lab
// fails on a fault, when nobody is fragged, or when Quake's rules play no pain or death frame.
//
// ge:summary Lab: Quake's deathmatch rules on the arena against its own: frames, sounds, frags
// ge:keywords lab se-quake-dm quake-rules rules-q1 deathmatch player-frames pain-frames
// ge:keywords death-frames gibs sounds intermission respawn-times arena
// ge:skill progs shooter labs
// ge:test tests/shooters_edge_quake_rules_test.cpp
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "arena_chart.hpp"
#include "core/num_text.hpp"
#include "shooters_edge/rules/arena.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace {

namespace gl = ge::lab;
namespace sv = shooters_edge::server;
namespace la = shooters_edge::labs;
using ge::core::num::to_fixed;
using Inputs = std::vector<std::pair<std::string, std::string>>;

constexpr double kFrame = 0.013;  // a server frame, s

// The player model's sequences by their first frame (measured from the model's frame names).
struct Sequence {
  int first, last;
  const char* name;
};
constexpr Sequence kSequences[] = {
    {0, 5, "axe run"},        {6, 11, "run"},
    {12, 16, "stand"},        {17, 28, "axe stand"},
    {29, 34, "axe pain"},     {35, 40, "pain"},
    {41, 49, "axe death"},    {50, 102, "deaths a to e"},
    {103, 104, "nailgun"},    {105, 106, "arc gun"},
    {107, 112, "launchers"},  {113, 118, "shotguns"},
    {119, 142, "axe swings"},
};

const char* sequence_of(float frame) {
  for (const Sequence& s : kSequences)
    if (frame >= static_cast<float>(s.first) && frame <= static_cast<float>(s.last)) return s.name;
  return "other";
}

double time_of(const sv::ModMatchReport& r, size_t frame) {
  return r.end_time - kFrame * static_cast<double>(r.frames) +
         kFrame * static_cast<double>(frame + 1);
}

sv::ModMatchReport play(gl::Lab& lab, bool quake) {
  sv::ModMatchOptions o;
  o.native = &sv::arena_rules();
  o.entities = sv::kBaselineArenaEntities;
  o.press_when_dead = true;
  o.bots = static_cast<int>(lab.arg_num("bots"));
  o.max_frames = static_cast<int>(lab.arg_num("frames"));
  o.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  o.console = {"deathmatch " + lab.arg("deathmatch"), "timelimit " + lab.arg("timelimit")};
  if (quake) o.console.insert(o.console.begin(), "localinfo rules q1");
  return sv::run_mod_match(o);
}

// The sample's name without its folder: "player/pain3.wav" -> "pain3".
std::string short_name(const std::string& sample) {
  const size_t slash = sample.rfind('/');
  std::string s = slash == std::string::npos ? sample : sample.substr(slash + 1);
  if (s.size() > 4 && s.compare(s.size() - 4, 4, ".wav") == 0) s.resize(s.size() - 4);
  return s;
}

void draw_frames(gl::Lab& lab, const sv::ModMatchReport& r, const Inputs& in) {
  if (r.players.empty()) return;
  const sv::PlayerPath& p = r.players[0];
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"ONE BOT'S BODY UNDER QUAKE'S RULES",
             p.name + ": the model's frame after every server frame, its health below",
             "server time (s)", "frame / health"};
  gl::Trace frame{"model frame", {}, {}};
  gl::Trace health{"health", {}, {}};
  for (size_t i = 0; i < p.frame_at.size(); i++) {
    frame.x.push_back(time_of(r, i));
    frame.y.push_back(p.frame_at[i]);
    health.x.push_back(time_of(r, i));
    health.y.push_back(p.health_at[i]);
  }
  c.traces = {frame, health};
  c.rules = {35, 41, 50, 103, 113};
  la::shoot(lab, "frames", gl::line_chart(c),
            "The first bot's model frame as Quake's rules set it, a server frame a point: the run "
            "and stand cycles low (0 to 28), pain from 29 or 35 for six frames when hurt, the "
            "firing sequences above 103 while the trigger is held, a death's frames from 41 or 50 "
            "to 102 held on its last until the bot stands again; the health line drops with each "
            "hit and jumps back to 100 at each respawn.",
            "dashed lines at the pain, axe death, deaths, nailgun and shotgun sequences' first "
            "frames; climbs through a death's frames ending flat on its last; no frame above 142",
            in);
}

void draw_frags(gl::Lab& lab, const sv::ModMatchReport& r, const Inputs& in) {
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"FRAGS UNDER QUAKE'S RULES", "each bot's frags after every server frame",
             "server time (s)", "frags"};
  for (const sv::PlayerPath& p : r.players) {
    gl::Trace t{p.name, {}, {}};
    for (size_t i = 0; i < p.frags_at.size(); i++) {
      t.x.push_back(time_of(r, i));
      t.y.push_back(p.frags_at[i]);
    }
    c.traces.push_back(t);
  }
  if (r.ended) c.marks.push_back({r.end_time, "a button takes everyone on"});
  la::shoot(lab, "frags", gl::line_chart(c),
            "Each bot's frags under Quake's rules: up one for a kill, down one for a death by the "
            "world or by its own hand, down two for the kill command; flat through the "
            "intermission, which holds the map at least 5 s and until a bot presses a button.",
            "steps of one (two for a kill command); the lines flat for at least 5 s before the "
            "mark",
            in);
}

void run(gl::Lab& lab) {
  const Inputs in = {{"bots", lab.arg("bots")},
                     {"deathmatch", lab.arg("deathmatch")},
                     {"timelimit min", lab.arg("timelimit")},
                     {"seed", lab.arg("seed")}};
  const sv::ModMatchReport own = play(lab, false);
  const sv::ModMatchReport quake = play(lab, true);
  for (const sv::ModMatchReport* r : {&own, &quake}) {
    if (!r->ok) {
      lab.fail("se-quake-dm: " + r->error,
               "next: run tests/shooters_edge_quake_rules_test.cpp, which names the failing mode");
      return;
    }
    if (r->faults > 0)
      lab.fail("se-quake-dm: the rules faulted; the last: " + r->last_fault,
               "next: read the stack in the fault; it names the rule's function");
  }

  gl::LabTable frags{"the frags and deaths",
                     {"player", "arena frags", "arena deaths", "Quake frags", "Quake deaths"},
                     {}};
  for (size_t k = 0; k < quake.players.size() && k < own.players.size(); k++) {
    const sv::PlayerPath& a = own.players[k];
    const sv::PlayerPath& b = quake.players[k];
    frags.rows.push_back({b.name, to_fixed(a.frags, 0), std::to_string(a.deaths),
                          to_fixed(b.frags, 0), std::to_string(b.deaths)});
  }
  lab.table(frags);

  std::map<std::string, std::pair<int, int>> sounds;
  for (const auto& s : own.sounds) sounds[short_name(s.sample)].first++;
  for (const auto& s : quake.sounds) sounds[short_name(s.sample)].second++;
  gl::LabTable sound_table{
      "the sounds the rules started, by sample", {"sample", "arena's rules", "Quake's rules"}, {}};
  for (const auto& [name, n] : sounds)
    sound_table.rows.push_back({name, std::to_string(n.first), std::to_string(n.second)});
  lab.table(sound_table);

  std::map<std::string, std::pair<int, int>> shown;
  for (const sv::PlayerPath& p : own.players)
    for (float f : p.frame_at) shown[sequence_of(f)].first++;
  for (const sv::PlayerPath& p : quake.players)
    for (float f : p.frame_at) shown[sequence_of(f)].second++;
  gl::LabTable frame_table{"server frames each sequence was shown, every bot",
                           {"sequence", "arena's rules", "Quake's rules"},
                           {}};
  for (const auto& [name, n] : shown)
    frame_table.rows.push_back({name, std::to_string(n.first), std::to_string(n.second)});
  lab.table(frame_table);

  gl::LabTable lines{"what Quake's rules printed (names left in)", {"line"}, {}};
  for (const std::string& p : quake.prints) {
    std::string l = p;
    while (!l.empty() && l.back() == '\n') l.pop_back();
    if (!l.empty() && lines.rows.size() < 40) lines.rows.push_back({l});
  }
  lab.table(lines);

  int pain = 0, death = 0;
  for (const auto& [name, n] : shown) {
    const std::string s = name;
    if (s == "pain" || s == "axe pain") pain += n.second;
    if (s == "deaths a to e" || s == "axe death") death += n.second;
  }
  lab.metric("quake.pain-frames", pain, "count");
  lab.metric("quake.death-frames", death, "count");
  lab.metric("quake.sounds", static_cast<double>(quake.sounds.size()), "count");
  lab.metric("quake.frag-log", static_cast<double>(quake.frag_log.size()), "count");
  lab.metric("quake.end-s", quake.end_time, "s");
  lab.metric("arena.frag-log", static_cast<double>(own.frag_log.size()), "count");
  if (quake.frag_log.empty())
    lab.fail("se-quake-dm: nobody was fragged under Quake's rules",
             "next: run tests/shooters_edge_quake_rules_test.cpp");
  if (pain == 0 || death == 0)
    lab.fail("se-quake-dm: Quake's rules showed no pain or no death frames",
             "next: read rules/common_quake.cpp's quake_pain and quake_die");

  draw_frames(lab, quake, in);
  draw_frags(lab, quake, in);
}

const gl::LabSpec kSpec{
    "se-quake-dm",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/rules/common_quake.cpp (Quake's own deathmatch rules on the arena, "
    "`rules q1`) against the arena's own, played by bots on the test arena",
    "progs",
    "Does the arena play Quake's deathmatch when the game asks: the body's frames, the cries, "
    "gibs and landings, the spawns, the frags and the intermission?",
    {
        {"bots", "4", "bots in the match, 1..32"},
        {"deathmatch", "1", "the deathmatch mode, 1..5"},
        {"timelimit", "1", "the match's time limit, minutes"},
        {"frames", "6000", "server frames of 13 ms at most"},
        {"seed", "1", "the server's generator; the bots' is seed + 1"},
    },
    "--timelimit=0.25 --frames=1900",
    run,
};
GE_LAB(kSpec);

}  // namespace
