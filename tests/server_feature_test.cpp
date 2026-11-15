// The server's feature seam and the administrator's features (engine/server/feature.hpp,
// admin.hpp), over the simulated network (admin_trial.hpp).
//
// ge:summary Tests: the feature seam, status, rcon refused and run, kick, ban, the ban file
// ge:keywords test server feature admin rcon kick ban status log
// ge:skill net testing
#include <map>
#include <string>

#include "check.hpp"
#include "net/conditions.hpp"
#include "server/admin.hpp"
#include "server/admin_trial.hpp"
#include "server/feature.hpp"
#include "server/sample.hpp"

namespace gs = ge::server;
namespace gn = ge::net;

namespace {

std::string said_to(const gs::AdminTrial& t, const std::string& who) {
  std::string out;
  for (const gs::AdminLine& l : t.lines)
    if (l.player == who) out += (l.sent ? "> " : "") + l.text + "\n";
  return out;
}

}  // namespace

TEST(admin_trial_runs_every_step) {
  const gs::AdminTrial t = gs::run_admin_trial({});
  CHECK_MSG(t.error.empty(), t.error);
  const std::string ana = said_to(t, "Ana");
  CHECK_MSG(t.wrong_refused, "a wrong rcon password was not refused:\n" + ana);
  CHECK_MSG(t.bo_kicked && t.bo_back, "Bo was not kicked, or not let back:\n" + said_to(t, "Bo"));
  CHECK_MSG(t.cy_banned && t.cy_refused, "Cy was not banned and refused:\n" + said_to(t, "Cy"));
  CHECK_MSG(t.hello_answered, "the trial's own feature did not answer:\n" + ana);
  CHECK_MSG(t.unknown_told, "an unknown command was not told so:\n" + ana);
  CHECK_MSG(t.locked, "three wrong passwords did not make the tries wait:\n" + ana);
  CHECK(t.ok);
  CHECK(t.seated_end == 2);  // Ana and Bo
}

TEST(admin_status_names_the_players_without_addresses) {
  const gs::AdminTrial t = gs::run_admin_trial({});
  const std::string ana = said_to(t, "Ana");
  CHECK_MSG(ana.find("3 of 8 players") != std::string::npos, ana);
  CHECK_MSG(ana.find("Cy") != std::string::npos, ana);
  // The player's status shows no address; rcon's (the console's) does.
  const size_t own = ana.find("> status\n"), rcon = ana.find(" status\n", own + 4);
  CHECK(own != std::string::npos && rcon != std::string::npos);
  const size_t address = ana.find("sim:10:40000");
  CHECK_MSG(address != std::string::npos, "rcon status lacks the addresses:\n" + ana);
  CHECK_MSG(address > rcon, "the player's own status shows addresses:\n" + ana);
}

TEST(admin_ban_is_kept_in_the_file_and_the_log_hides_the_password) {
  const gs::AdminTrial t = gs::run_admin_trial({});
  CHECK_MSG(t.ban_file.find("addip ") == 0, "ban file: " + t.ban_file);
  bool kick = false, ban = false, refused = false, left = false;
  for (const std::string& l : t.log) {
    CHECK_MSG(l.find("secret") == std::string::npos, "the log shows the password: " + l);
    kick |= l.find("kick 1 Bo") != std::string::npos;
    ban |= l.find("ban 2 Cy") != std::string::npos;
    refused |= l.find("(banned)") != std::string::npos;
    left |= l.find("disconnect 1 kicked") != std::string::npos;
  }
  CHECK(kick && ban && refused && left);
}

TEST(admin_trial_replays_under_a_bad_network) {
  gs::AdminTrialConfig c;
  CHECK(gn::conditions_preset("bad", c.link));
  const gs::AdminTrial a = gs::run_admin_trial(c);
  const gs::AdminTrial b = gs::run_admin_trial(c);
  CHECK_MSG(a.ok, said_to(a, "Ana"));
  CHECK(a.digest == b.digest);
}

TEST(admin_no_password_no_remote_console) {
  gs::AdminTrialConfig c;
  c.rcon_password = "";
  const gs::AdminTrial t = gs::run_admin_trial(c);
  CHECK(t.rcon_runs == 0);
  CHECK(!t.bo_kicked && !t.cy_banned);
  CHECK_MSG(said_to(t, "Ana").find("This server has no remote console.") != std::string::npos,
            said_to(t, "Ana"));
}

TEST(admin_ban_words_on_the_console) {
  ge::console::Console con(true);
  std::map<std::string, std::string> files;
  con.set_files(
      [&](std::string_view n, std::string& text) {
        if (!files.count(std::string(n))) return false;
        text = files[std::string(n)];
        return true;
      },
      [&](std::string_view n, std::string_view text) {
        files[std::string(n)] = std::string(text);
        return true;
      });
  gs::ServerConfig c;
  gs::add_server_cvars(con.cvars(), c, gs::sample_game());
  gs::AdminFeatures admin;
  gs::Features f;
  f.list = gs::admin_features(admin);
  gs::features_attach(f, con);
  con.execute("addip 10.0.*.* ban; addip 10.0.0.7 safe; listip");
  CHECK(admin.bans.entries.size() == 2);
  CHECK_MSG(files["listip.cfg"].find("addip 10.0") == 0, files["listip.cfg"]);
  con.execute("removeip 10.0.*.*");
  CHECK(admin.bans.entries.size() == 1);
  CHECK(files["listip.cfg"].find("10.0.*") == std::string::npos);
  // A server started later reads the file back on its first frame.
  gs::AdminFeatures again;
  gs::Features f2;
  f2.list = gs::admin_features(again);
  ge::console::Console con2(true);
  con2.set_files(
      [&](std::string_view n, std::string& text) {
        if (!files.count(std::string(n))) return false;
        text = files[std::string(n)];
        return true;
      },
      nullptr);
  gs::ServerConfig c2;
  gs::add_server_cvars(con2.cvars(), c2, gs::sample_game());
  gs::features_attach(f2, con2);
  gs::features_frame(f2);  // no server yet: the read waits for one
  gs::Server s;
  s.features = &f2;
  std::string error;
  CHECK_MSG(gs::server_open(s, gs::sample_game(), c2, nullptr, nullptr, error), error);
  gs::server_frame(s);
  CHECK(again.bans.entries.size() == 1);
  gs::server_close(s);
}
