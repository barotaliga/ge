// Recording a QuakeWorld server joined (qw_match_record.hpp).
//
// ge:summary The joined game's recording file, the match recorded on its own, their commands
// ge:keywords remote_record_open remote_record_close remote_match_frame match_save match_forcestart
// ge:skill net replay
// ge:test tests/shooters_edge_remote_test.cpp
#include "shooters_edge/net/qw_match_record.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <system_error>

#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/qw_remote.hpp"

namespace shooters_edge::net {

namespace fs = std::filesystem;
namespace con = ge::console;

namespace {

std::string lower(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}

int setting(const con::Console& c, const char* name, int fallback) {
  const con::Cvar* v = c.cvars().find(name);
  return v != nullptr ? std::atoi(v->value.c_str()) : fallback;
}

bool joined_qw(const Remote& r) { return r.active && !r.watch && !r.q2; }

fs::path temp_file(const fs::path& dir) { return dir / "temp" / "_match_.qwd"; }

// The waiting recording moved to its name; the line said.
std::string save_match(MatchAuto& m) {
  m.waiting = false;
  const std::string stem = client::free_match_file(m.dir, m.name, {"qwd", "qwz", "mvd"});
  if (stem.empty()) return "Every name from " + m.name + "_000 to _999 is taken; nothing saved.";
  const fs::path to = m.dir / (stem + ".qwd");
  std::error_code ec;
  fs::create_directories(to.parent_path(), ec);
  fs::rename(temp_file(m.dir), to, ec);
  if (ec)
    return "Couldn't save the match's recording as " + to.string() + " (" + ec.message() + ").";
  return "Match recording saved to " + stem + ".qwd.";
}

// The file closed, nothing said.
void close_file(Remote& r) {
  if (r.file.path.empty()) return;
  remote_record_flush(r);
  r.file.out.close();
  r.file.path.clear();
}

// The match's recording ended as a match ends (`cut`: cut short, and saved by its length).
void end_match(Remote& r, con::Console& c, double now_s, bool cut) {
  MatchAuto& m = r.match;
  if (!m.running) return;
  if (cut && m.end_at_s >= 0) cut = false;  // its scores are in: an end
  m.running = false;
  m.end_at_s = -1;
  close_file(r);
  m.waiting = true;
  if (m.mode == 1) {
    c.print("The match's recording is complete; type match_save to keep it.");
    return;
  }
  if (cut && now_s - m.began_s <= setting(c, "match_auto_minlength", 30)) {
    c.print("The match's recording was cut short, so it isn't saved (match_save keeps it).");
    return;
  }
  c.print(save_match(m));
}

void start_match(Remote& r, con::Console& c, double now_s) {
  MatchAuto& m = r.match;
  if (m.running) end_match(r, c, now_s, true);
  if (r.end.cfg.spectator && setting(c, "match_auto_spectating", 0) == 0) return;
  const int mode = setting(c, "match_auto_record", 0);
  if (mode == 0) return;
  if (!r.file.path.empty()) {
    c.print("The match isn't recorded on its own: you are recording already.");
    return;
  }
  if (!r.joined_record.folder) return;
  m.dir = r.joined_record.folder();
  if (m.dir.empty()) return;
  m.name = r.joined_record.match_name();
  m.mode = mode;
  m.began_s = now_s;
  m.end_at_s = -1;
  m.level = r.levels;
  std::string error;
  if (!remote_record_open(r, temp_file(m.dir), error)) {
    c.print(error);
    return;
  }
  m.running = true;
  m.waiting = false;
  c.print("Recording the match.");
}

}  // namespace

client::MatchFacts match_facts(const Remote& r) {
  client::MatchFacts f;
  const QwClientEnd& e = r.end;
  for (const std::string& u : e.userinfos) {
    client::NamedPlayer p;
    p.name = info_get(u, "name");
    p.team = info_get(u, "team");
    p.spectator = !info_get(u, "*spectator").empty() && info_get(u, "*spectator") != "0";
    f.players.push_back(p);
  }
  f.me = e.playernum;
  f.spectating = e.cfg.spectator;
  f.map = r.demo.state.map;
  f.gamedir = e.gamedir.empty() ? "qw" : e.gamedir;
  const auto num = [&](const char* key, int fallback) {
    const std::string v = info_get(e.serverinfo, key);
    return v.empty() ? fallback : std::atoi(v.c_str());
  };
  f.teamplay = num("teamplay", 0);
  f.timelimit = num("timelimit", 0);
  f.fraglimit = num("fraglimit", 0);
  f.deathmatch = num("deathmatch", 1);
  f.race = !f.map.empty() && info_get(e.serverinfo, "race") == f.map;
  f.fortress = lower(f.gamedir) == "fortress";
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  f.year = local.tm_year + 1900;
  f.month = local.tm_mon + 1;
  f.day = local.tm_mday;
  f.hour = local.tm_hour;
  f.minute = local.tm_min;
  f.second = local.tm_sec;
  return f;
}

bool remote_record_open(Remote& r, const fs::path& file, std::string& error) {
  if (!joined_qw(r)) {
    error = "Join a QuakeWorld server first: only its games are recorded while joined.";
    return false;
  }
  close_file(r);
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  r.file.out.open(file, std::ios::binary | std::ios::trunc);
  if (!r.file.out) {
    error = "Couldn't write " + file.string() + ". Check that the folder is writable.";
    return false;
  }
  r.file.path = file;
  r.file.written = 0;  // the level so far goes first
  r.file.bytes = 0;
  remote_record_flush(r);
  return true;
}

std::string remote_record_close(Remote& r, con::Console& c, double now_s) {
  if (r.match.running) {
    end_match(r, c, now_s, false);
    return " ";  // said by the match's end
  }
  if (r.file.path.empty()) return "";
  const std::string path = r.file.path.string();
  close_file(r);
  const long kib = std::lround(static_cast<double>(r.file.bytes) / 1024);
  return "Recorded " + path + " (" + std::to_string(kib) + " KiB). Play it with playdemo " + path +
         ".";
}

void remote_record_flush(Remote& r) {
  JoinedFile& f = r.file;
  std::vector<uint8_t>& b = f.level.bytes;
  if (!f.path.empty() && f.written < b.size()) {
    f.out.write(reinterpret_cast<const char*>(b.data() + f.written),
                static_cast<std::streamsize>(b.size() - f.written));
    f.out.flush();
    f.bytes += static_cast<int64_t>(b.size() - f.written);
    f.written = b.size();
  }
  const size_t from = std::min(r.end.level_from, b.size());  // the older levels let go
  if (from == 0) return;
  b.erase(b.begin(), b.begin() + static_cast<std::ptrdiff_t>(from));
  f.written = f.written > from ? f.written - from : 0;
  r.end.level_from = 0;
}

void remote_match_frame(Remote& r, con::Console& c, double now_s) {
  MatchAuto& m = r.match;
  if (!joined_qw(r)) {
    if (m.running) end_match(r, c, now_s, true);
    if (!r.file.path.empty()) close_file(r);
    m.standby = false;
    m.level = -1;
    return;
  }
  if (m.level != r.levels) {  // a new level: the old one's match is over
    if (m.running) end_match(r, c, now_s, true);
    m.standby = false;
    m.level = r.levels;
  }
  if (m.running && m.end_at_s >= 0 && now_s >= m.end_at_s) end_match(r, c, now_s, false);
  const bool standby = lower(info_get(r.end.serverinfo, "status")) == "standby";
  const bool intermission = r.demo.state.intermission;
  if (m.running && standby) end_match(r, c, now_s, true);
  if (!m.running && m.standby && !standby && !intermission) start_match(r, c, now_s);
  if (m.running && intermission && m.end_at_s < 0) m.end_at_s = now_s + kMatchEndHoldS;
  m.standby = standby;
}

void add_match_record_commands(con::Console& c, Remote& r) {
  client::add_match_cvars(c);
  client::JoinedRecord& j = r.joined_record;
  j.joined = [&r] { return joined_qw(r); };
  j.open = [&r](const fs::path& file, std::string& error) {
    return remote_record_open(r, file, error);
  };
  const auto wall = [] {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
  };
  j.close = [&r, &c, wall] {
    const std::string line = remote_record_close(r, c, wall());
    return line == " " ? std::string() : line;
  };
  j.path = [&r] { return r.file.path.string(); };
  j.automatic = [&r] { return r.match.running; };
  j.match_name = [&r, &c] {
    return client::match_name(match_facts(r), client::match_words(c.cvars()),
                              [&c](const std::string& name) {
                                const con::Cvar* v = c.cvars().find(name);
                                return v != nullptr ? v->value : std::string();
                              });
  };
  c.add_command(
      "match_save", "match_save: keep the match recorded on its own (match_auto_record 1)",
      [&r](con::Console& cc, const con::Args&) {
        if (r.match.waiting) {
          cc.print("Saving the match...");
          cc.print(save_match(r.match));
        } else {
          cc.print(r.match.running ? "The match is still being recorded." : "Nothing to save.");
        }
      });
  c.add_command("match_forcestart",
                "match_forcestart: start the match's own recording now, as if it had begun",
                [&r, wall](con::Console& cc, const con::Args& a) {
                  if (a.size() > 1) {
                    cc.print("match_forcestart takes no words after it.");
                    return;
                  }
                  if (!joined_qw(r)) {
                    cc.print("You must be connected to a server to start a match by hand.");
                    return;
                  }
                  start_match(r, cc, wall());
                });
}

}  // namespace shooters_edge::net
