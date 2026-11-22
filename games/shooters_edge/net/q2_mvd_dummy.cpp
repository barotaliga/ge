// The definitions behind q2_mvd_dummy.hpp.
//
// ge:summary The recorder's dummy spectator made, seated, run and taken out
// ge:keywords mvdspec dummy command-buffer wait scorecmd
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_mvd_server_test.cpp
#include "shooters_edge/net/q2_mvd_dummy.hpp"

#include <algorithm>
#include <cctype>

namespace shooters_edge::net {

namespace {

constexpr const char* kDummyName = "[MVDSPEC]";
constexpr double kScoreGone = 9.0;  // s without the scoreboard before `sv_mvd_scorecmd` runs
constexpr int kMostLines = 256;     // lines one frame runs (an alias loop stopped)

void say(Q2NetServer& s, const std::string& line) {
  if (s.mvd.log) s.mvd.log(line);
}

// The next line of `buf` (to a `;` or a line end outside quotes), taken off it.
std::string take_line(std::string& buf) {
  bool quoted = false;
  size_t i = 0;
  for (; i < buf.size(); i++) {
    if (buf[i] == '"') quoted = !quoted;
    if (buf[i] == '\n' || (buf[i] == ';' && !quoted)) break;
  }
  std::string line = buf.substr(0, i);
  buf.erase(0, std::min(i + 1, buf.size()));
  return line;
}

std::vector<std::string> words_of(const std::string& line) {
  std::vector<std::string> w;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) i++;
    if (i >= line.size()) break;
    std::string word;
    if (line[i] == '"') {
      const size_t end = line.find('"', i + 1);
      word = line.substr(i + 1, end == std::string::npos ? std::string::npos : end - i - 1);
      i = end == std::string::npos ? line.size() : end + 1;
    } else {
      while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i])))
        word.push_back(line[i++]);
    }
    w.push_back(word);
  }
  return w;
}

std::string trimmed(const std::string& line) {
  const size_t a = line.find_first_not_of(" \t\r");
  if (a == std::string::npos) return "";
  return line.substr(a, line.find_last_not_of(" \t\r") - a + 1);
}

// The slot it takes: past the seats first, else the highest free one; -1 none.
int free_slot(const Q2NetServer& s) {
  for (int i = q2_net_slots(s); i < sh::kMaxClients; i++)
    if (s.clients[i].stage == Q2Slot::free && !(**s.rules).clients[i].active) return i;
  for (int i = q2_net_slots(s); i-- > 0;)
    if (s.clients[i].stage == Q2Slot::free && !(**s.rules).clients[i].active) return i;
  return -1;
}

// Seated in the rules (a level's start too): its begin commands added.
void seat(Q2NetServer& s) {
  Q2Mvd& m = s.mvd;
  s.rules->connect(m.dummy, kDummyName, true, false);
  s.rules->user_info(m.dummy, "skin", "male/grunt");
  if (!m.cfg.begincmd.empty()) m.dummy_cmds += m.cfg.begincmd + "\n";
  m.dummy_layout_at = s.now;
}

// Made (false when it should be and cannot be).
bool make(Q2NetServer& s) {
  Q2Mvd& m = s.mvd;
  if (m.cfg.spawn_dummy <= 0) return true;  // none wanted: the stream names no observer
  const int slot = free_slot(s);
  if (slot < 0) {
    say(s, "No slot for dummy MVD client");
    return false;
  }
  Q2NetClient& c = s.clients[slot];
  c = Q2NetClient{};
  c.stage = Q2Slot::dummy;
  c.name = kDummyName;
  c.spectator = true;
  c.userinfo = "\\name\\[MVDSPEC]\\skin\\male/grunt\\spectator\\1\\mvdspec\\2013\\ip\\loopback";
  m.dummy = slot;
  m.dummy_cmds.clear();
  m.dummy_wait = 0;
  m.dummy_forwarded.clear();
  seat(s);
  return true;
}

void unmake(Q2NetServer& s) {
  Q2Mvd& m = s.mvd;
  if (m.dummy < 0) return;
  if ((**s.rules).clients[m.dummy].active) s.rules->drop(m.dummy);
  s.clients[m.dummy] = Q2NetClient{};
  m.dummy = -1;
  m.dummy_cmds.clear();
  m.dummy_forwarded.clear();
}

// `record` and `stop` from the dummy: an auto-recording the game drives.
void auto_record(Q2NetServer& s, const std::vector<std::string>& w) {
  Q2Mvd& m = s.mvd;
  if (!m.cfg.autorecord) return;
  if (w[0] == "stop") {
    say(s, q2_mvd_stop(m) ? "Stopped local MVD auto-recording." : "Not recording a local MVD.");
    return;
  }
  if (w.size() < 2) return say(s, "Usage: record <filename>");
  if (m.cfg.enable <= 0) return say(s, "MVD recording is disabled on this server.");
  std::string path, out;
  if (!q2_mvd_record(m, w[1], false, path, out)) {
    if (!out.empty() && out.back() == '\n') out.pop_back();
    return say(s, out);
  }
  say(s, "Auto-recording local MVD to " + path);
}

void run_line(Q2NetServer& s, const std::string& raw) {
  Q2Mvd& m = s.mvd;
  const std::string line = trimmed(raw);
  const std::vector<std::string> w = words_of(line);
  if (w.empty()) return;
  const std::string& cmd = w[0];
  if (cmd == "cmd") {
    const std::string rest = trimmed(line.substr(3));
    if (!rest.empty()) s.rules->client_command(m.dummy, rest);
  } else if (cmd == "wait") {
    const int n = w.size() > 1 ? std::atoi(w[1].c_str()) : 1;
    m.dummy_wait += std::max(n, 1);
  } else if (cmd == "play" || cmd == "stopsound" || cmd == "exec" || cmd == "screenshot") {
    // ignored: the dummy plays no sound and runs no files
  } else if (cmd == "record" || cmd == "stop") {
    auto_record(s, w);
  } else if (cmd == "set" || cmd == "alias" || (**s.rules).cvars.find(cmd) != nullptr) {
    s.rules->console(line);
  } else {
    s.rules->client_command(m.dummy, line);
  }
}

}  // namespace

void q2_mvd_dummy_before(Q2NetServer& s, bool level, sv::Mvd2Capture& cap) {
  Q2Mvd& m = s.mvd;
  cap.flags = m.cfg.capture_flags;
  cap.no_msgs = m.cfg.nomsgs;
  cap.dummy_stuff = m.cfg.allow_stufftext > 0;
  if (level && m.cfg.autorecord && m.cfg.enable > 0) {  // the game may record through it
    m.cfg.suspend_min = 0;
    q2_mvd_switch_on(m, s.now);
  }
  if (!m.on) {
    m.dummy_failed = false;
  } else if (m.dummy < 0 && !m.dummy_failed) {
    if (!make(s)) {
      m.dummy_failed = true;
      q2_mvd_dummy_refused(m);
    }
  } else if (m.dummy >= 0 && level) {
    seat(s);
  }
  cap.dummy = m.dummy;
  if (m.dummy < 0 || level) return;
  for (const std::string& line : m.dummy_forwarded) s.rules->client_command(m.dummy, line);
  m.dummy_forwarded.clear();
  for (int k = 0; k < kMostLines && m.dummy_wait == 0 && !m.dummy_cmds.empty(); k++)
    run_line(s, take_line(m.dummy_cmds));
  if (m.dummy_wait > 0) m.dummy_wait--;
  if (m.active && !m.cfg.scorecmd.empty() && s.now - m.dummy_layout_at > kScoreGone) {
    m.dummy_cmds += m.cfg.scorecmd + "\n";
    m.dummy_layout_at = s.now;
  }
}

void q2_mvd_dummy_after(Q2NetServer& s) {
  Q2Mvd& m = s.mvd;
  if (m.dummy < 0) return;
  if (!m.on) return unmake(s);
  if (m.live.dummy_layout) m.dummy_layout_at = s.now;
  for (const std::string& line : m.live.dummy_lines) m.dummy_cmds += line + "\n";
  m.live.dummy_lines.clear();
}

}  // namespace shooters_edge::net
