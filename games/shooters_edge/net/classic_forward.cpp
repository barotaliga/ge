// The classic face of our forward (classic_forward.hpp): the handshake, the answers, the probes.
//
// ge:summary The classic face of our forward: prx handshake, ping, status, pingstatus, probes
// ge:keywords forward classic-forward prx getchallenge pingstatus probe
// ge:skill net shooter
#include "shooters_edge/net/classic_forward.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include "net/udp_socket.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_wire.hpp"

namespace shooters_edge::net {

namespace {

namespace fw = ge::forward;

constexpr int kProtocol = 28;
constexpr int kStepConnect = 100;  // face_step from here: the connect sent upstream
constexpr const char* kVersion = "qwfwd ge 1";
constexpr const char* kShortVersion = "ge 1";
constexpr size_t kMostAnswer = 1400;  // a status answer's bytes, one datagram

uint64_t mix(uint64_t z) {
  z += 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}

bool oob(const uint8_t* data, size_t size) {
  return size >= 4 && data[0] == 0xff && data[1] == 0xff && data[2] == 0xff && data[3] == 0xff;
}

std::string_view text_of(const uint8_t* data, size_t size) {
  return {reinterpret_cast<const char*>(data) + 4, size - 4};
}

void send_oob(fw::Forward& f, const gn::Address& to, const std::string& text, int64_t now) {
  std::string d = "\xff\xff\xff\xff";
  d += text;
  fw::forward_send(f, to, reinterpret_cast<const uint8_t*>(d.data()), d.size(), now);
}

void send_up(fw::HopClient& c, const std::string& text, int64_t now) {
  std::string d = "\xff\xff\xff\xff";
  d += text;
  gn::transport_send(*c.up, c.to, reinterpret_cast<const uint8_t*>(d.data()), d.size(), now);
}

// A print the client shows: `n`, the text, a newline.
void print_to(fw::Forward& f, const gn::Address& to, const std::string& text, int64_t now) {
  std::string t = "n";
  t += text;
  t += '\n';
  send_oob(f, to, t, now);
}

int32_t challenge_of(const ClassicFace& face, const gn::Address& from) {
  return static_cast<int32_t>(mix(face.secret ^ gn::address_hash(from)) & 0x7fffffffu);
}

gn::Address host_of(gn::Address a) {
  a.port = 0;
  return a;
}

// One status-sized answer a second a host; true when this one may go.
bool may_answer(ClassicFace& face, const gn::Address& from, int64_t now) {
  const uint64_t host = gn::address_hash(host_of(from));
  std::erase_if(face.answered, [&](const auto& e) { return now - e.second >= 1000000; });
  for (const auto& [h, at] : face.answered)
    if (h == host) {
      face.counts.limited++;
      return false;
    }
  if (face.answered.size() < 1024) face.answered.emplace_back(host, now);
  return true;
}

// The words after the out-of-band header split on spaces, a quoted word kept whole.
std::vector<std::string> words_of(std::string_view line) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at < line.size()) {
    while (at < line.size() && line[at] == ' ') at++;
    if (at >= line.size()) break;
    if (line[at] == '"') {
      const size_t end = line.find('"', at + 1);
      if (end == std::string_view::npos) {
        out.emplace_back();  // an unclosed quote: a word the connect refuses
        out.back() = "\"";
        return out;
      }
      out.emplace_back(line.substr(at + 1, end - at - 1));
      at = end + 1;
    } else {
      size_t end = line.find(' ', at);
      if (end == std::string_view::npos) end = line.size();
      out.emplace_back(line.substr(at, end - at));
      at = end;
    }
  }
  return out;
}

void answer_status(fw::Forward& f, ClassicFace& face, const gn::Address& from,
                   std::string_view rest, int64_t now) {
  if (!may_answer(face, from, now)) return;
  face.counts.statuses++;
  const int flags = rest.empty() ? 0 : std::atoi(std::string(rest).c_str());
  std::string t = "n";
  if (flags == 0 || (flags & 1) != 0) {
    t += classic_forward_info(f, face);
    t += '\n';
  }
  if (flags == 0 || (flags & 6) != 0) {
    int id = 1;
    for (const auto& c : f.clients) {
      if (!c->face || !c->connected || c->dropped) continue;
      const int64_t minutes = (now - c->since_us) / 60000000;
      std::string line = std::to_string(id++) + " 0 " + std::to_string(minutes) + " 0 \"";
      line += c->name;
      line += "\" \"\" 0 0\n";
      if (t.size() + line.size() > kMostAnswer) break;
      t += line;
    }
  }
  send_oob(f, from, t, now);
}

void put_u16le(std::string& s, int v) {
  s += static_cast<char>(v & 0xff);
  s += static_cast<char>((v >> 8) & 0xff);
}

void answer_pingstatus(fw::Forward& f, ClassicFace& face, const gn::Address& from, int64_t now) {
  if (!may_answer(face, from, now)) return;
  face.counts.pingstatuses++;
  std::string t = "n";
  for (const FaceServer& s : face.known) {
    uint8_t ip[4];
    if (s.address.family == gn::Family::sim)
      gn::sim_to_ipv4(s.address, ip);
    else if (s.address.family == gn::Family::ipv4)
      std::memcpy(ip, s.address.bytes, 4);
    else
      continue;
    if (t.size() + 8 > kMostAnswer) break;
    t.append(reinterpret_cast<const char*>(ip), 4);
    put_u16le(t, s.address.port);
    put_u16le(t, s.ping_ms);
  }
  send_oob(f, from, t, now);
}

// `connect 28 <qport> <challenge> "<userinfo>"` from `from`: checked, the route popped, the client
// admitted and its handshake upstream begun.
void take_connect(fw::Forward& f, ClassicFace& face, const gn::Address& from, std::string_view text,
                  int64_t now) {
  face.counts.connects++;
  const std::string_view line = text.substr(0, text.find('\n'));
  const std::vector<std::string> w = words_of(line);
  const auto refuse = [&](const std::string& why) {
    face.counts.refused++;
    print_to(f, from, why, now);
  };
  if (w.size() < 5) return refuse("The connect is missing its user info.");
  if (std::atoi(w[1].c_str()) != kProtocol) return refuse("This forwarder takes protocol 28.");
  if (std::atoi(w[3].c_str()) != challenge_of(face, from)) return refuse("Bad challenge.");
  std::string userinfo = w[4];
  if (userinfo == "\"" || !info_valid(userinfo)) return refuse("Invalid user info.");
  const std::string prx = info_get(userinfo, "prx");
  if (prx.empty())
    return refuse("This is a forwarder: set prx to the server's address, then connect again.");
  const size_t at = prx.find('@');
  const std::string first = prx.substr(0, at);
  const std::string rest = at == std::string::npos ? std::string() : prx.substr(at + 1);
  gn::Address to;
  if (!classic_destination(first, to)) return refuse("'" + first + "' is not a server's address.");
  if (rest.empty())
    info_remove(userinfo, "prx");
  else
    info_set(userinfo, "prx", rest);
  info_set(userinfo, "*qwfwd", kShortVersion);
  const int qport = std::atoi(w[2].c_str());
  if (fw::HopClient* known = fw::forward_client(f, from); known != nullptr && known->face &&
                                                          !known->connected && known->to == to &&
                                                          known->face_number == qport)
    return;  // the same connect again while its handshake upstream runs
  fw::Answer why_not = fw::Answer::ok;
  std::string why;
  fw::HopClient* c = fw::forward_admit(f, from, to, now, why_not, why);
  if (c == nullptr) return refuse(why + ".");
  c->face_number = qport;
  c->face_text = userinfo;
  const std::string name = info_get(userinfo, "name");
  if (!name.empty()) c->name = name;
}

// A client datagram the forward did not take.
bool from_client(fw::Forward& f, ClassicFace& face, const uint8_t* data, size_t size,
                 const gn::Address& from, int64_t now) {
  if (oob(data, size)) {
    const std::string_view t = text_of(data, size);
    if (t.rfind("getchallenge", 0) == 0) {
      face.counts.challenges++;
      std::string a = "c" + std::to_string(challenge_of(face, from));
      a += '\0';
      send_oob(f, from, a, now);
    } else if (t.rfind("pingstatus", 0) == 0) {
      answer_pingstatus(f, face, from, now);
    } else if (t.rfind("ping", 0) == 0 ||
               (t.size() >= 2 && t[0] == 'k' && (t[1] == '\0' || t[1] == '\n'))) {
      face.counts.pings++;
      const uint8_t l = 'l';
      fw::forward_send(f, from, &l, 1, now);
    } else if (t.rfind("status", 0) == 0) {
      std::string_view rest = t.substr(6);
      while (!rest.empty() && (rest.front() == ' ')) rest.remove_prefix(1);
      while (!rest.empty() && (rest.back() == '\n' || rest.back() == '\0')) rest.remove_suffix(1);
      answer_status(f, face, from, rest, now);
    } else if (t.rfind("connect ", 0) == 0) {
      take_connect(f, face, from, t, now);
    }
    return true;
  }
  if (size == 1 && data[0] == 'l') {  // a server's answer to a probe
    for (FaceServer& s : face.known)
      if (s.address == from && s.probed_us >= 0) {
        s.ping_ms = static_cast<int>(std::min<int64_t>((now - s.probed_us) / 1000, 65534));
        s.answered_us = now;
        return true;
      }
    return false;
  }
  if (fw::HopClient* c = fw::forward_client(f, from)) {
    (void)c;
    return true;  // a client not yet connected: its game datagrams wait for the route
  }
  // A connected client's NAT gave it another port: its qport on the same host says who it is.
  if (size >= 10) {
    const int qport = data[8] | (data[9] << 8);
    for (auto& c : f.clients) {
      if (!c->face || !c->connected || c->dropped || c->face_number != qport) continue;
      if (host_of(c->from) != host_of(from)) continue;
      c->from = from;
      face.counts.rebinds++;
      f.stats.rebinds++;
      gn::transport_send(*c->up, c->to, data, size, now);
      c->packets_up++;
      c->bytes_up += size;
      c->last_us = now;
      return true;
    }
  }
  return false;
}

bool client_packet(fw::Forward& f, ClassicFace& face, fw::HopClient& c, const uint8_t* data,
                   size_t size, int64_t now) {
  if (oob(data, size)) {
    if (text_of(data, size).rfind("rcon", 0) == 0) return false;  // goes up
    from_client(f, face, data, size, c.from, now);
    return true;
  }
  // A string command `drop` (after the sequence, the acknowledge and the qport): sent up three
  // times to outlast a loss, and the client freed.
  if (size >= 15 && data[10] == 4 && std::memcmp(data + 11, "drop", 4) == 0) {
    for (int i = 0; i < 3; i++) gn::transport_send(*c.up, c.to, data, size, now);
    face.counts.drops++;
    fw::forward_drop(f, c);
    return true;
  }
  return false;
}

std::string connect_line(const fw::HopClient& c, int64_t challenge) {
  return "connect " + std::to_string(kProtocol) + " " + std::to_string(c.face_number) + " " +
         std::to_string(challenge) + " \"" + c.face_text + "\"\n";
}

void from_upstream(fw::Forward& f, ClassicFace& face, fw::HopClient& c, const uint8_t* data,
                   size_t size, int64_t now) {
  if (!oob(data, size) || size < 5) return;
  const char kind = static_cast<char>(data[4]);
  if (kind == 'c' && c.face_step < kStepConnect) {
    const int64_t challenge = std::atoll(std::string(text_of(data, size).substr(1)).c_str());
    c.face_text = connect_line(c, challenge);
    c.face_step = kStepConnect;
    c.face_next_us = now;
    send_up(c, c.face_text, now);
    c.face_next_us = now + face.retry_us;
  } else if (kind == 'j' && c.face_step >= kStepConnect) {
    c.connected = true;
    face.counts.accepted++;
    f.stats.routed++;
    send_oob(f, c.from, "j", now);
  } else if (kind == 'n') {  // the destination's refusal, passed on in its words
    fw::forward_send(f, c.from, data, size, now);
    face.counts.refused++;
    f.stats.refused++;
    fw::forward_drop(f, c);
  }
}

void tick(fw::Forward& f, ClassicFace& face, fw::HopClient& c, int64_t now) {
  if (now < c.face_next_us) return;
  const int tries = c.face_step < kStepConnect ? c.face_step : c.face_step - kStepConnect;
  if (tries >= face.tries) {
    face.counts.timed_out++;
    f.stats.refused++;
    print_to(f, c.from, "No answer from " + gn::to_string(c.to) + ".", now);
    fw::forward_drop(f, c);
    return;
  }
  if (c.face_step < kStepConnect) {
    send_up(c, "getchallenge\n", now);
  } else if (tries > 0) {
    send_up(c, c.face_text, now);  // the connect again (the first went with the challenge)
  }
  c.face_step++;
  c.face_next_us = now + face.retry_us;
}

// The servers to ping: the face's own and every allowed destination with a port.
void know_servers(const fw::Forward& f, ClassicFace& face) {
  const auto add = [&](const gn::Address& a) {
    if (a.port == 0) return;
    for (const FaceServer& s : face.known)
      if (s.address == a) return;
    face.known.push_back({a});
  };
  for (const gn::Address& a : face.servers) add(a);
  for (const gn::Address& a : f.config.allow) add(a);
}

void probe(fw::Forward& f, ClassicFace& face, int64_t now) {
  if (face.known.empty() || now < face.next_probe_us) return;
  face.next_probe_us = now + face.probe_us;
  FaceServer& s = face.known[face.next_probe % face.known.size()];
  face.next_probe++;
  if (s.probed_us >= 0 && now - s.probed_us < face.every_us) return;
  s.probed_us = now;
  face.counts.probes++;
  send_oob(f, s.address, "k\n", now);
}

}  // namespace

bool classic_destination(const std::string& text, gn::Address& out) {
  if (text.empty()) return false;
  if (gn::parse_address(text, out)) return out.port != 0;
  const size_t colon = text.rfind(':');
  std::string host = text;
  int port = kPortServer;
  if (colon != std::string::npos) {
    host = text.substr(0, colon);
    port = std::atoi(text.c_str() + colon + 1);
    if (port < 1 || port > 65535) return false;
  }
  if (gn::parse_address(host + ":" + std::to_string(port), out)) return true;
  return gn::resolve_ipv4(host, static_cast<uint16_t>(port), out, nullptr);
}

std::string classic_forward_info(const fw::Forward& f, const ClassicFace& face) {
  std::string info;
  info_set(info, "*version", kVersion);
  info_set(info, "hostname", f.config.name);
  info_set(info, "maxclients", std::to_string(f.config.max_clients));
  if (!face.hostport.empty()) info_set(info, "hostport", face.hostport);
  if (!face.countrycode.empty()) info_set(info, "countrycode", face.countrycode);
  if (!face.city.empty()) info_set(info, "city", face.city);
  if (!face.coords.empty()) info_set(info, "coords", face.coords);
  return info;
}

void classic_forward_attach(fw::Forward& f, ClassicFace& face) {
  face.secret = mix(f.config.seed ^ 0x636c6173736963ull);
  know_servers(f, face);
  ClassicFace* p = &face;
  f.face.from_client = [p](fw::Forward& fo, const uint8_t* d, size_t n, const gn::Address& from,
                           int64_t now) { return from_client(fo, *p, d, n, from, now); };
  f.face.client_packet = [p](fw::Forward& fo, fw::HopClient& c, const uint8_t* d, size_t n,
                             int64_t now) { return client_packet(fo, *p, c, d, n, now); };
  f.face.from_upstream = [p](fw::Forward& fo, fw::HopClient& c, const uint8_t* d, size_t n,
                             int64_t now) { from_upstream(fo, *p, c, d, n, now); };
  f.face.tick = [p](fw::Forward& fo, fw::HopClient& c, int64_t now) { tick(fo, *p, c, now); };
  f.face.frame = [p](fw::Forward& fo, int64_t now) { probe(fo, *p, now); };
}

}  // namespace shooters_edge::net
