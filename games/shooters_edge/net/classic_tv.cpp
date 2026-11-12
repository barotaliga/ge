// Our TV node's classic face: the match pulled from a stream port, delayed and served to classic
// viewers (classic_tv.hpp).
//
// ge:summary The classic TV node: dial, answer, delay, catch up, fan out, chat and viewers
// ge:keywords tv-node relay upstream dial back-off delay release viewers handshake catch-up
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_tv_test.cpp
#include "shooters_edge/net/classic_tv.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <utility>

#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_wire.hpp"

namespace shooters_edge::net {

namespace {

constexpr int kPrintChat = 3;

void log_line(TvNode& n, const std::string& line) {
  n.log.push_back(line);
  if (n.log.size() > 256) n.log.erase(n.log.begin());
}

std::string viewer_tag(const TvViewer& v) {
  return "viewer " + std::to_string(v.id) + (v.name.empty() ? "" : " (" + v.name + ")");
}

void send_text(TvViewer& v, const std::string& text) {
  v.out.insert(v.out.end(), text.begin(), text.end());
}

void refuse(TvNode& n, TvViewer& v, QtvAnswerKind kind, const std::string& why) {
  QtvAnswer a;
  a.kind = kind;
  a.text = why;
  send_text(v, qtv_answer_text(a));
  v.gone = true;
  n.counts.refused++;
  log_line(n, viewer_tag(v) + " refused: " + why);
}

void send_up(TvNode& n, const std::string& command) {
  if (n.state != TvUpState::streaming) return;
  std::vector<uint8_t> b;
  qtv_put_up(b, command);
  gn::tcp_send_bytes(n.up, b.data(), b.size());
}

std::string quoted(const std::string& s) {
  std::string t;
  for (const char c : s)
    if (c != '"') t.push_back(c);
  return "\"" + t + "\"";
}

void drop(TvNode& n, TvViewer& v, const std::string& why) {
  if (v.state == QtvState::streaming) {
    n.counts.dropped++;
    log_line(n, viewer_tag(v) + " dropped: " + why);
    send_up(n, "qul 3 " + std::to_string(v.id));
    n.counts.qul++;
    v.state = QtvState::pending;
  }
  v.gone = true;
}

// A print to one viewer's stream, as a record of its own between the match's.
void print_to(TvViewer& v, const std::string& text) {
  std::vector<uint8_t> msg;
  msg.push_back(svc::print);
  msg.push_back(kPrintChat);
  msg.insert(msg.end(), text.begin(), text.end());
  msg.push_back(0);
  mv_put_record(v.out, 0, kMvAll, 0, 0, msg);
}

// ---- Up ----

void fail_up(TvNode& n, double now, const std::string& why) {
  gn::tcp_close(n.up);
  n.up = gn::TcpStream{};
  n.state = TvUpState::idle;
  n.last_error = why;
  n.retry_at = now + n.retry_s;
  log_line(n, "the server's stream lost: " + why + "; dialling again in " +
                  std::to_string(static_cast<int>(n.retry_s)) + " s");
  n.retry_s = std::min(n.retry_s * 2, n.cfg.last_retry_s);
  n.held.clear();
}

QtvRequest up_request(const TvNode& n) {
  QtvRequest q;
  q.version = 1;
  q.receive = true;
  q.userinfo = "\\name\\" + n.cfg.name;
  if (!n.cfg.server_password.empty()) q.offers = {QtvAuth::sha3_512, QtvAuth::plain};
  return q;
}

void send_request(TvNode& n, const QtvRequest& q) {
  const std::string text = qtv_request_text(q);
  gn::tcp_send_bytes(n.up, reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

void dial(TvNode& n, double now) {
  n.counts.dials++;
  n.up = gn::TcpStream{};
  std::string error;
  if (!gn::tcp_connect(n.up, n.server, &error)) {
    fail_up(n, now, error);
    return;
  }
  n.state = TvUpState::asking;
  n.up_text.clear();
  n.held.clear();
  n.retry_at = now;  // the handshake's start
  send_request(n, up_request(n));
}

// Records split off what has arrived and queued to wait out the delay.
void take_records(TvNode& n, double now) {
  size_t at = 0;
  for (;;) {
    MvRecord h;
    const MvSplit got = mv_record(std::span<const uint8_t>(n.held).subspan(at), h);
    if (got == MvSplit::more) break;
    if (got == MvSplit::bad) {
      fail_up(n, now, "the server sent what is not a match");
      return;
    }
    TvNode::Pending p;
    p.release_at = now + n.cfg.delay_s;
    p.record.assign(n.held.begin() + static_cast<std::ptrdiff_t>(at),
                    n.held.begin() + static_cast<std::ptrdiff_t>(at + h.whole));
    n.pending.push_back(std::move(p));
    n.counts.records++;
    n.retry_s = n.cfg.first_retry_s;  // the stream came through
    at += h.whole;
  }
  n.held.erase(n.held.begin(), n.held.begin() + static_cast<std::ptrdiff_t>(at));
}

void read_up(TvNode& n, double now) {
  if (n.state == TvUpState::refused) return;
  if (n.state == TvUpState::idle) {
    if (now >= n.retry_at) dial(n, now);
    return;
  }
  gn::tcp_pump(n.up);
  std::vector<uint8_t> got;
  gn::tcp_take_bytes(n.up, got);
  if (n.state == TvUpState::asking || n.state == TvUpState::challenged) {
    n.up_text.append(got.begin(), got.end());
    QtvAnswer a;
    size_t used = 0;
    const QtvParse p = qtv_parse_answer(n.up_text, a, used);
    if (p == QtvParse::bad) return fail_up(n, now, "the server did not answer as a stream port");
    if (p == QtvParse::more) {
      if (n.up.closed) return fail_up(n, now, "the server closed the connection");
      if (now - n.retry_at > 2 * n.cfg.pending_s)
        return fail_up(n, now, "the server did not answer in time");
      return;
    }
    switch (a.kind) {
      case QtvAnswerKind::begin: {
        n.state = TvUpState::streaming;
        n.held.assign(n.up_text.begin() + static_cast<std::ptrdiff_t>(used), n.up_text.end());
        n.up_text.clear();
        log_line(n, "the server let the node in");
        for (const auto& v : n.viewers)
          if (v->state == QtvState::streaming && !v->gone) {
            send_up(n, "qul 1 " + std::to_string(v->id) + " " + quoted(v->name));
            n.counts.qul++;
          }
        take_records(n, now);
        return;
      }
      case QtvAnswerKind::auth: {
        if (n.cfg.server_password.empty() || n.state == TvUpState::challenged) {
          n.state = TvUpState::refused;
          n.last_error = "the server wants a password";
          log_line(n, "the server's stream refused: " + n.last_error);
          gn::tcp_close(n.up);
          return;
        }
        QtvRequest q = up_request(n);
        q.has_password = true;
        q.password = qtv_auth_response(a.method, a.challenge, n.cfg.server_password);
        n.up_text.erase(0, used);
        n.state = TvUpState::challenged;
        send_request(n, q);
        return;
      }
      case QtvAnswerKind::perror:
        n.state = TvUpState::refused;
        n.last_error = a.text;
        log_line(n, "the server's stream refused: " + a.text);
        gn::tcp_close(n.up);
        return;
      default:
        return fail_up(n, now, a.text);
    }
  }
  n.held.insert(n.held.end(), got.begin(), got.end());
  take_records(n, now);
  if (n.state == TvUpState::streaming && n.up.closed)
    fail_up(n, now, "the server closed the stream");
}

// ---- Down ----

std::vector<std::string> words(const std::string& line) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && line[i] == ' ') i++;
    if (i >= line.size()) break;
    std::string w;
    if (line[i] == '"') {
      for (i++; i < line.size() && line[i] != '"'; i++) w.push_back(line[i]);
      i++;
    } else {
      for (; i < line.size() && line[i] != ' '; i++) w.push_back(line[i]);
    }
    out.push_back(std::move(w));
  }
  return out;
}

std::string rest_of(const std::string& line) {
  const size_t space = line.find(' ');
  if (space == std::string::npos) return {};
  std::string t = line.substr(space + 1);
  if (t.size() >= 2 && t.front() == '"' && t.back() == '"') t = t.substr(1, t.size() - 2);
  return t;
}

void viewer_command(TvNode& n, TvViewer& v, const std::string& line) {
  const std::vector<std::string> w = words(line);
  if (w.empty()) return;
  const std::string& cmd = w[0];
  if (cmd != "say" && cmd != "say_team" && cmd != "say_game") return;  // the rest are ignored
  std::string text = rest_of(line);
  bool game = cmd == "say_game";
  if (text.starts_with("say_game ")) {
    text = text.substr(9);
    game = true;
  }
  const std::string who = "#" + std::to_string(v.id) + ":" + v.name + ": " + text;
  if (game) {
    if (n.state != TvUpState::streaming) {
      print_to(v, "say_game failed\n");
      return;
    }
    send_up(n, "say_game " + quoted(who));
    n.counts.game_chat++;
    return;
  }
  std::string line_out = "#0:qtv_" + cmd;
  line_out += ":";
  line_out += who;
  line_out += "\n";
  for (const auto& o : n.viewers)
    if (o->state == QtvState::streaming && !o->gone && !o->waiting) print_to(*o, line_out);
  n.counts.chat++;
}

std::string make_challenge(TvNode& n, const TvViewer& v) {
  std::string c = gn::to_string(v.tcp.peer);
  while (c.size() < 63) c.push_back(static_cast<char>(ge::core::int_in(n.rng, 33, 126)));
  c.resize(63);
  return c;
}

void let_in(TvNode& n, TvViewer& v, const QtvRequest& q) {
  std::vector<std::string> taken;
  for (const auto& o : n.viewers)
    if (o.get() != &v && o->state == QtvState::streaming && !o->gone) taken.push_back(o->name);
  v.name = tv_viewer_name(info_get(q.userinfo, "name"), taken, n.rng);
  if (q.raw == 0) {
    QtvAnswer a;
    a.kind = QtvAnswerKind::begin;
    send_text(v, qtv_answer_text(a));
  }
  v.waiting = !n.match.level;
  if (!v.waiting) {
    const std::vector<uint8_t> c = mv_catch_up(n.match);
    v.out.insert(v.out.end(), c.begin(), c.end());
    v.catch_up = static_cast<int64_t>(c.size());
  }
  v.allowed = v.out.size() + n.cfg.max_queue;
  v.state = QtvState::streaming;
  n.counts.viewers++;
  log_line(n, viewer_tag(v) + " watching" + (v.waiting ? " (waiting for the level)" : ""));
  send_up(n, "qul 1 " + std::to_string(v.id) + " " + quoted(v.name));
  n.counts.qul++;
}

void handshake(TvNode& n, TvViewer& v) {
  QtvRequest q;
  size_t used = 0;
  const QtvParse got = qtv_parse_request(v.in, q, used);
  if (got == QtvParse::more) return;
  if (got == QtvParse::bad) {
    v.gone = true;  // not the stream protocol: closed without a word
    n.counts.refused++;
    log_line(n, viewer_tag(v) + " closed: not a stream request");
    return;
  }
  v.in.erase(0, used);
  if (q.version != 1) {
    refuse(n, v, QtvAnswerKind::perror, "Incompatible version (valid version is v1)");
    return;
  }
  if (!n.cfg.password.empty()) {
    if (v.state == QtvState::pending) {
      QtvAuth best = QtvAuth::none;
      for (const QtvAuth a : q.offers) best = std::max(best, a);
      if (best == QtvAuth::none) {
        refuse(n, v, QtvAnswerKind::perror, "You need to provide a common auth method.");
        return;
      }
      if (best != QtvAuth::plain || !q.has_password) {
        v.method = best;
        v.challenge = best == QtvAuth::plain ? std::string() : make_challenge(n, v);
        v.state = QtvState::challenged;
        QtvAnswer a;
        a.kind = QtvAnswerKind::auth;
        a.method = best;
        a.challenge = v.challenge;
        send_text(v, qtv_answer_text(a));
        return;
      }
      v.method = QtvAuth::plain;
    }
    if (!q.has_password || q.password != qtv_auth_response(v.method, v.challenge, n.cfg.password)) {
      refuse(n, v, QtvAnswerKind::perror, "Bad password.");
      return;
    }
  }
  if (tv_watching(n) >= n.cfg.max_viewers) {
    refuse(n, v, QtvAnswerKind::error, "The broadcast is full. Please try again later.");
    return;
  }
  let_in(n, v, q);
}

void serve(TvNode& n, TvViewer& v, double now) {
  gn::tcp_pump(v.tcp);
  std::vector<uint8_t> got;
  gn::tcp_take_bytes(v.tcp, got);
  if (!v.gone && v.state != QtvState::streaming) {
    v.in.append(got.begin(), got.end());
    handshake(n, v);
    if (!v.gone && v.state != QtvState::streaming && now - v.since > n.cfg.pending_s) {
      v.gone = true;
      n.counts.refused++;
      log_line(n, viewer_tag(v) + " closed: the handshake took too long");
    }
  } else if (!v.gone) {
    v.up.insert(v.up.end(), got.begin(), got.end());
    size_t at = 0;
    while (!v.gone) {
      std::string cmd;
      size_t used = 0;
      const QtvParse p = qtv_next_up(v.up.data() + at, v.up.size() - at, cmd, used);
      if (p == QtvParse::more) break;
      if (p == QtvParse::bad) {
        drop(n, v, "it sent what a viewer does not");
        break;
      }
      at += used;
      viewer_command(n, v, cmd);
    }
    v.up.erase(v.up.begin(), v.up.begin() + static_cast<std::ptrdiff_t>(std::min(at, v.up.size())));
  }
  if (v.state == QtvState::streaming && !v.gone && v.out.size() > v.allowed)
    drop(n, v, "its queue overflowed");
  size_t sent = 0;
  while (sent < v.out.size()) {
    const size_t piece = std::min(v.out.size() - sent, gn::kStreamBacklog / 4);
    if (!gn::tcp_send_bytes(v.tcp, v.out.data() + sent, piece)) break;
    sent += piece;
  }
  if (sent > 0) {
    v.out.erase(v.out.begin(), v.out.begin() + static_cast<std::ptrdiff_t>(sent));
    v.sent += static_cast<int64_t>(sent);
    v.allowed = v.allowed > sent ? std::max(v.allowed - sent, n.cfg.max_queue) : n.cfg.max_queue;
    v.last_taken = now;
  } else if (v.out.empty()) {
    v.last_taken = now;
  }
  if (v.state == QtvState::streaming && !v.gone) {
    if (v.tcp.closed)
      drop(n, v, "it closed the connection");
    else if (now - v.last_taken > n.cfg.viewer_timeout_s)
      drop(n, v, "it took nothing");
  }
}

// The records whose wait is over, into the kept match and every viewer's stream.
void release(TvNode& n, double now) {
  while (!n.pending.empty() && n.pending.front().release_at <= now) {
    const std::vector<uint8_t> rec = std::move(n.pending.front().record);
    n.pending.pop_front();
    const int64_t levels = n.match.levels;
    const bool was_broken = n.match.broken;
    mv_apply(n.match, rec);
    if (n.match.broken && !was_broken)
      log_line(n, "a late viewer gets no catch-up until the next level: " + n.match.why);
    const bool level = n.match.levels != levels;
    if (level) {
      n.counts.levels++;
      log_line(n, "a level began");
    }
    n.counts.released++;
    for (const auto& v : n.viewers) {
      if (v->state != QtvState::streaming || v->gone) continue;
      if (v->waiting && !level) continue;
      v->waiting = false;
      v->out.insert(v->out.end(), rec.begin(), rec.end());
    }
  }
}

}  // namespace

std::string tv_viewer_name(const std::string& wanted, const std::vector<std::string>& taken,
                           ge::core::Rng& rng) {
  std::string name;
  for (const char c : wanted) name.push_back(static_cast<unsigned char>(c) == 0xA0 ? ' ' : c);
  const size_t a = name.find_first_not_of(' ');
  const size_t b = name.find_last_not_of(' ');
  name = a == std::string::npos ? std::string() : name.substr(a, b - a + 1);
  if (name.empty() || name == "console" || name.find_first_of("#:") != std::string::npos)
    name = "unnamed";
  if (std::find(taken.begin(), taken.end(), name) != taken.end()) {
    name += "(";
    for (int i = 0; i < 8; i++)
      name.push_back(static_cast<char>('a' + ge::core::int_in(rng, 0, 25)));
    name += ")";
    if (name.size() > 25) name.resize(25);
  }
  return name;
}

bool tv_open(TvNode& n, const gn::Address& bind, const gn::Address& server, const TvConfig& cfg,
             std::string& error) {
  n.cfg = cfg;
  n.server = server;
  n.retry_s = cfg.first_retry_s;
  if (!gn::tcp_listen(n.listener, bind, &error)) {
    error = "the broadcast port could not listen at " + gn::to_string(bind) + ": " + error +
            "; pick another port or free that one";
    return false;
  }
  return true;
}

int tv_watching(const TvNode& n) {
  int k = 0;
  for (const auto& v : n.viewers) k += v->state == QtvState::streaming && !v->gone ? 1 : 0;
  return k;
}

void tv_update(TvNode& n, double now) {
  read_up(n, now);
  release(n, now);
  for (;;) {
    auto v = std::make_unique<TvViewer>();
    if (!gn::tcp_accept(n.listener, v->tcp)) break;
    v->id = n.next_id++;
    v->since = v->last_taken = now;
    if (static_cast<int>(n.viewers.size()) >= n.cfg.max_viewers + 3)
      refuse(n, *v, QtvAnswerKind::error, "The broadcast is full. Please try again later.");
    n.viewers.push_back(std::move(v));
  }
  for (const auto& v : n.viewers) serve(n, *v, now);
  std::erase_if(n.viewers, [&](const std::unique_ptr<TvViewer>& v) {
    if (!v->gone) return false;
    if (v->gone_at < 0) v->gone_at = now;
    const bool said = v->out.empty() && v->tcp.out_head >= v->tcp.out.size();
    if (!said && !v->tcp.closed && now - v->gone_at < 1) return false;
    gn::tcp_close(v->tcp);
    return true;
  });
}

void tv_close(TvNode& n) {
  for (const auto& v : n.viewers) {
    if (!v->out.empty())
      gn::tcp_send_bytes(v->tcp, v->out.data(), std::min(v->out.size(), gn::kStreamBacklog));
    gn::tcp_pump(v->tcp);
    gn::tcp_close(v->tcp);
  }
  n.viewers.clear();
  gn::tcp_close(n.up);
  gn::tcp_close_listener(n.listener);
}

}  // namespace shooters_edge::net
