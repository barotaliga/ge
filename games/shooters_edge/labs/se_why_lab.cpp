// LAB `se-why`: WHY DID THAT BOT DO THAT, THERE? A bots' decision log (labs/why_log.hpp, written
// by `se-team --why=<file>` and by `make bot-video` beside its video) read for one bot and one
// moment: `--bot=red2 --at=4:12 --around=10` lists every decision that bot changed and every
// event (a speed lost to a wall or a player, a death) within 10 s of 4:12 on the recording's
// clock (the video's too), each with what settled it, the margin and the facts it read. With no
// `--at`, the whole match for that bot; with no `--bot`, every bot. The summary says how often
// each decision is made and how often it changes, a bot-minute (a decision that flips several
// times a second is jitter, not a mind), and which events happened how often. Without `--file`
// it reads a short log written in code (the smoke run's).
//
// ge:summary Lab: the bots' decision log read for one bot and one moment of the match recording
// ge:keywords lab se-why why-log decision-log bots why moment demo-clock flips changes events
// ge:keywords debug explain stopped-shooting mind
// ge:skill bots labs
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/labs/move_study.hpp"
#include "shooters_edge/labs/why_log.hpp"

namespace {

namespace gl = ge::lab;
namespace la = shooters_edge::labs;
using ge::core::num::to_fixed;

// A short log as se-team writes one: two bots, a fight begun and given up, a wall met, a death.
constexpr const char* kSample =
    "t\tbot\tkind\tname\tchosen\tbecause\tmargin\tfacts\n"
    "250.00\tred2\tdecision\tmode\tfight\tenemy_in_view\t0.80\tenemies=1 health=100\n"
    "250.00\tred2\tdecision\ttrigger\tfire\ton_target\t-\toff=2.10\n"
    "251.50\tred2\tdecision\ttrigger\thold\toff_target\t-\toff=18.00\n"
    "252.10\tred2\tdecision\tmode\tretreat\tlow_health\t0.30\tenemies=1 health=35\n"
    "252.40\tred2\tevent\tspeed_lost\thop\twalk\t180.00\tfrom=420 to=240 at=1904,-512,96\n"
    "253.00\tblue1\tdecision\tmode\troam\tno_enemy\t-\tenemies=0 health=100\n"
    "254.20\tred2\tdeath\tdied\trocket\tblue1\t-\tat=1910,-520,96\n"
    "-1\t*\tcount\tmode\t1200\t3\t-\t\n"
    "-1\t*\tcount\ttrigger\t9000\t2\t-\t\n";

// "4:12", "252" or "252.5" as seconds; NaN when it does not read.
double seconds_of(const std::string& text) {
  if (text.empty()) return std::nan("");
  const size_t colon = text.find(':');
  char* end = nullptr;
  if (colon == std::string::npos) {
    const double v = std::strtod(text.c_str(), &end);
    return end != text.c_str() ? v : std::nan("");
  }
  const double m = std::strtod(text.substr(0, colon).c_str(), nullptr);
  const double s = std::strtod(text.substr(colon + 1).c_str(), &end);
  return m * 60 + s;
}

std::string clock_of(double t) {
  if (t < 0) return "-";
  const int m = static_cast<int>(t / 60);
  const double s = t - m * 60;
  return std::to_string(m) + ":" + (s < 10 ? "0" : "") + to_fixed(s, 2);
}

void run(gl::Lab& lab) {
  std::string text = kSample, error;
  if (!lab.arg("file").empty()) {
    const std::vector<uint8_t> bytes = la::read_whole(lab.arg("file"));
    if (bytes.empty()) {
      lab.fail("se-why: the log " + lab.arg("file") + " does not open or is empty",
               "next: write one with se-team --why=<file> (or make bot-video), then pass it");
      return;
    }
    text.assign(bytes.begin(), bytes.end());
  }
  std::vector<la::WhyEntry> all;
  if (!la::read_why_log(text, all, error)) {
    lab.fail("se-why: " + error, "next: pass a file se-team --why=<file> wrote");
    return;
  }
  const std::string bot = lab.arg("bot");
  const double at = seconds_of(lab.arg("at"));
  const double around = lab.arg_num("around");
  if (!lab.arg("at").empty() && std::isnan(at)) {
    lab.fail("se-why: --at=" + lab.arg("at") + " is no time", "next: --at=4:12 or --at=252");
    return;
  }
  std::set<std::string> names;  // --name: only these decisions (comma separated)
  for (size_t i = 0, j; i < lab.arg("name").size(); i = j + 1) {
    j = std::min(lab.arg("name").find(',', i), lab.arg("name").size());
    names.insert(lab.arg("name").substr(i, j - i));
  }
  // The match: its length and its bots, off the entries.
  double t_end = 0;
  std::set<std::string> bots;
  for (const la::WhyEntry& e : all) {
    if (e.kind == "count") continue;
    t_end = std::max(t_end, e.t);
    if (e.kind == "decision") bots.insert(e.bot);
  }
  gl::LabTable rows{
      "the log" + (bot.empty() ? std::string() : ", " + bot) +
          (std::isnan(at) ? std::string()
                          : " within " + to_fixed(around, 0) + " s of " + clock_of(at)),
      {"at", "bot", "kind", "what", "chosen", "because", "margin", "facts"},
      {}};
  int shown = 0;
  std::map<std::string, int> events;
  for (const la::WhyEntry& e : all) {
    if (e.kind == "count") continue;
    if (e.kind != "decision") events[e.kind + " " + e.name]++;
    if (!bot.empty() && e.bot != bot) continue;
    if (!std::isnan(at) && std::fabs(e.t - at) > around) continue;
    if (!names.empty() && e.kind == "decision" && names.count(e.name) == 0) continue;
    if (shown++ >= static_cast<int>(lab.arg_num("rows"))) continue;
    rows.rows.push_back({clock_of(e.t), e.bot, e.kind, e.name, e.chosen, e.because,
                         std::isnan(e.margin) ? "-" : to_fixed(e.margin, 2), e.facts});
  }
  lab.table(rows);
  if (shown > static_cast<int>(lab.arg_num("rows")))
    lab.note("se-why: " + std::to_string(shown) + " rows, the first " + lab.arg("rows") +
             " shown; narrow with --bot, --at, --around or --name");
  // How often each decision is made and changed, a bot-minute.
  const double bot_min = std::max(1e-9, static_cast<double>(bots.size()) * t_end / 60);
  gl::LabTable counts{"each decision: made and changed a bot-minute (over " +
                          std::to_string(bots.size()) + " bots, " + clock_of(t_end) + ")",
                      {"decision", "made", "changed", "changed a second"},
                      {}};
  for (const la::WhyEntry& e : all) {
    if (e.kind != "count") continue;
    const double made = std::strtod(e.chosen.c_str(), nullptr),
                 changed = std::strtod(e.because.c_str(), nullptr);
    counts.rows.push_back({e.name, to_fixed(made / bot_min, 1), to_fixed(changed / bot_min, 1),
                           to_fixed(changed / bot_min / 60, 2)});
    lab.metric("why." + e.name + ".changed-per-bot-min", changed / bot_min, "per-min");
  }
  lab.table(counts);
  gl::LabTable ev{"events over the match", {"event", "count"}, {}};
  for (const auto& [name, n] : events) ev.rows.push_back({name, std::to_string(n)});
  lab.table(ev);
  lab.metric("why.rows-shown", shown, "count");
  lab.metric("why.entries", static_cast<double>(all.size()), "count");
}

const gl::LabSpec kSpec{
    "se-why",
    gl::LabKind::sim,
    "game:shooters_edge",
    "the bots' decision log: games/shooters_edge/labs/why_log.hpp, written by se-team --why",
    "bots",
    "Why did that bot do that at that moment of the match: which decisions changed, what settled "
    "them, and how often do the bots change their minds?",
    {
        {"file", "", "the log se-team --why=<file> wrote; empty: a short one written in code"},
        {"bot", "", "the bot's name (red2); empty: every bot"},
        {"at", "", "the moment on the recording's (and its video's) clock: 4:12 or 252"},
        {"around", "10", "with --at: the entries this many seconds either side"},
        {"name", "", "only these decisions, comma separated (mode,trigger)"},
        {"rows", "200", "at most this many rows shown"},
    },
    "--bot=red2 --at=4:12 --around=5",
    run,
};
GE_LAB(kSpec);

}  // namespace
