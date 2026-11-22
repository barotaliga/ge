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
bool joined_any(const Remote& r) { return r.active && !r.watch; }

// The newest level's first byte in the level kept, the joined game's.
size_t* level_from(Remote& r) {
  if (r.q2 != nullptr) return &r.q2->level_from;
  return &r.end.level_from;
}

fs::path temp_file(const fs::path& dir) { return dir / "temp" / "_match_.qwd"; }

// The waiting recording moved to its name; the line said.
std::string save_match(MatchAuto& m) {
  m.waiting = false;
  const std::string stem = client::free_match_file(m.dir, m.name, {"qwd", "qwz", "mvd", "dm2"});
  if (stem.empty()) return "Every name from " + m.name + "_000 to _999 is taken; nothing saved.";
  const fs::path to = m.dir / (stem + ".qwd");
  std::error_code ec;
  fs::create_directories(to.parent_path(), ec);
  fs::rename(temp_file(m.dir), to, ec);
  if (ec)
    return "Couldn't save the match's recording as " + to.string() + " (" + ec.message() + ").";
  return "Match recording saved to " + stem + ".qwd.";
}

// The waiting log written to its name; the line said.
std::string save_log(MatchAuto& m) {
  m.log_waiting = false;
  const std::string stem = client::free_match_file(m.dir, m.name, {"log"});
  std::string text;
  text.swap(m.log);
  if (stem.empty())
    return "Every log name from " + m.name + "_000 to _999 is taken; nothing saved.";
  const fs::path to = m.dir / (stem + ".log");
  std::error_code ec;
  fs::create_directories(to.parent_path(), ec);
  std::ofstream out(to, std::ios::binary | std::ios::trunc);
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (!out) return "Couldn't write the match's console log to " + to.string() + ".";
  return "Match console log saved to " + stem + ".log.";
}

// The file closed, nothing said.
void close_file(Remote& r) {
  if (r.file.path.empty()) return;
  remote_record_flush(r);
  if (r.file.q2) {  // Quake II's recording ends with a block of length -1
    const char end[4] = {'\xff', '\xff', '\xff', '\xff'};
    r.file.out.write(end, 4);
  }
  r.file.out.close();
  r.file.path.clear();
}

// The console's lines kept from now on (the console's tap).
void start_log(MatchAuto& m, con::Console& c) {
  m.log.clear();
  m.log_waiting = false;
  m.logging = true;
  c.set_tap([&m](std::string_view line) {
    m.log.append(line);
    m.log.push_back('\n');
  });
}

void stop_log(MatchAuto& m, con::Console& c) {
  if (!m.logging) return;
  m.logging = false;
  c.set_tap(nullptr);
}

// The match ended as a match ends (`cut`: cut short, and saved by its length).
void end_match(Remote& r, con::Console& c, double now_s, bool cut) {
  MatchAuto& m = r.match;
  if (!m.running) return;
  if (cut && m.end_at_s >= 0) cut = false;  // its scores are in: an end
  m.running = false;
  m.end_at_s = -1;
  const bool short_cut = cut && now_s - m.began_s <= setting(c, "match_auto_minlength", 30);
  const bool logged = m.logging;
  stop_log(m, c);
  if (m.recording) {
    m.recording = false;
    close_file(r);
    m.waiting = true;
    if (m.mode == 1) {
      c.print("The match's recording is complete; type match_save to keep it.");
    } else if (short_cut) {
      c.print("The match's recording was cut short, so it isn't saved (match_save keeps it).");
    } else {
      c.print(save_match(m));
    }
  }
  if (logged) {
    m.log_waiting = true;
    if (m.log_mode == 2 && short_cut) {
      c.print("The match's console log was cut short, so it isn't saved (match_save keeps it).");
    } else if (m.log_mode == 2) {
      c.print(save_log(m));
    }
  }
}

void start_match(Remote& r, con::Console& c, double now_s) {
  MatchAuto& m = r.match;
  if (m.running) end_match(r, c, now_s, true);
  if (r.end.cfg.spectator && setting(c, "match_auto_spectating", 0) == 0) return;
  const int mode = setting(c, "match_auto_record", 0);
  const int log_mode = setting(c, "match_auto_logconsole", 1);
  if (mode == 0 && log_mode == 0) return;
  if (!r.joined_record.folder) return;
  const fs::path dir = r.joined_record.folder();
  if (dir.empty()) return;
  m.dir = dir;
  m.name = r.joined_record.match_name();
  m.mode = mode;
  m.log_mode = log_mode;
  m.began_s = now_s;
  m.end_at_s = -1;
  m.level = r.levels;
  if (mode != 0 && !r.file.path.empty()) {
    c.print("The match isn't recorded on its own: you are recording already.");
  } else if (mode != 0) {
    std::string error;
    if (remote_record_open(r, temp_file(m.dir), error)) {
      m.recording = true;
      m.waiting = false;
      c.print("Recording the match.");
    } else {
      c.print(error);
    }
  }
  if (log_mode != 0) {
    c.print("Logging the match's console.");
    start_log(m, c);
  }
  m.running = m.recording || m.logging;
}

}  // namespace

client::MatchFacts match_facts(const Remote& r) {
  client::MatchFacts f;
  const QwClientEnd& e = r.end;
  if (r.q2) {  // Quake II's: the players' names as the recording says them
    for (const std::string& name : r.demo.state.names) f.players.push_back({name, "", false});
    f.me = r.demo.state.player;
    f.map = r.demo.state.map;
    f.gamedir = "baseq2";
  }
  for (const std::string& u : e.userinfos) {
    if (r.q2) break;
    client::NamedPlayer p;
    p.name = info_get(u, "name");
    p.team = info_get(u, "team");
    p.spectator = !info_get(u, "*spectator").empty() && info_get(u, "*spectator") != "0";
    f.players.push_back(p);
  }
  if (!r.q2) {
    f.me = e.playernum;
    f.spectating = e.cfg.spectator;
    f.map = r.demo.state.map;
    f.gamedir = e.gamedir.empty() ? "qw" : e.gamedir;
  }
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
  if (!joined_any(r)) {
    error = "Join a server first.";
    return false;
  }
  close_file(r);
  r.file.q2 = r.q2 != nullptr;
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
  if (r.match.recording) {
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

void remote_record_restart(Remote& r) {
  if (!r.file.path.empty()) {  // a recording of the last server ends with it
    remote_record_flush(r);
    r.file.out.close();
    r.file.path.clear();
  }
  r.file.level = ClientRecorder{};
  record_start(r.file.level, false);
  r.file.written = 0;
  r.end.level_record = r.q2 ? nullptr : &r.file.level;
  r.end.level_from = 0;
  if (r.q2) {
    r.q2->level_record = &r.file.level.bytes;
    r.q2->level_from = 0;
  }
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
  if (r.file.q2 != (r.q2 != nullptr) && !f.path.empty()) return;  // the game it was begun in left
  size_t& at = *level_from(r);
  const size_t from = std::min(at, b.size());  // the older levels let go
  if (from == 0) return;
  b.erase(b.begin(), b.begin() + static_cast<std::ptrdiff_t>(from));
  f.written = f.written > from ? f.written - from : 0;
  at = 0;
}

// match_auto_sshot: at an intermission's start, the screenshot set for three frames on.
void match_shot(Remote& r, con::Console& c, bool intermission) {
  MatchAuto& m = r.match;
  if (intermission && !m.intermission && setting(c, "match_auto_sshot", 0) != 0 &&
      r.joined_record.screenshot) {
    const client::MatchFacts f = match_facts(r);
    const std::string type = client::match_type(f);
    bool kind = false;
    for (const char* k : {"duel", "ffa", "2on2", "3on3", "4on4", "tdm", "multiteam", "arena",
                          "tfduel", "tfclanwar"})
      kind = kind || type == k;
    int others = 0;
    for (size_t i = 0; i < f.players.size(); i++)
      if (!f.players[i].spectator && static_cast<int>(i) != f.me) others++;
    if (kind && others == 0) {
      c.print("Auto screenshot cancelled.");
    } else if (kind) {
      m.shot_name = m.running ? m.name : r.joined_record.match_name();
      m.shot_frames = 3;
    }
  }
  m.intermission = intermission;
  if (m.shot_frames <= 0) return;
  m.shot_frames--;
  if (m.shot_frames == 0 && intermission) {
    std::string why;
    if (!r.joined_record.screenshot(m.shot_name, why)) c.print(why);
  }
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
  if (!r.rerecord.empty()) {  // `rerecord`: the join just started, recorded from its first byte
    std::string error;
    if (remote_record_open(r, r.rerecord, error))
      c.print("recording to " + r.rerecord.string() + ".");
    else
      c.print("ERROR: couldn't open.");
    r.rerecord.clear();
  }
  if (m.level != r.levels) {  // a new level: the old one's match is over
    if (m.running) end_match(r, c, now_s, true);
    m.standby = false;
    m.level = r.levels;
  }
  if (m.running && m.end_at_s >= 0 && now_s >= m.end_at_s) end_match(r, c, now_s, false);
  const bool standby = lower(info_get(r.end.serverinfo, "status")) == "standby";
  const bool intermission = r.demo.state.intermission;
  match_shot(r, c, intermission);
  if (m.running && standby) end_match(r, c, now_s, true);
  if (!m.running && m.standby && !standby && !intermission) start_match(r, c, now_s);
  if (m.running && intermission && m.end_at_s < 0) m.end_at_s = now_s + kMatchEndHoldS;
  m.standby = standby;
}

void add_match_record_commands(con::Console& c, Remote& r) {
  client::add_match_cvars(c);
  client::JoinedRecord& j = r.joined_record;
  j.joined = [&r] { return joined_any(r); };
  j.extension = [&r] { return std::string(r.q2 ? "dm2" : "qwd"); };
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
  j.automatic = [&r] { return r.match.recording; };
  j.match_name = [&r, &c] {
    return client::match_name(match_facts(r), client::match_words(c.cvars()),
                              [&c](const std::string& name) {
                                const con::Cvar* v = c.cvars().find(name);
                                return v != nullptr ? v->value : std::string();
                              });
  };
  // `rerecord <name>`: the recording stopped, the last server joined again and recorded from the
  // start of the join (`<game folder>/<name>`, .qwd added when it has no extension).
  c.add_command(
      "rerecord", "rerecord <name>: join the last server again and record from the start",
      [&r](con::Console& cc, const con::Args& a) {
        if (a.size() != 2) {
          cc.print("rerecord <demoname>");
          return;
        }
        if (r.server_name.empty()) {
          cc.print("No server to reconnect to...");
          return;
        }
        if (r.joined_record.close) {
          const std::string done = r.joined_record.close();
          if (!done.empty()) cc.print(done);
        }
        fs::path file =
            fs::path(r.joined_record.folder ? r.joined_record.folder() : std::string(".")) / a[1];
        if (!file.has_extension()) file += ".qwd";
        r.rerecord = file;
        cc.execute("reconnect");
      });
  c.add_command("match_save",
                "match_save: keep the match recorded on its own (match_auto_record 1)",
                [&r](con::Console& cc, const con::Args&) {
                  MatchAuto& m = r.match;
                  if (m.waiting || m.log_waiting) {
                    cc.print("Saving the match...");
                    if (m.waiting) cc.print(save_match(m));
                    if (m.log_waiting) cc.print(save_log(m));
                  } else {
                    cc.print(m.running ? "The match is still going on." : "Nothing to save.");
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
