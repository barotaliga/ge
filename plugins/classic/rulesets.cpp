// The classic games' rulesets as data, and the fairness answer (rulesets.hpp).
//
// ge:summary The classic rulesets' books as data; f_ruleset's question and answer
// ge:keywords ruleset smackdown smackdrive qcon thunderdome mtfl duel f_ruleset rsn
// ge:skill classic console
// ge:test tests/classic_rulesets_test.cpp
#include "classic/rulesets.hpp"

#include <array>

namespace ge::classic {

namespace {

using console::RuleLock;
using console::RuleRefusal;
using console::Ruleset;

// Every ruleset but default turns off what this engine adds: the first three locks of each.
constexpr std::array kSmackdownLocks{
    RuleLock{.name = "ce_enabled", .value = "0"},
    RuleLock{.name = "cr_enabled", .value = "0"},
    RuleLock{.name = "cl_speedjump", .value = "0"},
    RuleLock{.name = "allow_scripts", .value = "0"},
    RuleLock{.name = "cl_idrive", .value = "0"},
    RuleLock{.name = "cl_safestrafe", .value = "0"},
    RuleLock{.name = "cl_hud", .value = "0"},
    RuleLock{.name = "cl_rollalpha", .value = "20"},
    RuleLock{.name = "cl_rollangle", .min = 0, .max = 5},
    RuleLock{.name = "r_shiftbeam", .value = "0"},
    RuleLock{.name = "scr_allowsnap", .value = "1"},
    RuleLock{.name = "cl_c2spps", .value = "0"},
    RuleLock{.name = "cl_physfps", .min = 0, .max = 77},
    RuleLock{.name = "r_drawparticles", .value = "1"},
};
constexpr std::array kSmackdriveLocks{
    RuleLock{.name = "ce_enabled", .value = "0"},
    RuleLock{.name = "cr_enabled", .value = "0"},
    RuleLock{.name = "cl_speedjump", .value = "0"},
    RuleLock{.name = "allow_scripts", .value = "0"},
    RuleLock{.name = "cl_hud", .value = "0"},
    RuleLock{.name = "cl_rollalpha", .value = "20"},
    RuleLock{.name = "cl_rollangle", .min = 0, .max = 5},
    RuleLock{.name = "r_shiftbeam", .value = "0"},
    RuleLock{.name = "scr_allowsnap", .value = "1"},
    RuleLock{.name = "cl_c2spps", .value = "0"},
    RuleLock{.name = "cl_physfps", .min = 0, .max = 77},
    RuleLock{.name = "r_drawparticles", .value = "1"},
};
constexpr std::array kQconLocks{
    RuleLock{.name = "ce_enabled", .value = "0"},
    RuleLock{.name = "cr_enabled", .value = "0"},
    RuleLock{.name = "cl_speedjump", .value = "0"},
    RuleLock{.name = "allow_scripts", .value = "0"},
    RuleLock{.name = "cl_idrive", .value = "0"},
    RuleLock{.name = "cl_hud", .value = "0"},
    RuleLock{.name = "cl_rollalpha", .value = "20"},
    RuleLock{.name = "cl_rollangle", .min = 0, .max = 5},
    RuleLock{.name = "r_shiftbeam", .value = "0"},
    RuleLock{.name = "cl_c2spps", .value = "0"},
    RuleLock{.name = "cl_physfps", .min = 0, .max = 77},
    RuleLock{.name = "r_drawparticles", .value = "1"},
    RuleLock{.name = "gl_outline", .value = "0"},
};
constexpr std::array kThunderdomeLocks{
    RuleLock{.name = "ce_enabled", .value = "0"},
    RuleLock{.name = "cr_enabled", .value = "0"},
    RuleLock{.name = "cl_speedjump", .value = "0"},
    RuleLock{.name = "allow_scripts", .value = "0"},
    RuleLock{.name = "cl_idrive", .value = "0"},
    RuleLock{.name = "cl_hud", .value = "0"},
    RuleLock{.name = "cl_rollangle", .min = 0, .max = 5},
    RuleLock{.name = "r_shiftbeam", .value = "0"},
    RuleLock{.name = "cl_c2spps", .value = "0"},
    RuleLock{.name = "cl_physfps", .min = 0, .max = 77},
};
constexpr std::array kMtflLocks{
    RuleLock{.name = "ce_enabled", .value = "0"},
    RuleLock{.name = "cr_enabled", .value = "0"},
    RuleLock{.name = "cl_speedjump", .value = "0"},
    RuleLock{.name = "r_drawflat", .value = "0"},
    RuleLock{.name = "gl_detpacklights", .value = "0"},
    RuleLock{.name = "gl_textureless", .value = "0"},
    RuleLock{.name = "r_fullbrightskins", .value = "0"},
    RuleLock{.name = "vid_software_palette", .value = "1"},
    RuleLock{.name = "cl_c2spps", .value = "0"},
    RuleLock{.name = "gl_picmip", .min = 0, .max = 3},
    RuleLock{.name = "gl_max_size", .min = 512},
};

// What the four strict rulesets refuse while a match is played, and our aids at all times.
constexpr std::array kStrictRefused{
    RuleRefusal{.command = "+rjump"},
    RuleRefusal{.command = "+rrjump"},
    RuleRefusal{.command = "exec", .match_only = true},
    RuleRefusal{.command = "packet", .match_only = true},
    RuleRefusal{.command = "play", .match_only = true},
    RuleRefusal{.command = "set_calc", .match_only = true},
    RuleRefusal{.command = "set_eval", .match_only = true},
    RuleRefusal{.command = "set_ex", .match_only = true},
    RuleRefusal{.command = "set_ex2", .match_only = true},
    RuleRefusal{.command = "timerefresh", .match_only = true},
};
constexpr std::array kAidsRefused{RuleRefusal{.command = "+rjump"},
                                  RuleRefusal{.command = "+rrjump"}};

constexpr std::array kQw{
    Ruleset{.name = "default", .title = "No rules: every setting as you like it."},
    Ruleset{.name = "smackdown",
            .title = "The league rules for duels and team games: no movement scripts or aids, "
                     "the game as it was made.",
            .locks = kSmackdownLocks,
            .refused = kStrictRefused},
    Ruleset{.name = "smackdrive",
            .title = "The league rules as smackdown, with the side-step aid allowed.",
            .locks = kSmackdriveLocks,
            .refused = kStrictRefused},
    Ruleset{.name = "qcon",
            .title = "The QuakeCon rules: as smackdown, with no outlines.",
            .locks = kQconLocks,
            .refused = kStrictRefused},
    Ruleset{
        .name = "thunderdome",
        .title = "The Thunderdome rules: no movement scripts or aids, effects as you like them.",
        .locks = kThunderdomeLocks,
        .refused = kStrictRefused},
    Ruleset{.name = "mtfl",
            .title = "The team fortress league rules: the world drawn as it is, no hidden walls.",
            .locks = kMtflLocks,
            .refused = kAidsRefused},
};

constexpr std::array kDuelLocks{
    RuleLock{.name = "ce_enabled", .value = "0"},
    RuleLock{.name = "cr_enabled", .value = "0"},
    RuleLock{.name = "cl_speedjump", .value = "0"},
    RuleLock{.name = "timescale", .value = "1"},
    RuleLock{.name = "gl_lightmap", .value = "0"},
    RuleLock{.name = "r_fullbright", .value = "0"},
    RuleLock{.name = "gl_showtris", .value = "0"},
    RuleLock{.name = "cl_thirdperson", .value = "0"},
    RuleLock{.name = "cl_maxfps", .min = 10, .max = 125},
};

constexpr std::array kQ2{
    Ruleset{.name = "default", .title = "No rules: every setting as you like it."},
    Ruleset{.name = "duel",
            .title = "The rules of Quake II duels: the game as it was made, nothing that helps.",
            .locks = kDuelLocks,
            .refused = kAidsRefused},
};

// The charset's coloured letters (the high half) as plain ones.
char plain(char c) { return static_cast<char>(static_cast<unsigned char>(c) & 0x7f); }

bool is_query_after(std::string_view text, std::string_view word) {
  if (text.size() < word.size()) return false;
  for (size_t i = 0; i < word.size(); ++i)
    if (plain(text[i]) != word[i]) return false;
  size_t i = word.size();
  while (i < text.size() && plain(text[i]) == ' ') ++i;
  if (i == word.size() && i < text.size() && text[i] != '\n') return false;  // "f_rulesets"
  while (i < text.size() && plain(text[i]) >= '0' && plain(text[i]) <= '9') ++i;
  while (i < text.size() && (plain(text[i]) == ' ' || text[i] == '\n')) ++i;
  return i == text.size();
}

}  // namespace

std::span<const Ruleset> qw_rulesets() { return kQw; }
std::span<const Ruleset> q2_rulesets() { return kQ2; }

std::string_view ruleset_info_key(bool q2) { return q2 ? "rsn" : "*rsn"; }

bool fair_query(std::string_view line, std::string_view word) {
  // The speaker's name may hold ": " itself: try after each.
  for (size_t at = line.find(": "); at != std::string_view::npos; at = line.find(": ", at + 1))
    if (is_query_after(line.substr(at + 2), word)) return true;
  return false;
}

std::string ruleset_reply(const FairFacts& f) {
  std::string on, off;
  const auto letter = [&](bool is_on, char c) { (is_on ? on : off) += c; };
  letter(f.modified, 'm');
  letter(f.scripts, 's');
  letter(f.forced_skins, 'f');
  letter(f.side_step, 'i');
  std::string out = f.server + " " + f.version + " " + f.ruleset;
  if (!on.empty()) out += "+" + on;
  if (!off.empty()) out += "-" + off;
  return out;
}

std::string fair_help() {
  return "f_ruleset: every player's client answers a chat line f_ruleset with its server, its "
         "version and its ruleset.\n"
         "f_ruleset check: ask everyone now.\n"
         "The answer ends with + and the aids on, then - and the aids off:\n"
         "m models or sounds changed, s movement scripts, f enemy skins forced, i side-step aid.";
}

bool FairReplies::may_answer(double now_s) {
  if (now_s - ruleset_at_s < kFairEveryS) return false;
  ruleset_at_s = now_s;
  return true;
}

}  // namespace ge::classic
