// The feature seam's dispatch (feature.hpp).
//
// ge:summary The feature seam's dispatch: session events to hooks, answers, admit and chat filter
// ge:keywords server feature hook dispatch features_frame feature_answer
// ge:skill net
// ge:test tests/server_feature_test.cpp
#include "server/feature.hpp"

#include <cstdlib>

#include "console/lex.hpp"
#include "server/server.hpp"

namespace ge::server {

namespace gn = ge::net;

void features_attach(Features& f, console::Console& con) {
  f.console = &con;
  for (Feature& x : f.list)
    if (x.attach) x.attach(x.self, f);
}

void feature_answer(Features& f, int player, std::string_view text) {
  size_t at = 0;
  while (at <= text.size()) {
    size_t nl = text.find('\n', at);
    if (nl == std::string_view::npos) nl = text.size();
    const std::string_view line = text.substr(at, nl - at);
    if (!(line.empty() && nl == text.size() && at > 0)) {
      if (player == gn::kNoPlayer) {
        if (f.console) f.console->print(line);
      } else if (f.server && f.server->networked && !f.server->own_protocol) {
        gn::session_host_print(f.server->session, player, line.data(), line.size());
      }
    }
    at = nl + 1;
  }
}

int feature_player(const Features& f, std::string_view word) {
  if (!f.server || word.empty()) return gn::kNoPlayer;
  const gn::SessionHost& h = f.server->session;
  char* end = nullptr;
  const std::string w(word);
  const long seat = std::strtol(w.c_str(), &end, 10);
  if (end && *end == 0 && seat >= 0 && seat < h.config.max_players && h.players[seat].used)
    return static_cast<int>(seat);
  for (int s = 0; s < h.config.max_players; s++)
    if (h.players[s].used && word == h.players[s].name) return s;
  return gn::kNoPlayer;
}

namespace {

void run_command(Features& f, int player, std::string_view line) {
  f.commands++;
  const std::vector<std::string> words = console::tokenize(line);
  if (words.empty()) return;
  for (Feature& x : f.list)
    if (x.command && x.command(x.self, f, player, words, line)) return;
  f.unknown++;
  feature_answer(f, player, "Unknown command \"" + words[0] + "\".");
}

}  // namespace

void features_frame(Features& f) {
  Server* s = f.server;
  if (s == nullptr) return;
  if (s->networked && !s->own_protocol) {
    const gn::SessionHost& h = s->session;
    // The events are the session's own until its next update. A hook may act on the session (a
    // kick adds a `left`), so each event is copied out and the list read to its end as it grows:
    // every feature hears of the seat its own act freed.
    for (size_t i = 0; i < h.events.size(); i++) {
      const gn::SessionEvent e = h.events[i];
      for (Feature& x : f.list) {
        switch (e.type) {
          case gn::SessionEventType::joined:
          case gn::SessionEventType::rejoined:
            if (x.connect) x.connect(x.self, f, e.player);
            break;
          case gn::SessionEventType::left:
            if (x.disconnect) x.disconnect(x.self, f, e.player, e.why);
            break;
          case gn::SessionEventType::changed:
            if (x.info) x.info(x.self, f, e.player);
            break;
          case gn::SessionEventType::phase:
            if (x.phase) x.phase(x.self, f, h.phase);
            break;
          default:
            break;
        }
      }
      if (e.type == gn::SessionEventType::command) {
        const std::string line(gn::session_text(h.event_bytes, e), e.size);
        run_command(f, e.player, line);
      }
    }
  }
  for (Feature& x : f.list)
    if (x.frame) x.frame(x.self, f);
}

void features_server_info(const Features& f, gn::Status& status) {
  for (const Feature& x : f.list)
    if (x.server_info) x.server_info(x.self, f, status);
}

bool features_admit(void* features, const gn::Address& from) {
  auto& f = *static_cast<Features*>(features);
  for (Feature& x : f.list) {
    if (x.admit && !x.admit(x.self, f, from)) {
      f.refused++;
      return false;
    }
  }
  f.admitted++;
  return true;
}

bool features_chat(void* features, int from, gn::ChatScope scope, int /*to*/, const char* text,
                   size_t size) {
  auto& f = *static_cast<Features*>(features);
  for (Feature& x : f.list) {
    if (x.chat && x.chat(x.self, f, from, scope, std::string_view(text, size))) {
      f.held++;
      return false;
    }
  }
  return true;
}

}  // namespace ge::server
