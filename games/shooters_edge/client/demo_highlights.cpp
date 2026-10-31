// Shooter's Edge's highlight rules (demo_highlights.hpp).
//
// ge:summary The demo's highlight rules and kinds, and the finished list for one player
// ge:keywords highlights rules multi-kill long-shot air-kill annihilation streak player_name
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_highlights_test.cpp
#include "shooters_edge/client/demo_highlights.hpp"

#include <cmath>
#include <cstdio>

namespace shooters_edge::client {

namespace {

namespace gr = ge::replay;

bool scored(const DemoKill& k) { return k.killer >= 0 && k.killer != k.victim; }

std::string fmt(const char* f, double a, double b = 0) {
  char buf[96];
  std::snprintf(buf, sizeof buf, f, a, b);
  return buf;
}

}  // namespace

std::string player_name(const std::array<std::string, 32>& names, int slot) {
  std::string out;
  if (slot >= 0 && slot < 32)
    for (char c : names[static_cast<size_t>(slot)]) {
      const char a = static_cast<char>(static_cast<unsigned char>(c) & 0x7f);  // the coloured half
      if (a >= 32 && a < 127) out += a;
    }
  return out.empty() ? "player " + std::to_string(slot) : out;
}

const std::vector<gr::HighlightKind>& demo_kinds() {
  static const std::vector<gr::HighlightKind> kinds{
      {"multi-kill", "multi kill", 0xffc83dffu, 3, 2, 1},
      {"long-shot", "long rocket", 0x4fc3f7ffu, 3, 1.5, 1},
      {"air-kill", "air kill", 0xb388ffffu, 3, 1.5, 1},
      {"annihilation", "annihilation", 0xff5252ffu, 4, 1.5, 1},
      {"telefrag", "telefrag", 0xff9100ffu, 3, 1.5, 1},
      {"streak", "streak", 0x69f0aeffu, 8, 2, 1},
  };
  return kinds;
}

std::vector<DemoRule> demo_rules(const std::array<std::string, 32>& names) {
  const auto who = [names](int slot) { return player_name(names, slot); };
  std::vector<DemoRule> rules;
  rules.push_back(
      {"multi-kill", [who](std::span<const DemoKill> ks, std::vector<gr::Highlight>& out) {
         for (size_t i = 0; i < ks.size(); i++) {
           if (!scored(ks[i])) continue;
           std::vector<size_t> in;
           for (size_t k = i; k < ks.size() && ks[k].t - ks[i].t <= kOneShotS; k++)
             if (scored(ks[k]) && ks[k].killer == ks[i].killer) in.push_back(k);
           if (in.size() < 2) continue;
           std::string text = in.size() == 2   ? "double kill: "
                              : in.size() == 3 ? "triple kill: "
                                               : std::to_string(in.size()) + " kills: ";
           for (size_t n = 0; n < in.size(); n++) text += (n ? ", " : "") + who(ks[in[n]].victim);
           out.push_back(gr::highlight_at(ks[in.back()].t, 3.0 * static_cast<double>(in.size() - 1),
                                          ks[i].killer, text));
           i = in.back();
         }
       }});
  rules.push_back(
      {"long-shot", [who](std::span<const DemoKill> ks, std::vector<gr::Highlight>& out) {
         for (const DemoKill& k : ks) {
           if (!scored(k) || k.weapon != hud::Weapon::rocket_launcher) continue;
           const float reach = k.rocket_flight > 0 ? k.rocket_flight : k.distance;
           if (reach < kLongRocket) continue;
           out.push_back(gr::highlight_at(k.t, reach / 500.0, k.killer,
                                          fmt("long rocket, %.0f units: ", reach) + who(k.victim)));
         }
       }});
  rules.push_back(
      {"air-kill", [who](std::span<const DemoKill> ks, std::vector<gr::Highlight>& out) {
         for (const DemoKill& k : ks) {
           if (!scored(k) || !k.airborne) continue;
           if (k.weapon != hud::Weapon::rocket_launcher &&
               k.weapon != hud::Weapon::grenade_launcher)
             continue;
           out.push_back(
               gr::highlight_at(k.t, k.gib ? 3.0 : 2.0, k.killer,
                                std::string(k.gib ? "air gib: " : "air kill: ") + who(k.victim)));
         }
       }});
  rules.push_back(
      {"annihilation", [who](std::span<const DemoKill> ks, std::vector<gr::Highlight>& out) {
         for (const DemoKill& k : ks) {
           if (!scored(k) || k.stack < kBigStack || k.stack_s > kQuickS) continue;
           const double quick = 1 + (kQuickS - k.stack_s) / kQuickS;  // 1..2, quicker more
           const std::string how = k.stack_s < 0.05
                                       ? fmt(" annihilated: %.0f health and armour at once",
                                             static_cast<double>(k.stack))
                                       : fmt(" annihilated: %.0f health and armour in %.1f s",
                                             static_cast<double>(k.stack), k.stack_s);
           out.push_back(
               gr::highlight_at(k.t, k.stack / 100.0 * quick, k.killer, who(k.victim) + how));
         }
       }});
  rules.push_back(
      {"telefrag", [who](std::span<const DemoKill> ks, std::vector<gr::Highlight>& out) {
         for (const DemoKill& k : ks)
           if (scored(k) && k.telefrag)
             out.push_back(gr::highlight_at(k.t, 3, k.killer, "telefrag: " + who(k.victim)));
       }});
  rules.push_back({"streak", [who](std::span<const DemoKill> ks, std::vector<gr::Highlight>& out) {
                     std::vector<bool> used(ks.size(), false);
                     for (size_t i = 0; i < ks.size(); i++) {
                       if (!scored(ks[i]) || used[i]) continue;
                       size_t n = 0;
                       size_t last = i;
                       for (size_t k = i; k < ks.size() && ks[k].t - ks[i].t <= kStreakS; k++)
                         if (scored(ks[k]) && ks[k].killer == ks[i].killer) {
                           used[k] = true;
                           n++;
                           last = k;
                         }
                       if (n < static_cast<size_t>(kStreak)) continue;
                       out.push_back(gr::highlight_at(
                           ks[last].t, 1.5 * static_cast<double>(n - 2), ks[i].killer,
                           fmt("%.0f kills in %.0f s", static_cast<double>(n),
                               std::round(ks[last].t - ks[i].t))));
                     }
                   }});
  return rules;
}

gr::Highlights demo_highlights(const DemoScan& scan, int subject, int most) {
  const std::vector<DemoRule> rules = demo_rules(scan.names);
  std::vector<gr::Highlight> found = gr::run_highlight_rules<DemoKill>(scan.kills, rules);
  gr::HighlightPolicy policy;
  policy.subject = subject;
  policy.most = most;
  return gr::finish_highlights(std::move(found), demo_kinds(), scan.length_s, policy);
}

}  // namespace shooters_edge::client
