// Our client watching a classic broadcast (classic_watch.hpp).
//
// ge:summary The broadcast watched: the request, the challenge, whole frames to the demo reader
// ge:keywords watch viewer broadcast request challenge whole-frames multiview live-demo
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_watch_test.cpp
#include "shooters_edge/net/classic_watch.hpp"

#include <span>

#include "shooters_edge/net/classic_mv_state.hpp"

namespace shooters_edge::net {

namespace {

QtvRequest request(const StreamWatch& w) {
  QtvRequest q;
  q.version = 1;
  q.receive = true;
  q.userinfo = "\\name\\" + w.name;
  if (!w.password.empty()) q.offers = {QtvAuth::sha3_512, QtvAuth::plain};
  return q;
}

void send(StreamWatch& w, const QtvRequest& q) {
  const std::string text = qtv_request_text(q);
  gn::tcp_send_bytes(w.tcp, reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

bool end(StreamWatch& w, const std::string& why) {
  w.state = WatchState::ended;
  w.why = why;
  gn::tcp_close(w.tcp);
  return false;
}

// The answer read; true once the match follows.
bool answer(StreamWatch& w) {
  QtvAnswer a;
  size_t used = 0;
  const QtvParse p = qtv_parse_answer(w.text, a, used);
  if (p == QtvParse::bad) return end(w, "that address is not a broadcast");
  if (p == QtvParse::more) return true;
  switch (a.kind) {
    case QtvAnswerKind::begin:
      w.state = WatchState::watching;
      w.held.assign(w.text.begin() + static_cast<std::ptrdiff_t>(used), w.text.end());
      w.text.clear();
      return true;
    case QtvAnswerKind::auth: {
      if (w.password.empty()) return end(w, "the broadcast wants a password");
      if (w.state == WatchState::challenged) return end(w, "the broadcast refused the password");
      QtvRequest q = request(w);
      q.has_password = true;
      q.password = qtv_auth_response(a.method, a.challenge, w.password);
      w.text.erase(0, used);
      w.state = WatchState::challenged;
      send(w, q);
      return true;
    }
    default:
      return end(w, a.text.empty() ? "the broadcast refused" : a.text);
  }
}

}  // namespace

void watch_demo(client::Demo& d) {
  d = client::Demo{};
  d.state.format = client::DemoFormat::multiview;
  d.state.protocol = 28;
  d.state.view_height = 22;
  d.state.players.resize(32);
  d.ring.resize(1);
  d.player_stats.resize(32);
}

bool watch_open(StreamWatch& w, const gn::Address& to, const std::string& name,
                const std::string& password, std::string& error) {
  w = StreamWatch{};
  w.to = to;
  w.name = name;
  w.password = password;
  if (!gn::tcp_connect(w.tcp, to, &error)) {
    error = "could not reach the broadcast at " + gn::to_string(to) + ": " + error;
    return false;
  }
  send(w, request(w));
  return true;
}

bool watch_take(StreamWatch& w, client::Demo& demo) {
  if (w.state == WatchState::ended) return false;
  gn::tcp_pump(w.tcp);
  std::vector<uint8_t> got;
  gn::tcp_take_bytes(w.tcp, got);
  if (w.state != WatchState::watching) {
    w.text.append(got.begin(), got.end());
    if (!answer(w)) return false;
    if (w.state != WatchState::watching) {
      if (w.tcp.closed) return end(w, "the broadcast closed the connection");
      return true;
    }
  } else {
    w.held.insert(w.held.end(), got.begin(), got.end());
  }
  // Whole records; the frames before the newest record that starts one are handed over.
  size_t at = 0, frames_end = 0;
  int64_t records = 0, frames = 0, records_end = 0, frames_ended = 0;
  bool over = false;  // the match's own disconnect came: nothing follows it
  for (;;) {
    MvRecord h;
    const MvSplit s = mv_record(std::span<const uint8_t>(w.held).subspan(at), h);
    if (s == MvSplit::bad) return end(w, "the broadcast sent what is not a match");
    if (s == MvSplit::more) break;
    if (h.ms != 0 && at > 0) {
      frames_end = at;
      records_end = records;
      frames_ended = frames;
    }
    frames += h.ms != 0 ? 1 : 0;
    over = h.size > 0 && (h.kind == kMvAll || h.kind == kMvRead) &&
           w.held[at + h.body] == svc::disconnect;
    at += h.whole;
    records++;
  }
  const bool closed = w.tcp.closed;
  if (closed || over) {  // the last frame is whole when nothing more comes
    frames_end = at;
    records_end = records;
    frames_ended = frames;
  }
  w.records += records_end;
  w.frames += frames_ended;
  demo.bytes.insert(demo.bytes.end(), w.held.begin(),
                    w.held.begin() + static_cast<std::ptrdiff_t>(frames_end));
  w.held.erase(w.held.begin(), w.held.begin() + static_cast<std::ptrdiff_t>(frames_end));
  if (closed) return end(w, "the broadcast ended");
  return true;
}

void watch_say(StreamWatch& w, const std::string& text) {
  if (w.state != WatchState::watching) return;
  std::vector<uint8_t> b;
  std::string t;
  for (const char c : text)
    if (c != '"') t.push_back(c);
  qtv_put_up(b, "say \"" + t + "\"");
  gn::tcp_send_bytes(w.tcp, b.data(), b.size());
}

void watch_close(StreamWatch& w) {
  gn::tcp_close(w.tcp);
  w.state = WatchState::ended;
}

}  // namespace shooters_edge::net
