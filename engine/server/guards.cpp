// A server's chat flood rule, defined: the rule read from its text, the sliding check, the words.
//
// ge:summary The chat flood rule's definitions: parse, the sliding check, what a player is told
// ge:keywords flood-protection parse_flood_rule flood_check flood_text
// ge:skill net
// ge:test tests/server_guards_test.cpp
#include "server/guards.hpp"

#include <charconv>

namespace ge::server {

namespace {

// The next whole number in `text` from `at`, spaces skipped; false when there is none.
bool next_int(std::string_view text, size_t& at, long& out) {
  while (at < text.size() && (text[at] == ' ' || text[at] == '\t')) at++;
  const char* first = text.data() + at;
  const char* last = text.data() + text.size();
  const auto r = std::from_chars(first, last, out);
  if (r.ec != std::errc{} || (r.ptr != last && *r.ptr != ' ' && *r.ptr != '\t')) return false;
  at = static_cast<size_t>(r.ptr - text.data());
  return true;
}

}  // namespace

bool parse_flood_rule(std::string_view text, FloodRule& rule, std::string& error) {
  size_t at = 0;
  long n = 0, s = 0, t = 0;
  const bool read = next_int(text, at, n) && next_int(text, at, s) && next_int(text, at, t);
  while (at < text.size() && (text[at] == ' ' || text[at] == '\t')) at++;
  if (!read || at != text.size() || n < 1 || n > kMostFloodLines || s < 1 || t < 1) {
    error = "sv_floodprot \"" + std::string(text) +
            "\" is not \"N S T\" (N lines, 1..10, in S seconds, else silent T seconds; S and T at "
            "least 1); next: set it like \"10 1 1\"";
    return false;
  }
  rule.lines = static_cast<int>(n);
  rule.window_s = static_cast<double>(s);
  rule.silence_s = static_cast<double>(t);
  return true;
}

std::string flood_rule_text(const FloodRule& rule) {
  return std::to_string(rule.lines) + " " + std::to_string(static_cast<long>(rule.window_s)) + " " +
         std::to_string(static_cast<long>(rule.silence_s));
}

FloodVerdict flood_check(FloodGuard& g, const FloodRule& rule, double now_s) {
  if (now_s < g.silent_until) return FloodVerdict::silenced;
  const int n = rule.lines < 1 ? 1 : rule.lines > kMostFloodLines ? kMostFloodLines : rule.lines;
  // The N-th most recent allowed line: N slots back from the next one written.
  const int oldest = ((g.next - n) % kMostFloodLines + kMostFloodLines) % kMostFloodLines;
  const double then = g.times[oldest];
  if (then >= 0 && now_s - then < rule.window_s) {
    g.silent_until = now_s + rule.silence_s;
    return FloodVerdict::tripped;
  }
  g.times[g.next] = now_s;
  g.next = (g.next + 1) % kMostFloodLines;
  return FloodVerdict::allowed;
}

std::string flood_text(FloodVerdict verdict, const FloodGuard& g, const FloodRule& rule,
                       std::string_view message, double now_s) {
  switch (verdict) {
    case FloodVerdict::allowed:
      return {};
    case FloodVerdict::tripped:
      if (!message.empty()) return "FloodProt: " + std::string(message) + "\n";
      return "FloodProt: You can't talk for " + std::to_string(static_cast<long>(rule.silence_s)) +
             " seconds.\n";
    case FloodVerdict::silenced:
      break;
  }
  const auto left = static_cast<long>(g.silent_until - now_s);  // positive: truncation floors
  return "You can't talk for " + std::to_string(left) + " more seconds\n";
}

}  // namespace ge::server
