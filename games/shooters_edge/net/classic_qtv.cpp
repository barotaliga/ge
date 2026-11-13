// The server's stream port for classic relays (classic_qtv.hpp).
//
// ge:summary The classic relay handshake, SHA3-512 auth, up messages and the per-relay streams
// ge:keywords qtv stream-port relay handshake auth challenge sha3-512 keccak
// ge:keywords up-message qul say_game
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_qtv_test.cpp
#include "shooters_edge/net/classic_qtv.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_server.hpp"

namespace shooters_edge::net {

namespace {

// ---- SHA3-512 (FIPS 202): the Keccak-f[1600] permutation, rate 72 bytes ----

constexpr uint64_t kRound[24] = {
    0x0000000000000001ull, 0x0000000000008082ull, 0x800000000000808Aull, 0x8000000080008000ull,
    0x000000000000808Bull, 0x0000000080000001ull, 0x8000000080008081ull, 0x8000000000008009ull,
    0x000000000000008Aull, 0x0000000000000088ull, 0x0000000080008009ull, 0x000000008000000Aull,
    0x000000008000808Bull, 0x800000000000008Bull, 0x8000000000008089ull, 0x8000000000008003ull,
    0x8000000000008002ull, 0x8000000000000080ull, 0x000000000000800Aull, 0x800000008000000Aull,
    0x8000000080008081ull, 0x8000000000008080ull, 0x0000000080000001ull, 0x8000000080008008ull};
// Each lane's rotation, by x + 5y.
constexpr int kRotate[25] = {0,  1,  62, 28, 27, 36, 44, 6,  55, 20, 3,  10, 43,
                             25, 39, 41, 45, 15, 21, 8,  18, 2,  61, 56, 14};

uint64_t rotl(uint64_t v, int n) { return n == 0 ? v : (v << n) | (v >> (64 - n)); }

void keccak(uint64_t a[25]) {
  for (const uint64_t rc : kRound) {
    uint64_t c[5];
    for (int x = 0; x < 5; x++) c[x] = a[x] ^ a[x + 5] ^ a[x + 10] ^ a[x + 15] ^ a[x + 20];
    for (int x = 0; x < 5; x++) {
      const uint64_t d = c[(x + 4) % 5] ^ rotl(c[(x + 1) % 5], 1);
      for (int y = 0; y < 25; y += 5) a[x + y] ^= d;
    }
    uint64_t b[25];
    for (int x = 0; x < 5; x++)
      for (int y = 0; y < 5; y++)
        b[y + 5 * ((2 * x + 3 * y) % 5)] = rotl(a[x + 5 * y], kRotate[x + 5 * y]);
    for (int y = 0; y < 25; y += 5)
      for (int x = 0; x < 5; x++) a[x + y] = b[x + y] ^ (~b[(x + 1) % 5 + y] & b[(x + 2) % 5 + y]);
    a[0] ^= rc;
  }
}

void absorb(uint64_t a[25], const uint8_t* block, size_t size) {
  for (size_t i = 0; i < size; i++) a[i / 8] ^= static_cast<uint64_t>(block[i]) << (8 * (i % 8));
}

// ---- Text ----

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

std::string unquote(std::string_view s) {
  s = trim(s);
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
  return std::string(s);
}

bool parse_auth(std::string_view name, QtvAuth& out) {
  if (name == "NONE")
    out = QtvAuth::none;
  else if (name == "PLAIN")
    out = QtvAuth::plain;
  else if (name == "SHA3_512")
    out = QtvAuth::sha3_512;
  else
    return false;
  return true;
}

// The lines of a block ended by a blank line at the front of `text`: `more` until it is whole.
QtvParse block_lines(std::string_view text, std::vector<std::string_view>& lines, size_t& used) {
  lines.clear();
  size_t at = 0;
  while (true) {
    const size_t nl = text.find('\n', at);
    if (nl == std::string_view::npos) return text.size() > 8192 ? QtvParse::bad : QtvParse::more;
    std::string_view line = text.substr(at, nl - at);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    at = nl + 1;
    if (line.empty()) {
      used = at;
      return QtvParse::ok;
    }
    lines.push_back(line);
  }
}

// "KEY: value" split; false when the line has no colon.
bool key_value(std::string_view line, std::string_view& key, std::string_view& value) {
  const size_t colon = line.find(':');
  if (colon == std::string_view::npos) return false;
  key = trim(line.substr(0, colon));
  value = trim(line.substr(colon + 1));
  return true;
}

// The command's words, quotes keeping spaces in one.
std::vector<std::string> words(const std::string& line) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at < line.size()) {
    while (at < line.size() && std::isspace(static_cast<unsigned char>(line[at])) != 0) at++;
    if (at >= line.size()) break;
    if (line[at] == '"') {
      const size_t end = line.find('"', at + 1);
      out.push_back(line.substr(at + 1, (end == std::string::npos ? line.size() : end) - at - 1));
      at = end == std::string::npos ? line.size() : end + 1;
      continue;
    }
    size_t end = at;
    while (end < line.size() && std::isspace(static_cast<unsigned char>(line[end])) == 0) end++;
    out.push_back(line.substr(at, end - at));
    at = end;
  }
  return out;
}

// The rest of `line` after its first word, a pair of quotes round it taken off.
std::string rest_after_command(const std::string& line) {
  const size_t space = line.find(' ');
  if (space == std::string::npos) return {};
  return unquote(std::string_view(line).substr(space + 1));
}

bool same_name(const std::string& a, const std::string& b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return std::tolower(static_cast<unsigned char>(x)) ==
                  std::tolower(static_cast<unsigned char>(y));
         });
}

constexpr int kPrintChat = 3;
constexpr int kMostUserBytes = 2048;  // a relay's viewers' names held at most

void log_line(QtvPort& p, const std::string& line) {
  p.log.push_back(line);
  if (p.log.size() > 256) p.log.erase(p.log.begin());
}

void send_text(QtvRelay& r, const std::string& text) {
  r.out.insert(r.out.end(), text.begin(), text.end());
}

void refuse(QtvPort& p, QtvRelay& r, QtvAnswerKind kind, const std::string& why) {
  QtvAnswer a;
  a.kind = kind;
  a.text = why;
  send_text(r, qtv_answer_text(a));
  r.gone = true;
  p.counts.refused++;
  log_line(p, "relay " + std::to_string(r.id) + " refused: " + why);
}

void drop(QtvPort& p, ClassicServer& s, QtvRelay& r, const std::string& why) {
  if (r.state == QtvState::streaming) {
    std::erase(s.streams, &r.rec);
    p.counts.dropped++;
    log_line(p, "relay " + std::to_string(r.id) + " (" + r.name + ") dropped: " + why);
    r.state = QtvState::pending;
  }
  r.gone = true;
}

// Every spawned player (only the spectators when `spectators`) told `text`, and every relay's
// stream with it.
void tell(ClassicServer& s, QtvPort& p, const std::string& text, bool spectators) {
  for (int k = 0; k < sh::kMaxClients; k++) {
    const ClassicClient& c = s.clients[k];
    if (c.state != ConnState::spawned || c.local) continue;
    if (spectators && !c.spectator) continue;
    client_print(s, k, kPrintChat, text);
  }
  for (const auto& r : p.relays)
    if (r->state == QtvState::streaming) mvd_print(r->rec, kPrintChat, text);
}

void tell_qul(ClassicServer& s, const std::string& text) {
  for (int k = 0; k < sh::kMaxClients; k++) {
    const ClassicClient& c = s.clients[k];
    if (c.state != ConnState::spawned || c.local) continue;
    if (info_get(c.userinfo, "qul") == "1") stuff(s, k, "//" + text + "\n");
  }
}

void up_command(QtvPort& p, ClassicServer& s, QtvRelay& r, const std::string& line) {
  const std::vector<std::string> w = words(line);
  if (w.empty()) return;
  const std::string& cmd = w[0];
  if (cmd == "say" || cmd == "say_team" || cmd == "say_game") {
    std::string text = rest_after_command(line);
    if (text.starts_with("say_game ")) text = text.substr(9);
    const std::string kind = cmd == "say_team" ? "say_team" : "say";
    tell(s, p, "#0:qtv_" + kind + "_game:" + text + "\n", cmd == "say_team" || !p.cfg.say_enabled);
    p.counts.chat++;
    return;
  }
  if (cmd == "qul" && w.size() >= 3) {
    const int action = std::atoi(w[1].c_str());
    const int id = std::atoi(w[2].c_str());
    const std::string name = w.size() >= 4 ? w[3] : std::string();
    auto it = std::find_if(r.viewers.begin(), r.viewers.end(),
                           [id](const QtvViewer& v) { return v.id == id; });
    p.counts.qul++;
    if (action == 1 || action == 2) {
      if (it == r.viewers.end()) {
        r.user_bytes += static_cast<int>(name.size()) + 8;
        if (r.user_bytes > kMostUserBytes) {
          drop(p, s, r, "too many viewers' names");
          return;
        }
        r.viewers.push_back({id, name});
        tell(s, p, "QTV: " + r.name + ": " + name + " join\n", false);
      } else if (it->name != name) {
        tell(s, p, "QTV: " + r.name + ": " + it->name + " changed name to " + name + "\n", false);
        it->name = name;
      }
    } else if (action == 3 && it != r.viewers.end()) {
      tell(s, p, "QTV: " + r.name + ": " + it->name + " left\n", false);
      r.user_bytes -= static_cast<int>(it->name.size()) + 8;
      r.viewers.erase(it);
    } else {
      return;
    }
    tell_qul(s, line);
  }
  // Anything else from a relay is not a server's to run.
}

std::string make_challenge(QtvPort& p, const QtvRelay& r) {
  std::string c = gn::to_string(r.tcp.peer);
  while (c.size() < 63) c.push_back(static_cast<char>(ge::core::int_in(p.rng, 33, 126)));
  c.resize(63);
  return c;
}

void handshake(QtvPort& p, ClassicServer& s, QtvRelay& r) {
  QtvRequest q;
  size_t used = 0;
  const QtvParse got = qtv_parse_request(r.in, q, used);
  if (got == QtvParse::more) return;
  if (got == QtvParse::bad) {
    r.gone = true;  // not the stream protocol: closed without a word
    p.counts.refused++;
    log_line(p, "relay " + std::to_string(r.id) + " closed: not a stream request");
    return;
  }
  r.in.erase(0, used);
  if (q.version != 1) {
    refuse(p, r, QtvAnswerKind::perror, "Incompatible version (valid version is v1)");
    return;
  }
  if (!p.cfg.password.empty()) {
    if (r.state == QtvState::pending) {
      QtvAuth best = QtvAuth::none;
      bool any = false;
      for (const QtvAuth a : q.offers)
        if (a != QtvAuth::none) {
          best = std::max(best, a);
          any = true;
        }
      if (!any) {
        refuse(p, r, QtvAnswerKind::perror, "You need to provide a common auth method.");
        return;
      }
      if (best == QtvAuth::plain && q.has_password) {
        r.method = QtvAuth::plain;  // a plain password may come at once
      } else {
        r.method = best;
        r.challenge = best == QtvAuth::plain ? std::string() : make_challenge(p, r);
        r.state = QtvState::challenged;
        QtvAnswer a;
        a.kind = QtvAnswerKind::auth;
        a.method = best;
        a.challenge = r.challenge;
        send_text(r, qtv_answer_text(a));
        return;
      }
    }
    if (!q.has_password) {
      refuse(p, r, QtvAnswerKind::perror, "You need to provide a password.");
      return;
    }
    if (q.password != qtv_auth_response(r.method, r.challenge, p.cfg.password)) {
      refuse(p, r, QtvAnswerKind::perror, "Bad password.");
      return;
    }
  }
  r.name = info_get(q.userinfo, "name");
  if (r.name.empty()) r.name = "relay " + std::to_string(r.id);
  int streaming = 0;
  bool taken = false;
  for (const auto& o : p.relays)
    if (o.get() != &r && o->state == QtvState::streaming) {
      streaming++;
      taken = taken || same_name(o->name, r.name);
    }
  if (taken || (p.cfg.max_streams > 0 && streaming >= p.cfg.max_streams)) {
    refuse(p, r, QtvAnswerKind::error,
           "Can't init stream, probably server reach a limit on the number of proxies connected "
           "at any one time.");
    return;
  }
  if (q.raw == 0) {
    QtvAnswer a;
    a.kind = QtvAnswerKind::begin;
    send_text(r, qtv_answer_text(a));
  }
  mvd_start(r.rec, false);
  r.rec.stream = true;
  s.streams.push_back(&r.rec);
  r.state = QtvState::streaming;
  r.last_taken = s.now;
  p.counts.streams++;
  log_line(p, "relay " + std::to_string(r.id) + " (" + r.name + ") streaming");
}

}  // namespace

const char* qtv_auth_name(QtvAuth a) {
  switch (a) {
    case QtvAuth::plain:
      return "PLAIN";
    case QtvAuth::sha3_512:
      return "SHA3_512";
    default:
      return "NONE";
  }
}

std::string sha3_512_hex(std::string_view bytes) {
  constexpr size_t kRate = 72;
  uint64_t a[25] = {};
  const auto* p = reinterpret_cast<const uint8_t*>(bytes.data());
  size_t left = bytes.size();
  while (left >= kRate) {
    absorb(a, p, kRate);
    keccak(a);
    p += kRate;
    left -= kRate;
  }
  uint8_t last[kRate] = {};
  if (left > 0) std::memcpy(last, p, left);
  last[left] ^= 0x06;
  last[kRate - 1] ^= 0x80;
  absorb(a, last, kRate);
  keccak(a);
  static const char kHex[] = "0123456789abcdef";
  std::string out;
  for (int i = 0; i < 64; i++) {
    const auto b = static_cast<uint8_t>(a[i / 8] >> (8 * (i % 8)));
    out.push_back(kHex[b >> 4]);
    out.push_back(kHex[b & 15]);
  }
  return out;
}

std::string qtv_auth_response(QtvAuth method, const std::string& challenge,
                              const std::string& password) {
  switch (method) {
    case QtvAuth::sha3_512:
      return sha3_512_hex(challenge + password);
    case QtvAuth::plain:
      return password;
    default:
      return {};
  }
}

QtvParse qtv_parse_request(std::string_view text, QtvRequest& out, size_t& used) {
  out = QtvRequest{};
  if (text.size() >= 4 && !text.starts_with("QTV\n") && !text.starts_with("QTV\r"))
    return QtvParse::bad;
  if (text.size() < 4)
    return std::string_view("QTV\n").starts_with(text) ? QtvParse::more : QtvParse::bad;
  std::vector<std::string_view> lines;
  const QtvParse got = block_lines(text, lines, used);
  if (got != QtvParse::ok) return got;
  for (size_t i = 1; i < lines.size(); i++) {
    std::string_view key, value;
    if (lines[i] == "RECEIVE") {
      out.receive = true;
    } else if (key_value(lines[i], key, value)) {
      if (key == "VERSION") {
        out.version = std::atoi(std::string(value).c_str());
      } else if (key == "SOURCE") {
        out.source = std::string(value);
      } else if (key == "USERINFO") {
        out.userinfo = unquote(value);
      } else if (key == "AUTH") {
        QtvAuth a;
        if (parse_auth(value, a)) out.offers.push_back(a);
      } else if (key == "PASSWORD") {
        out.has_password = true;
        out.password = unquote(value);
      } else if (key == "RAW") {
        out.raw = std::atoi(std::string(value).c_str());
      }
    }
  }
  return QtvParse::ok;
}

std::string qtv_request_text(const QtvRequest& r) {
  std::string t = "QTV\nVERSION: " + std::to_string(r.version) + "\n";
  if (r.receive) t += "RECEIVE\n";
  if (!r.source.empty()) t += "SOURCE: " + r.source + "\n";
  if (!r.userinfo.empty()) t += "USERINFO: \"" + r.userinfo + "\"\n";
  for (const QtvAuth a : r.offers) t += std::string("AUTH: ") + qtv_auth_name(a) + "\n";
  if (r.has_password) t += "PASSWORD: \"" + r.password + "\"\n";
  if (r.raw != 0) t += "RAW: " + std::to_string(r.raw) + "\n";
  return t + "\n";
}

QtvParse qtv_parse_answer(std::string_view text, QtvAnswer& out, size_t& used) {
  out = QtvAnswer{};
  std::vector<std::string_view> lines;
  const QtvParse got = block_lines(text, lines, used);
  if (got != QtvParse::ok) return got;
  if (lines.empty() || !lines[0].starts_with("QTVSV ")) return QtvParse::bad;
  bool known = false;
  for (size_t i = 1; i < lines.size(); i++) {
    std::string_view key, value;
    if (lines[i] == "BEGIN" || lines[i].starts_with("BEGIN:")) {
      out.kind = QtvAnswerKind::begin;
      known = true;
    } else if (key_value(lines[i], key, value)) {
      if (key == "AUTH") {
        out.kind = QtvAnswerKind::auth;
        known = parse_auth(value, out.method);
      } else if (key == "CHALLENGE") {
        out.challenge = std::string(value);
      } else if (key == "PERROR" || key == "TERROR" || key == "ERROR") {
        out.kind = key == "PERROR"   ? QtvAnswerKind::perror
                   : key == "TERROR" ? QtvAnswerKind::terror
                                     : QtvAnswerKind::error;
        out.text = std::string(value);
        known = true;
      }
    }
  }
  return known ? QtvParse::ok : QtvParse::bad;
}

std::string qtv_answer_text(const QtvAnswer& a) {
  std::string t = "QTVSV 1\n";
  switch (a.kind) {
    case QtvAnswerKind::begin:
      t += "BEGIN\n";
      break;
    case QtvAnswerKind::auth:
      t += std::string("AUTH: ") + qtv_auth_name(a.method) + "\n";
      if (!a.challenge.empty()) t += "CHALLENGE: " + a.challenge + "\n";
      break;
    case QtvAnswerKind::perror:
      t += "PERROR: " + a.text + "\n";
      break;
    case QtvAnswerKind::terror:
      t += "TERROR: " + a.text + "\n";
      break;
    case QtvAnswerKind::error:
      t += "ERROR: " + a.text + "\n";
      break;
  }
  return t + "\n";
}

void qtv_put_up(std::vector<uint8_t>& out, const std::string& command) {
  const size_t n = 3 + command.size() + 1;
  out.push_back(static_cast<uint8_t>(n & 0xFF));
  out.push_back(static_cast<uint8_t>(n >> 8));
  out.push_back(1);
  out.insert(out.end(), command.begin(), command.end());
  out.push_back(0);
}

QtvParse qtv_next_up(const uint8_t* bytes, size_t size, std::string& command, size_t& used) {
  if (size < 3) return QtvParse::more;
  const size_t n = bytes[0] | static_cast<size_t>(bytes[1]) << 8;
  if (n < 3 || n > 4096 || bytes[2] != 1) return QtvParse::bad;
  if (size < n) return QtvParse::more;
  const auto* body = reinterpret_cast<const char*>(bytes + 3);
  command.assign(body, strnlen(body, n - 3));
  used = n;
  return QtvParse::ok;
}

bool qtv_open(QtvPort& p, const gn::Address& bind, const QtvConfig& cfg, std::string& error) {
  p.cfg = cfg;
  if (!gn::tcp_listen(p.listener, bind, &error)) {
    error = "the stream port could not listen at " + gn::to_string(bind) + ": " + error +
            "; pick another qtv_streamport or free that one";
    return false;
  }
  return true;
}

int qtv_streaming(const QtvPort& p) {
  int n = 0;
  for (const auto& r : p.relays) n += r->state == QtvState::streaming ? 1 : 0;
  return n;
}

void qtv_update(QtvPort& p, ClassicServer& s) {
  // New connections; past the limit and three spare, told so and closed.
  for (;;) {
    auto r = std::make_unique<QtvRelay>();
    if (!gn::tcp_accept(p.listener, r->tcp)) break;
    r->id = p.next_id++;
    r->since = r->last_taken = s.now;
    p.counts.accepted++;
    const int held = static_cast<int>(p.relays.size());
    if (p.cfg.max_streams > 0 && held >= p.cfg.max_streams + 3) {
      refuse(p, *r, QtvAnswerKind::error,
             "This server enforces a limit on the number of proxies connected at any one time. "
             "Please try again later");
    }
    p.relays.push_back(std::move(r));
  }
  std::vector<uint8_t> got;
  for (const auto& rp : p.relays) {
    QtvRelay& r = *rp;
    gn::tcp_pump(r.tcp);
    got.clear();
    gn::tcp_take_bytes(r.tcp, got);
    if (!r.gone && r.state != QtvState::streaming) {
      r.in.append(got.begin(), got.end());
      handshake(p, s, r);
      if (!r.gone && r.state != QtvState::streaming && s.now - r.since > p.cfg.pending_s) {
        r.gone = true;
        p.counts.refused++;
        log_line(p, "relay " + std::to_string(r.id) + " closed: the handshake took too long");
      }
    } else if (!r.gone) {
      r.up.insert(r.up.end(), got.begin(), got.end());
      size_t at = 0;
      while (!r.gone) {
        std::string cmd;
        size_t used = 0;
        const QtvParse up = qtv_next_up(r.up.data() + at, r.up.size() - at, cmd, used);
        if (up == QtvParse::more) break;
        if (up == QtvParse::bad) {
          drop(p, s, r, "it sent what a relay does not");
          break;
        }
        at += used;
        up_command(p, s, r, cmd);
      }
      r.up.erase(r.up.begin(),
                 r.up.begin() + static_cast<std::ptrdiff_t>(std::min(at, r.up.size())));
    }
    if (r.state == QtvState::streaming && !r.gone) {
      r.out.insert(r.out.end(), r.rec.bytes.begin(), r.rec.bytes.end());
      r.rec.bytes.clear();
      if (r.out.size() > p.cfg.max_queue) drop(p, s, r, "its queue overflowed");
    }
    // Hand the OS what it will take, in pieces under its backlog.
    size_t sent = 0;
    while (sent < r.out.size()) {
      const size_t piece = std::min(r.out.size() - sent, gn::kStreamBacklog / 4);
      if (!gn::tcp_send_bytes(r.tcp, r.out.data() + sent, piece)) break;
      sent += piece;
    }
    if (sent > 0) {
      r.out.erase(r.out.begin(), r.out.begin() + static_cast<std::ptrdiff_t>(sent));
      r.sent += static_cast<int64_t>(sent);
      r.last_taken = s.now;
    } else if (r.out.empty()) {
      r.last_taken = s.now;
    }
    if (r.state == QtvState::streaming && !r.gone) {
      if (r.tcp.closed)
        drop(p, s, r, "it closed the connection");
      else if (s.now - r.last_taken > p.cfg.stream_timeout_s)
        drop(p, s, r, "it took nothing");
    }
  }
  // The gone closed once their last words are with the OS (or after a second's wait).
  std::erase_if(p.relays, [&](const std::unique_ptr<QtvRelay>& r) {
    if (!r->gone) return false;
    if (r->gone_at < 0) r->gone_at = s.now;
    const bool said = r->out.empty() && r->tcp.out_head >= r->tcp.out.size();
    if (!said && !r->tcp.closed && s.now - r->gone_at < 1) return false;
    gn::tcp_close(r->tcp);
    return true;
  });
}

void qtv_close(QtvPort& p, ClassicServer& s) {
  for (const auto& r : p.relays) {
    if (r->state == QtvState::streaming) {
      std::erase(s.streams, &r->rec);
      mvd_stop(r->rec);
      r->out.insert(r->out.end(), r->rec.bytes.begin(), r->rec.bytes.end());
      gn::tcp_send_bytes(r->tcp, r->out.data(), std::min(r->out.size(), gn::kStreamBacklog));
      gn::tcp_pump(r->tcp);
    }
    gn::tcp_close(r->tcp);
  }
  p.relays.clear();
  gn::tcp_close_listener(p.listener);
}

}  // namespace shooters_edge::net
