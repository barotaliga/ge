// Quake II server's administrator words and remote console (q2_admin.hpp).
//
// ge:summary Quake II server's administrator words and its remote console
// ge:keywords q2 admin rcon status kick dumpuser bans
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_admin_test.cpp
#include "shooters_edge/net/q2_admin.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "classic/protocol/info.hpp"
#include "server/admission.hpp"
#include "server/bans.hpp"
#include "server/lrcon.hpp"
#include "server/operator.hpp"
#include "shooters_edge/net/q2_net.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;

// A line in the server's log (kept to its first 512, as the endpoint's own).
void note(Q2NetServer& s, std::string line) {
  if (s.log.size() < 512) s.log.push_back(std::move(line));
}

int64_t unix_now(const Q2NetServer& s) { return s.cfg.unix_time ? s.cfg.unix_time() : 0; }

// Every client on the server whose address the list now refuses, dropped.
void drop_refused(Q2NetServer& s) {
  for (int i = 0; i < q2_net_slots(s); i++) {
    const Q2NetClient& c = s.clients[i];
    if (!q2_held(c)) continue;
    if (gs::ban_refuses(s.bans, s.cfg.ban_mode, c.address, unix_now(s)))
      q2_net_drop(s, i, "banned");
  }
}

}  // namespace

namespace {

std::string row(const char* format, ...) __attribute__((format(printf, 1, 2)));
std::string row(const char* format, ...) {
  char b[256];
  va_list args;
  va_start(args, format);
  std::vsnprintf(b, sizeof b, format, args);  // NOLINT(clang-analyzer-valist.Uninitialized)
  va_end(args);
  return b;
}

// The client settings a later protocol's client asked for, as `status s` shows them: G no gun,
// B no blend, R recording, I no gibs, F no footsteps, P no prediction, L no flares.
std::string options(const Q2NetClient& c) {
  static constexpr struct {
    int index;
    char letter;
  } kShown[] = {{kQ2SetNoGun, 'G'},   {kQ2SetNoBlend, 'B'},     {kQ2SetRecording, 'R'},
                {kQ2SetNoGibs, 'I'},  {kQ2SetNoFootsteps, 'F'}, {kQ2SetNoPredict, 'P'},
                {kQ2SetNoFlares, 'L'}};
  std::string o;
  for (const auto& k : kShown)
    if (c.asked[k.index] != 0) o += k.letter;
  return o;
}

// A client's ping word: ZMBI dropped, ASGN before its first `new`, DNLD sending it a file, HTTP
// fetching from the web, CNCT signing on, else its ping (at most 9999).
std::string ping_word(const Q2NetClient& c) {
  if (c.stage == Q2Slot::zombie) return "ZMBI";
  if (c.stage == Q2Slot::signon) {
    if (!c.newed) return "ASGN";
    if (c.downloading) return "DNLD";
    if (c.http_download) return "HTTP";
    return "CNCT";
  }
  return std::to_string(std::min(c.ping_ms, 9999));
}

std::vector<gs::PickSlot> pick_slots(const Q2NetServer& s) {
  std::vector<gs::PickSlot> slots;
  slots.reserve(static_cast<size_t>(q2_net_slots(s)));
  for (int i = 0; i < q2_net_slots(s); i++)
    slots.push_back({s.clients[i].name, q2_held(s.clients[i])});
  return slots;
}

}  // namespace

std::string q2_status_table(const Q2NetServer& s, char letter) {
  std::string t = "Current map: " + (*s.rules)->map_name + "\n\n";
  bool any = false;
  for (int i = 0; i < q2_net_slots(s); i++) any = any || s.clients[i].stage != Q2Slot::free;
  if (!any) return t + "No UDP clients.\n\n";
  static const char* const kHead[][2] = {
      {"num score ping name            lastmsg address                rate pr fps",
       "--- ----- ---- --------------- ------- --------------------- ----- -- ---"},
      {"num name            download                                 size    done",
       "--- --------------- ---------------------------------------- ------- ----"},
      {"num name            PLs2c PLc2s Rmin Ravg Rmax dup scale",
       "--- --------------- ----- ----- ---- ---- ---- --- -----"},
      {"num name            major minor msglen zlib chan",
       "--- --------------- ----- ----- ------ ---- ----"},
      {"num name            proto options upd fps", "--- --------------- ----- ------- --- ---"},
      {"num name            idle time", "--- --------------- ---- --------"},
      {"num name            version",
       "--- --------------- -----------------------------------------"}};
  static const char kLetters[] = " dlpstv";
  int kind = 0;
  for (int k = 1; k < 7; k++)
    if (letter == kLetters[k]) kind = k;
  t += std::string(kHead[kind][0]) + "\n" + kHead[kind][1] + "\n";
  for (int i = 0; i < q2_net_slots(s); i++) {
    const Q2NetClient& c = s.clients[i];
    if (c.stage == Q2Slot::free) continue;
    const char* name = c.name.c_str();
    switch (kind) {
      case 0: {
        const int frags = static_cast<int>((*s.rules)->world.clients[i].numbers.frags);
        const int last = static_cast<int>((s.now - c.heard) * 1000.0);
        t += row("%3d %5d %4s %-15.15s %7d %-21s %5d %2d %3d\n", i, frags, ping_word(c).c_str(),
                 name, last, gn::to_string(c.address).c_str(), c.rate, kQ2NetProtocol, c.move_fps);
        break;
      }
      case 1:
        if (c.downloading)
          t += row(
              "%3d %-15.15s %-40.40s %-7d %3d%%\n", i, name, c.download_name.c_str(),
              static_cast<int>(c.download.size()),
              c.download.empty() ? 0 : static_cast<int>(c.download_at * 100 / c.download.size()));
        else if (c.http_download)
          t += row("%3d %-15.15s %-40.40s %-7d %3d%%\n", i, name, "<HTTP download>", 0, 0);
        break;
      case 2:
        t += row("%3d %-15.15s %5.2f %5.2f %4d %4d %4d %3d %5.3f\n", i, name, q2_loss_down(c),
                 q2_loss_up(c), c.rtt_min, q2_rtt_avg(c), c.rtt_max, c.packet_dup, c.scale);
        break;
      case 3:
        t += row("%3d %-15.15s %5d %5d %6d  %s  %s\n", i, name, c.protocol, c.minor, c.msglen,
                 c.zlib ? "yes" : "no ", c.ch.fragments ? "new" : "old");
        break;
      case 4:
        t += row("%3d %-15.15s %5d %7s %3d %3d\n", i, name, c.protocol, options(c).c_str(),
                 c.asked[kQ2SetPlayerUpdates],
                 c.protocol == kQ2NetProtocol ? 0 : c.asked[kQ2SetFps]);
        break;
      case 5: {
        const int idle = std::min(
            9999, static_cast<int>(c.stage == Q2Slot::active ? s.now - c.idle.active_at : 0));
        t += row("%3d %-15.15s %4d %s\n", i, name, idle,
                 q2_span_text(c.new_at < 0 ? s.now : s.now - c.new_at).c_str());
        break;
      }
      default:
        t += row("%3d %-15.15s %s\n", i, name,
                 c.version_heard ? c.version.substr(0, 52).c_str() : "-");
    }
  }
  return t + "\n";
}

int q2_find_slot(const Q2NetServer& s, const std::string& word, std::string& error) {
  return gs::pick_player(pick_slots(s), word, s.cfg.enhanced_setplayer, error);
}

bool q2_admin(Q2NetServer& s, const std::string& line, std::string& out) {
  const std::vector<std::string> w = q2_words(line);
  if (w.empty()) return false;
  const std::string& cmd = w[0];
  if (gs::lock_command(line, s.cfg.locked, out)) return true;
  // The helpers' list, in our words or the modern server's.
  static const char* const kLrconWords[3] = {"addlrconcmd", "dellrconcmd", "listlrconcmds"};
  if (gs::lrcon_command(s.cfg.lrcon, line, out, &kLrconWords)) return true;
  if (gs::stuff_list_command(s.stuff_lists, line, out)) return true;
  if (cmd == "status") {
    const char letter = w.size() > 1 && !w[1].empty() ? w[1][0] : ' ';
    static const std::string kKnown = "dlpstv";
    out = letter != ' ' && kKnown.find(letter) == std::string::npos
              ? "Usage: status [d|l|p|s|t|v]\n" + q2_status_table(s, ' ')
              : q2_status_table(s, letter);
    if (s.mvd.cfg.enable >= 2) out += q2_mvd_status(s.mvd, letter != ' ', s.now) + "\n";
    return true;
  }
  if (q2_mvd_command(s.mvd, w, line, s.rules != nullptr && !s.killed, out)) return true;
  if (cmd == "stuffall" || cmd == "printall") {
    if (w.size() < 2) {
      out = "Usage: " + cmd + " <raw text>\n";
      return true;
    }
    const std::string text = gs::line_rest(line, 1);
    for (int i = 0; i < q2_net_slots(s); i++) {
      if (!q2_held(s.clients[i])) continue;
      if (cmd == "stuffall")
        q2_net_stuff(s, i, text);
      else
        q2_net_print(s, i, text + "\n");
    }
    if (cmd == "printall") out = text + "\n";
    return true;
  }
  if (cmd == "kick" || cmd == "kickban" || cmd == "dumpuser" || cmd == "stuff" ||
      cmd == "stuffcvar") {
    const bool more = cmd == "stuff" || cmd == "stuffcvar";
    if (more ? w.size() < 3 : w.size() != 2) {
      out = "Usage: " + cmd +
            (cmd == "stuff"       ? " <userid> <raw text>\n"
             : cmd == "stuffcvar" ? " <userid> <variable> [...]\n"
                                  : " <userid>\n");
      return true;
    }
    std::string error;
    const int slot = q2_find_slot(s, w[1], error);
    if (slot < 0) {
      out = error;
      return true;
    }
    Q2NetClient& c = s.clients[slot];
    if (cmd == "stuff") {
      q2_net_stuff(s, slot, gs::line_rest(line, 2));
      return true;
    }
    if (cmd == "stuffcvar") {
      for (size_t i = 2; i < w.size(); i++) q2_net_ask_setting(s, slot, w[i]);
      return true;
    }
    if (cmd == "dumpuser") {
      out = "\nuserinfo\n--------\n";
      std::vector<std::pair<std::string, std::string>> kv;
      ge::classic::protocol::info_pairs(c.userinfo, kv);
      for (auto& [key, value] : kv)
        out += row("%-20s %s\n", key.empty() ? "<MISSING KEY>" : key.c_str(),
                   value.empty() ? "<MISSING VALUE>" : value.c_str());
      out += "\nmiscinfo\n--------\n" + q2_misc_lines(s, slot);
      return true;
    }
    if (cmd == "kickban" && (c.address.family == gn::Family::ipv4 ||
                             c.address.family == gn::Family::sim)) {  // banned until it stops
      uint8_t ip[4] = {};
      if (c.address.family == gn::Family::sim)
        gn::sim_to_ipv4(c.address, ip);
      else
        std::copy(c.address.bytes, c.address.bytes + 4, ip);
      std::string said;
      gs::ban_command(s.bans, row("addip %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]), unix_now(s),
                      said);
    }
    out = c.name + "[" + gn::to_string(c.address) + "] was kicked\n";
    q2_net_kick(s, slot, false);
    return true;
  }
  if (cmd == "killserver") {
    q2_net_shutdown(s, "Server was killed.\n", false);
    s.killed = true;
    return true;
  }
  if (cmd == "pickclient") {
    if (s.cfg.max_clients == 1) {
      out = "Single player server running.\n";
    } else if (w.size() < 2) {
      out = "Usage: pickclient <address>\n";
    } else {
      gn::Address to;
      if (!gn::parse_address(w[1], to))
        out = "Bad client address: " + w[1] + "\n";
      else if (w[1].find(':') == std::string::npos)
        out = "Please specify client port explicitly.\n";
      else
        q2_net_oob(s, to, "passive_connect\n");
    }
    return true;
  }
  static const char* const kFilterWords[3] = {"addfiltercmd", "delfiltercmd", "listfiltercmds"};
  if (gs::cmdfilter_command(s.cmd_filters, line, out, &kFilterWords)) return true;
  // Setting and user info bans (server/settingban.hpp), by our words or the game's own.
  static const char* const kCvarWords[3] = {"addcvarban", "delcvarban", "listcvarbans"};
  static const char* const kInfoWords[3] = {"adduserinfoban", "deluserinfoban", "listuserinfobans"};
  if (gs::setting_ban_command(s.cvar_bans, "cvarban", line, out, &kCvarWords)) return true;
  if (gs::setting_ban_command(s.userinfo_bans, "userinfoban", line, out, &kInfoWords)) return true;
  if (cmd == "cvarban_check" || cmd == "checkcvarbans") {
    out = s.cvar_bans.entries.empty() ? "No cvarbans.\n" : "";
    if (!out.empty()) return true;
    int asked = 0;
    for (int i = 0; i < q2_net_slots(s); i++)
      if (s.clients[i].stage == Q2Slot::active) {
        q2_net_ask_settings(s, i);
        asked++;
      }
    out = "Asked " + std::to_string(asked) + " players for their settings.\n";
    return true;
  }
  // The blackhole list (server/bans.hpp), by our words or the game's own.
  static const std::pair<const char*, const char*> kHoleWords[] = {
      {"addblackhole", "blackhole_addip"},
      {"delblackhole", "blackhole_removeip"},
      {"listblackholes", "blackhole_listip"}};
  std::string hole_line = line;
  for (const auto& [theirs, ours] : kHoleWords)
    if (cmd == theirs) hole_line = ours + line.substr(line.find(cmd) + cmd.size());
  if (gs::blackhole_command(s.blackholes, hole_line, unix_now(s), out)) return true;
  if (gs::ban_command(s.bans, line, unix_now(s), out)) {
    if (cmd == "writeip") out = gs::ban_file_write(s.bans, s.cfg.ban_file, out);
    if (cmd == "addip") drop_refused(s);
    return true;
  }
  return false;
}

void q2_rcon(Q2NetServer& s, const gn::Address& from, const std::string& text) {
  if (gs::ban_refuses(s.bans, s.cfg.ban_mode, from, unix_now(s))) {
    s.rcon_refused++;  // a banned address is not answered
    return;
  }
  if (!gs::limit_take(s.rcon_tries, s.now, s.cfg.rcon_rate)) {
    s.rcon_refused++;  // past sv_rconrate: not answered
    return;
  }
  const std::vector<std::string> w = q2_words(text);
  // The helper's password (server/lrcon.hpp): only the lines the administrator allowed.
  const bool limited = !s.cfg.lrcon.password.empty() && w.size() >= 2 &&
                       w[1] == s.cfg.lrcon.password && w[1] != s.cfg.rcon_password;
  if (!limited && (s.cfg.rcon_password.empty() || w.size() < 2 || w[1] != s.cfg.rcon_password)) {
    s.rcon_refused++;
    note(s, "bad rcon from " + gn::to_string(from));
    q2_net_oob(s, from, "print\nBad rcon_password.\n");
    return;
  }
  gs::limit_give(s.rcon_tries, s.cfg.rcon_rate);  // a good password gives its try back
  // The line: the words after the password joined by single spaces.
  std::string line;
  for (size_t i = 2; i < w.size(); i++) line += (i > 2 ? " " : "") + w[i];
  if (limited && !gs::lrcon_allows(s.cfg.lrcon, line)) {
    note(s, "limited rcon refused from " + gn::to_string(from) + ": " + line);
    q2_net_oob(s, from, std::string("print\n") + gs::kLrconRefusal);
    return;
  }
  s.rcon_runs++;
  note(s, (limited ? "limited rcon from " : "rcon from ") + gn::to_string(from) + ": " + line);
  std::string answer;
  if (!line.empty() && !q2_admin(s, line, answer)) {
    const sv::RulesCommon& game = **s.rules;
    const size_t before = game.console.size();
    if (!s.rules->console(line) && game.console.size() == before)
      answer = "Unknown command \"" + w[2] + "\"\n";
    for (size_t i = before; i < game.console.size(); i++) answer += game.console[i] + "\n";
  }
  q2_net_oob(s, from, "print\n" + answer);
}

}  // namespace shooters_edge::net
