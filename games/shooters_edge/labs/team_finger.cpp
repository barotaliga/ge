// THE FINGER AND THE LEDGER, REPORTED (team_finger.hpp): the counts summed over the players.
//
// ge:summary se-team's finger and ledger metrics: summed over the players, shares and minutes
// ge:keywords report_finger finger-ledger ledger-miss burst-by rockets-per-owned-min
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/labs/team_finger.hpp"

#include <algorithm>
#include <string>

#include "shooterbots/fighter.hpp"
#include "shooters_edge/bots/arsenals.hpp"
#include "shooters_edge/bots/pilot_run.hpp"

namespace shooters_edge::labs {

namespace bt = shooters_edge::bots;
namespace cn = shooters_edge::net;

void report_finger(ge::lab::Lab& lab, const cn::TeamMatchReport& r, int64_t rockets, double secs) {
  // The roles' shares of every player's live commands (`team.role.<role>.share`): what a claim is
  // worth depends on how much of the time the rush and the push take a holder off it.
  {
    int64_t by[bt::kRoles] = {}, all = 0, cby[bt::kRoles] = {}, call = 0;
    for (const cn::TeamPlayer& p : r.players)
      for (int k = 0; k < bt::kRoles; k++) {
        by[k] += p.role_commands[k];
        all += p.role_commands[k];
        cby[k] += p.claim_role_commands[k];
        call += p.claim_role_commands[k];
      }
    lab.metric("team.role.claimed",
               all > 0 ? static_cast<double>(call) / static_cast<double>(all) : 0, "share");
    for (int k = 0; k < bt::kRoles; k++) {
      const std::string name = bt::role_name(static_cast<bt::Role>(k));
      lab.metric("team.role." + name + ".share",
                 all > 0 ? static_cast<double>(by[k]) / static_cast<double>(all) : 0, "share");
      // of the commands with a claim held, the share in each role (a holder off its area)
      lab.metric("team.role.claimant-" + name,
                 call > 0 ? static_cast<double>(cby[k]) / static_cast<double>(call) : 0, "share");
    }
  }
  // The quad's rushes begun strong (the launcher and armour), and (`rushstrong`) the thinks whose
  // ranking strength changed.
  {
    const int64_t rushes = r.teams[0].rushes + r.teams[1].rushes;
    lab.metric("team.rush.strong-share",
               rushes > 0
                   ? static_cast<double>(r.teams[0].rushes_strong + r.teams[1].rushes_strong) /
                         static_cast<double>(rushes)
                   : 0,
               "share");
    lab.metric("team.rush.strength-flips",
               static_cast<double>(r.teams[0].rush_strength_flips + r.teams[1].rush_strength_flips),
               "count");
  }

  // (18b) THE FINGER WITH THE LAUNCHER CARRIED: rockets fired a minute carrying it (se-people's
  // `rockets-per-owned-min`: people's dm2 17.8) and a minute holding it up, and the commands
  // carrying it by what the finger did: pressed (`down`: by the trigger; `other`: a spam shot, a
  // rocket jump), why it held with the launcher up, or another weapon up (`weapon`, `empty`).
  int64_t finger[bt::kFingerWhys] = {}, rl_cmds = 0, up_cmds = 0;
  for (const cn::TeamPlayer& p : r.players)
    for (int k = 0; k < bt::kFingerWhys; k++) {
      finger[k] += p.finger[k];
      rl_cmds += p.finger[k];
      if (k < static_cast<int>(bt::FingerWhy::weapon_fight)) up_cmds += p.finger[k];
    }
  const auto per_min_of = [&](int64_t cmds) {
    const double m = static_cast<double>(cmds) * bt::kPilotMsec / 1000.0 / 60;
    return m > 0 ? static_cast<double>(rockets) / m : 0;
  };
  lab.metric("team.finger.rockets-per-owned-min", per_min_of(rl_cmds), "per-min");
  // (`afterfire`) the rockets at one gone from sight a moment ago, a minute carrying the launcher
  int64_t after = 0, ear = 0;
  for (const cn::TeamPlayer& p : r.players) {
    after += p.after_shots;
    ear += p.ear_shots;
  }
  const double carried_min = static_cast<double>(rl_cmds) * bt::kPilotMsec / 1000.0 / 60;
  lab.metric("team.finger.after-per-owned-min",
             carried_min > 0 ? static_cast<double>(after) / carried_min : 0, "per-min");
  // The burst held past the near band, by what the crosshair met first (shooterbots/fighter.hpp).
  {
    int64_t by[4] = {}, n = 0;
    for (const cn::TeamPlayer& p : r.players)
      for (int w = 0; w < 4; w++) {
        by[w] += p.burst_by[w];
        n += p.burst_by[w];
      }
    static const char* kBy[4] = {"world-blocked", "player", "world-clear-turning",
                                 "world-clear-on-aim"};
    for (int w = 0; w < 4; w++)
      lab.metric(std::string("team.finger.burst-by.") + kBy[w],
                 n > 0 ? static_cast<double>(by[w]) / static_cast<double>(n) : 0, "share");
  }
  // (0q) THE LEDGER'S ERROR: a bot's belief of a live enemy's body (health and the armour's
  // share) against the truth: the mean miss and its sign, out of sight and in sight.
  {
    int64_t n[2] = {};
    double ab[2] = {}, bi[2] = {}, truth = 0, prior[2] = {};
    for (const cn::TeamPlayer& p : r.players) truth += p.ledger_truth;
    for (const cn::TeamPlayer& p : r.players)
      for (int k = 0; k < 2; k++) {
        n[k] += p.ledger_n[k];
        ab[k] += p.ledger_abs[k];
        bi[k] += p.ledger_bias[k];
        prior[k] += p.ledger_prior[k];
      }
    {
      static const char* kNews[6] = {"none", "spawn", "pickup", "cry", "hit", "typical"};
      int64_t bn[6] = {};
      double ba[6] = {}, bp[6] = {};
      for (const cn::TeamPlayer& p : r.players)
        for (int k = 0; k < 6; k++) {
          bn[k] += p.ledger_by_n[k];
          ba[k] += p.ledger_by_abs[k];
          bp[k] += p.ledger_by_prior[k];
        }
      int64_t all_n = 0;
      for (int64_t v : bn) all_n += v;
      for (int k = 0; k < 6; k++) {
        const std::string key = std::string("team.ledger.by.") + kNews[k];
        lab.metric(key + ".share",
                   all_n > 0 ? static_cast<double>(bn[k]) / static_cast<double>(all_n) : 0,
                   "share");
        lab.metric(key + ".miss", bn[k] > 0 ? ba[k] / static_cast<double>(bn[k]) : 0, "points");
        lab.metric(key + ".prior-miss", bn[k] > 0 ? bp[k] / static_cast<double>(bn[k]) : 0,
                   "points");
      }
    }
    lab.metric("team.ledger.truth-mean",
               n[0] + n[1] > 0 ? truth / static_cast<double>(n[0] + n[1]) : 0, "points");
    static const char* kWhere[2] = {"unseen", "seen"};
    for (int k = 0; k < 2; k++) {
      const std::string key = std::string("team.ledger.") + kWhere[k];
      lab.metric(key + ".miss", n[k] > 0 ? ab[k] / static_cast<double>(n[k]) : 0, "points");
      lab.metric(key + ".bias", n[k] > 0 ? bi[k] / static_cast<double>(n[k]) : 0, "points");
      // the floor: the miss of a belief that knew nothing (always the typical body)
      lab.metric(key + ".prior-miss", n[k] > 0 ? prior[k] / static_cast<double>(n[k]) : 0,
                 "points");
    }
  }
  // (`earfire`) and at one heard and not seen
  lab.metric("team.finger.ear-per-owned-min",
             carried_min > 0 ? static_cast<double>(ear) / carried_min : 0, "per-min");
  // (`earlob`) grenades thrown round a corner, a bot-minute (people's grenades 1.66 a minute)
  {
    int64_t lobs = 0;
    int64_t cleared = 0;
    for (const cn::TeamPlayer& p : r.players) {
      lobs += p.lob_shots;
      cleared += p.aim_cleared;
    }
    const double bm = secs / 60 * static_cast<double>(std::max<size_t>(r.players.size(), 1));
    lab.metric("team.finger.lobs-per-min", bm > 0 ? static_cast<double>(lobs) / bm : 0, "per-min");
    // (`aimclear`) commands a bot-minute the height aimed at was raised off a burst
    lab.metric("team.finger.aim-cleared-per-min", bm > 0 ? static_cast<double>(cleared) / bm : 0,
               "per-min");
  }
  lab.metric("team.finger.rockets-per-held-min", per_min_of(up_cmds), "per-min");
  lab.metric("team.finger.held-of-owned",
             rl_cmds > 0 ? static_cast<double>(up_cmds) / static_cast<double>(rl_cmds) : 0,
             "share");
  for (int k = 0; k < bt::kFingerWhys; k++)
    lab.metric(std::string("team.finger.") + bt::kFingerWhyNames[k],
               rl_cmds > 0 ? static_cast<double>(finger[k]) / static_cast<double>(rl_cmds) : 0,
               "share");
  // And with an enemy in plain view, loaded (se-people `rl.rockets-per-view-min`: people 54.9 a
  // minute, near the launcher's 75): what the finger did then.
  int64_t fv[bt::kFingerWhys] = {}, fv_n = 0;
  for (const cn::TeamPlayer& p : r.players)
    for (int k = 0; k < bt::kFingerWhys; k++) {
      fv[k] += p.finger_view[k];
      fv_n += p.finger_view[k];
    }
  for (int k = 0; k < bt::kFingerWhys; k++)
    lab.metric(std::string("team.finger.view.") + bt::kFingerWhyNames[k],
               fv_n > 0 ? static_cast<double>(fv[k]) / static_cast<double>(fv_n) : 0, "share");
  // By the range to the one aimed at (bt::kFingerBands; se-people `rl.near` and `rl.mid` by
  // the same bands): the commands with him in sight in each band, and of them the finger
  // pressing, held for the burst, another weapon up.
  static const char* kBand[3] = {"near", "mid", "far"};
  static const char* kWhat[3] = {"firing", "burst", "weapon"};
  int64_t band[3][3] = {}, band_n[3] = {}, band_all = 0;
  for (const cn::TeamPlayer& p : r.players)
    for (int b = 0; b < 3; b++)
      for (int w = 0; w < 3; w++) {
        band[b][w] += p.finger_band[b][w];
        band_n[b] += p.finger_band[b][w];
        band_all += p.finger_band[b][w];
      }
  for (int b = 0; b < 3; b++) {
    const std::string key = std::string("team.finger.band.") + kBand[b];
    lab.metric(key,
               band_all > 0 ? static_cast<double>(band_n[b]) / static_cast<double>(band_all) : 0,
               "share");
    for (int w = 0; w < 3; w++)
      lab.metric(
          key + "." + kWhat[w],
          band_n[b] > 0 ? static_cast<double>(band[b][w]) / static_cast<double>(band_n[b]) : 0,
          "share");
  }
}

}  // namespace shooters_edge::labs
