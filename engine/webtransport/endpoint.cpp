// The WebTransport server and client (endpoint.hpp) over ngtcp2 and picotls: one `Conn` per QUIC
// connection holds the library's state, its streams' bytes and the HTTP/3 session on it; the
// server keeps them by connection id, the client has one.
//
// ge:summary The WebTransport endpoint over ngtcp2 and picotls: connections, streams, sessions
// ge:keywords webtransport quic ngtcp2 picotls connection stream datagram handshake tls
// ge:keywords certificate-hash idle-timeout connection-id
// ge:skill net
// ge:test tests/webtransport_endpoint_test.cpp
#include "webtransport/endpoint.hpp"

#include <chrono>
#include <cstring>
#include <deque>
#include <map>
#include <unordered_map>

#include "ngtcp2/ngtcp2.h"
#include "ngtcp2/ngtcp2_crypto.h"
#include "ngtcp2/ngtcp2_crypto_picotls.h"
#include "picotls.h"
#include "picotls/minicrypto.h"
#include "uECC.h"
#include "webtransport/h3.hpp"

namespace ge::webtransport {

namespace {

constexpr size_t kCidSize = 18;
constexpr size_t kPacketSize = 1452;
constexpr uint64_t kH3NoError = 0x100;
constexpr uint64_t kMaxStreamOut = uint64_t{64} * 1024;  // a stream's bytes we keep to send
const char kAlpn[] = "h3";

uint64_t ms_to_ns(uint64_t ms) { return ms * 1000000ull; }

void random_bytes(uint8_t* dest, size_t size) { ptls_minicrypto_random_bytes(dest, size); }

void on_rand(uint8_t* dest, size_t size, const ngtcp2_rand_ctx*) { random_bytes(dest, size); }

int uecc_random(uint8_t* dest, unsigned size) {
  random_bytes(dest, size);
  return 1;
}

// net::Address to and from ngtcp2's own socket address (NGTCP2_USE_GENERIC_SOCKADDR: no OS
// header here; the port is kept big-endian as the OS would).
ngtcp2_socklen to_sockaddr(const net::Address& a, ngtcp2_sockaddr_union& out) {
  std::memset(&out, 0, sizeof out);
  const uint16_t port_be = static_cast<uint16_t>((a.port >> 8) | (a.port << 8));
  if (a.family == net::Family::ipv6) {
    out.in6.sin6_family = NGTCP2_AF_INET6;
    out.in6.sin6_port = port_be;
    std::memcpy(&out.in6.sin6_addr, a.bytes, 16);  // the field's name is a macro on Windows
    return sizeof out.in6;
  }
  out.in.sin_family = NGTCP2_AF_INET;
  out.in.sin_port = port_be;
  uint8_t ip[4];
  if (a.family == net::Family::sim) {
    net::sim_to_ipv4(a, ip);
  } else {
    std::memcpy(ip, a.bytes, 4);
  }
  std::memcpy(&out.in.sin_addr, ip, 4);
  return sizeof out.in;
}

struct Path {
  ngtcp2_sockaddr_union local{}, remote{};
  ngtcp2_path path{};
  void set(const net::Address& l, const net::Address& r) {
    path.local.addrlen = to_sockaddr(l, local);
    path.local.addr = &local.sa;
    path.remote.addrlen = to_sockaddr(r, remote);
    path.remote.addr = &remote.sa;
    path.user_data = nullptr;
  }
};

struct Stream {
  std::vector<uint8_t> out;  // everything this side sends on it, kept until the stream ends
  size_t sent = 0;
  bool fin = false;  // `out` ends the stream
  bool fin_sent = false;
  bool blocked = false;
  h3::FrameReader in;
  bool typed = false;  // a unidirectional stream's type has been read
  uint64_t uni_type = 0;
  bool discard = false;  // a stream this side ignores
};

struct Owner;

// One QUIC connection and the one WebTransport session on it.
struct Conn {
  Owner* owner = nullptr;
  bool server = true;
  ngtcp2_conn* conn = nullptr;
  ngtcp2_crypto_conn_ref ref{};
  ngtcp2_crypto_picotls_ctx tls{};
  ptls_raw_extension_t exts[2]{};
  Path path;
  net::Address local, peer;
  std::map<int64_t, Stream> streams;
  std::deque<std::vector<uint8_t>> datagrams_out;
  std::vector<std::string> cids;  // the ids the owner finds it by
  bool dead = false;              // to be removed after the current call
  std::string why;
  bool control_open = false;
  bool peer_settings = false;
  int64_t session_stream = -1;
  bool session_open = false;
  uint32_t session = 0;
  std::string session_path, origin;
  // Client: the expected certificate and the server's key from it.
  std::array<uint8_t, 32> expect_hash{};
  std::array<uint8_t, 64> server_key{};
  std::string authority;
  bool request_sent = false;
  std::vector<std::vector<uint8_t>> datagrams_in;  // client: payloads not yet taken

  ~Conn() {
    if (tls.ptls) {
      ngtcp2_crypto_picotls_deconfigure_session(&tls);
      ptls_free(tls.ptls);
    }
    if (conn) ngtcp2_conn_del(conn);
  }
};

// What a Conn calls back into: the server or the client.
struct Owner {
  Send send;
  virtual ~Owner() = default;
  virtual void on_cid(Conn&, const std::string&) {}
  virtual void on_cid_gone(Conn&, const std::string&) {}
  virtual void on_request(Conn&, int64_t, const std::vector<h3::Field>&) {}
  virtual void on_response(Conn&, int) {}
  virtual void on_datagram(Conn&, const uint8_t*, size_t) {}
  virtual void on_session_end(Conn&, const std::string&) {}
  virtual void on_handshake(Conn&) {}
  virtual void on_settings(Conn&) {}
};

Conn* conn_of(void* user_data) { return static_cast<Conn*>(user_data); }

ngtcp2_conn* get_conn(ngtcp2_crypto_conn_ref* ref) { return conn_of(ref->user_data)->conn; }

void queue(Conn& c, int64_t id, const std::vector<uint8_t>& bytes, bool fin) {
  Stream& s = c.streams[id];
  if (s.out.size() + bytes.size() > kMaxStreamOut) {
    c.dead = true;
    c.why = "a stream's outgoing bytes passed " + std::to_string(kMaxStreamOut);
    return;
  }
  s.out.insert(s.out.end(), bytes.begin(), bytes.end());
  if (fin) s.fin = true;
}

void open_control(Conn& c) {
  if (c.control_open) return;
  int64_t id = -1;
  if (ngtcp2_conn_open_uni_stream(c.conn, &id, nullptr) != 0) {
    c.dead = true;
    c.why = "the control stream could not be opened";
    return;
  }
  c.control_open = true;
  queue(c, id, c.server ? h3::server_control_stream() : h3::client_control_stream(), false);
}

void end_session(Conn& c, const std::string& why) {
  if (c.session_open) {
    c.session_open = false;
    c.owner->on_session_end(c, why);
  }
  if (c.why.empty()) c.why = why;
}

// A unidirectional stream's bytes: its type, then (the control stream) frames.
void read_uni(Conn& c, int64_t id, Stream& s, const uint8_t* data, size_t size) {
  if (s.discard) return;
  s.in.take(data, size);
  if (!s.typed) {
    const uint8_t* p = s.in.pending.data();
    if (!h3::get_varint(p, p + s.in.pending.size(), s.uni_type)) return;
    s.in.pending.erase(s.in.pending.begin(), s.in.pending.begin() + (p - s.in.pending.data()));
    s.typed = true;
    if (s.uni_type != h3::kStreamControl) {
      s.discard = true;      // QPACK's streams carry nothing with no table; WebTransport's own
      s.in.pending.clear();  // streams are not used by the game
      return;
    }
  }
  uint64_t type = 0;
  std::vector<uint8_t> payload;
  while (s.in.next(type, payload)) {
    if (type == h3::kFrameSettings && !c.peer_settings) {
      std::vector<h3::Setting> settings;
      if (!h3::parse_settings(payload.data(), payload.size(), settings)) {
        c.dead = true;
        c.why = "the peer's SETTINGS frame is malformed";
        return;
      }
      if (h3::setting(settings, h3::kSettingH3Datagram, 0) != 1) {
        c.dead = true;
        c.why = "the peer does not take HTTP/3 datagrams";
        return;
      }
      c.peer_settings = true;
      c.owner->on_settings(c);
    }
  }
  if (s.in.failed) {
    c.dead = true;
    c.why = "a control frame on stream " + std::to_string(id) + " is too big";
  }
}

// A bidirectional stream's bytes: the session's CONNECT (server) or its answer (client); then the
// session's capsules, which this side does not need.
void read_bidi(Conn& c, int64_t id, Stream& s, const uint8_t* data, size_t size) {
  if (s.discard) return;
  s.in.take(data, size);
  if (!s.typed) {
    const uint8_t* p = s.in.pending.data();
    uint64_t first = 0;
    if (!h3::get_varint(p, p + s.in.pending.size(), first)) return;
    s.typed = true;
    if (first == h3::kSignalWebTransport) {  // a WebTransport stream: not used
      s.discard = true;
      s.in.pending.clear();
      return;
    }
  }
  uint64_t type = 0;
  std::vector<uint8_t> payload;
  while (s.in.next(type, payload)) {
    if (type != h3::kFrameHeaders) continue;
    std::vector<h3::Field> fields;
    std::string error;
    if (!h3::qpack_decode(payload.data(), payload.size(), fields, &error)) {
      c.dead = true;
      c.why = error;
      return;
    }
    if (c.server) {
      if (c.session_stream < 0 || c.session_stream == id) c.owner->on_request(c, id, fields);
    } else if (id == c.session_stream && !c.session_open) {
      c.owner->on_response(c, h3::response_status(fields));
    }
  }
  if (s.in.failed) {
    c.dead = true;
    c.why = "a frame on stream " + std::to_string(id) + " is too big";
  }
}

int on_recv_stream_data(ngtcp2_conn* conn, uint32_t flags, int64_t id, uint64_t,
                        const uint8_t* data, size_t size, void* user_data, void*) {
  Conn& c = *conn_of(user_data);
  ngtcp2_conn_extend_max_stream_offset(conn, id, size);
  ngtcp2_conn_extend_max_offset(conn, size);
  Stream& s = c.streams[id];
  if (id & 0x2) {
    read_uni(c, id, s, data, size);
  } else {
    read_bidi(c, id, s, data, size);
  }
  if ((flags & NGTCP2_STREAM_DATA_FLAG_FIN) && id == c.session_stream) {
    end_session(c, "the page closed the session");
    c.dead = true;
  }
  return 0;
}

int on_stream_close(ngtcp2_conn*, uint32_t, int64_t id, uint64_t, void* user_data, void*) {
  Conn& c = *conn_of(user_data);
  if (id == c.session_stream && c.session_open) {
    end_session(c, "the session's stream closed");
    c.dead = true;
  }
  c.streams.erase(id);
  return 0;
}

int on_stream_reset(ngtcp2_conn*, int64_t id, uint64_t, uint64_t, void* user_data, void*) {
  Conn& c = *conn_of(user_data);
  if (id == c.session_stream && c.session_open) {
    end_session(c, "the page reset the session");
    c.dead = true;
  }
  return 0;
}

int on_recv_datagram(ngtcp2_conn*, uint32_t, const uint8_t* data, size_t size, void* user_data) {
  Conn& c = *conn_of(user_data);
  uint64_t stream = 0;
  size_t offset = 0;
  if (!h3::read_datagram(data, size, stream, offset)) return 0;
  if (!c.session_open || static_cast<int64_t>(stream) != c.session_stream) return 0;
  c.owner->on_datagram(c, data + offset, size - offset);
  return 0;
}

int on_handshake_completed(ngtcp2_conn*, void* user_data) {
  Conn& c = *conn_of(user_data);
  open_control(c);
  c.owner->on_handshake(c);
  return 0;
}

int on_get_new_connection_id(ngtcp2_conn*, ngtcp2_cid* cid, uint8_t* token, size_t cidlen,
                             void* user_data) {
  Conn& c = *conn_of(user_data);
  random_bytes(cid->data, cidlen);
  cid->datalen = cidlen;
  random_bytes(token, NGTCP2_STATELESS_RESET_TOKENLEN);
  const std::string key(reinterpret_cast<const char*>(cid->data), cid->datalen);
  c.cids.push_back(key);
  c.owner->on_cid(c, key);
  return 0;
}

int on_remove_connection_id(ngtcp2_conn*, const ngtcp2_cid* cid, void* user_data) {
  Conn& c = *conn_of(user_data);
  const std::string key(reinterpret_cast<const char*>(cid->data), cid->datalen);
  c.owner->on_cid_gone(c, key);
  return 0;
}

int on_extend_max_stream_data(ngtcp2_conn*, int64_t id, uint64_t, void* user_data, void*) {
  Conn& c = *conn_of(user_data);
  auto it = c.streams.find(id);
  if (it != c.streams.end()) it->second.blocked = false;
  return 0;
}

ngtcp2_callbacks callbacks(bool server) {
  ngtcp2_callbacks cb{};
  if (server) {
    cb.recv_client_initial = ngtcp2_crypto_recv_client_initial_cb;
  } else {
    cb.client_initial = ngtcp2_crypto_client_initial_cb;
    cb.recv_retry = ngtcp2_crypto_recv_retry_cb;
  }
  cb.recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb;
  cb.encrypt = ngtcp2_crypto_encrypt_cb;
  cb.decrypt = ngtcp2_crypto_decrypt_cb;
  cb.hp_mask = ngtcp2_crypto_hp_mask_cb;
  cb.update_key = ngtcp2_crypto_update_key_cb;
  cb.delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb;
  cb.delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb;
  cb.get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb;
  cb.version_negotiation = ngtcp2_crypto_version_negotiation_cb;
  cb.rand = on_rand;
  cb.get_new_connection_id = on_get_new_connection_id;
  cb.remove_connection_id = on_remove_connection_id;
  cb.recv_stream_data = on_recv_stream_data;
  cb.stream_close = on_stream_close;
  cb.stream_reset = on_stream_reset;
  cb.recv_datagram = on_recv_datagram;
  cb.handshake_completed = on_handshake_completed;
  cb.extend_max_stream_data = on_extend_max_stream_data;
  return cb;
}

void transport_params(ngtcp2_transport_params& p, uint64_t idle_ms) {
  ngtcp2_transport_params_default(&p);
  p.initial_max_stream_data_bidi_local = uint64_t{256} * 1024;
  p.initial_max_stream_data_bidi_remote = uint64_t{256} * 1024;
  p.initial_max_stream_data_uni = uint64_t{256} * 1024;
  p.initial_max_data = uint64_t{1024} * 1024;
  p.initial_max_streams_bidi = 16;
  p.initial_max_streams_uni = 16;
  p.max_idle_timeout = ms_to_ns(idle_ms);
  p.active_connection_id_limit = 4;
  p.max_datagram_frame_size = 65535;
}

// Sends everything the connection has: streams first, then datagrams, then whatever QUIC wants
// (acknowledgements, retransmissions). False when the connection failed.
bool flush(Conn& c, uint64_t now) {
  if (c.dead || !c.conn) return false;
  uint8_t buf[kPacketSize];
  ngtcp2_pkt_info pi{};
  for (int guard = 0; guard < 256; ++guard) {
    ngtcp2_ssize n = 0;
    // A stream with bytes to send.
    int64_t id = -1;
    Stream* s = nullptr;
    for (auto& [sid, st] : c.streams) {
      if (st.blocked) continue;
      if (st.sent < st.out.size() || (st.fin && !st.fin_sent)) {
        id = sid;
        s = &st;
        break;
      }
    }
    if (s) {
      ngtcp2_vec v{s->out.data() + s->sent, s->out.size() - s->sent};
      ngtcp2_ssize taken = -1;
      const uint32_t flags = s->fin ? NGTCP2_WRITE_STREAM_FLAG_FIN : 0;
      n = ngtcp2_conn_writev_stream(c.conn, &c.path.path, &pi, buf, sizeof buf, &taken, flags, id,
                                    &v, 1, now);
      if (n == NGTCP2_ERR_STREAM_DATA_BLOCKED || n == NGTCP2_ERR_STREAM_SHUT_WR ||
          n == NGTCP2_ERR_STREAM_NOT_FOUND) {
        s->blocked = true;
        if (n != NGTCP2_ERR_STREAM_DATA_BLOCKED) s->out.clear(), s->sent = 0, s->fin = false;
        continue;
      }
      if (taken >= 0) {
        s->sent += static_cast<size_t>(taken);
        if (s->fin && s->sent == s->out.size()) s->fin_sent = true;
      }
    } else if (!c.datagrams_out.empty()) {
      const std::vector<uint8_t>& d = c.datagrams_out.front();
      ngtcp2_vec v{const_cast<uint8_t*>(d.data()), d.size()};
      int accepted = 0;
      n = ngtcp2_conn_writev_datagram(c.conn, &c.path.path, &pi, buf, sizeof buf, &accepted, 0, 0,
                                      &v, 1, now);
      if (accepted || n == NGTCP2_ERR_INVALID_ARGUMENT) c.datagrams_out.pop_front();
      if (n == NGTCP2_ERR_INVALID_ARGUMENT) continue;
    } else {
      n = ngtcp2_conn_write_pkt(c.conn, &c.path.path, &pi, buf, sizeof buf, now);
    }
    if (n < 0) {
      c.dead = true;
      if (c.why.empty()) c.why = std::string("QUIC: ") + ngtcp2_strerror(static_cast<int>(n));
      return false;
    }
    if (n == 0) break;
    c.owner->send(c.peer, buf, static_cast<size_t>(n));
  }
  ngtcp2_conn_update_pkt_tx_time(c.conn, now);
  return true;
}

// A last packet that tells the peer the connection is over (`app`: an HTTP/3 code with `reason`,
// the words the other end reports, else QUIC's own error `liberr`).
void send_close(Conn& c, uint64_t now, int liberr, bool app, std::string_view reason = {}) {
  if (!c.conn || ngtcp2_conn_in_closing_period(c.conn) || ngtcp2_conn_in_draining_period(c.conn)) {
    return;
  }
  ngtcp2_ccerr err;
  ngtcp2_ccerr_default(&err);
  if (app) {
    reason = reason.substr(0, 200);  // a close fits one packet
    ngtcp2_ccerr_set_application_error(
        &err, kH3NoError, reinterpret_cast<const uint8_t*>(reason.data()), reason.size());
  } else if (liberr == NGTCP2_ERR_CRYPTO) {
    ngtcp2_ccerr_set_tls_alert(&err, ngtcp2_conn_get_tls_alert(c.conn), nullptr, 0);
  } else {
    ngtcp2_ccerr_set_liberr(&err, liberr, nullptr, 0);
  }
  uint8_t buf[kPacketSize];
  ngtcp2_pkt_info pi{};
  const ngtcp2_ssize n =
      ngtcp2_conn_write_connection_close(c.conn, &c.path.path, &pi, buf, sizeof buf, &err, now);
  if (n > 0) c.owner->send(c.peer, buf, static_cast<size_t>(n));
}

// Reads one packet; false when the connection is over (`dead` set, a close sent when owed).
bool read_packet(Conn& c, const uint8_t* data, size_t size, uint64_t now) {
  ngtcp2_pkt_info pi{};
  const int rv = ngtcp2_conn_read_pkt(c.conn, &c.path.path, &pi, data, size, now);
  if (rv != 0) {
    if (rv != NGTCP2_ERR_DRAINING && rv != NGTCP2_ERR_DROP_CONN) send_close(c, now, rv, false);
    c.dead = true;
    // The peer closed: its own words, when it gave any.
    const ngtcp2_ccerr* peer = rv == NGTCP2_ERR_DRAINING ? ngtcp2_conn_get_ccerr(c.conn) : nullptr;
    if (c.why.empty() && peer != nullptr && peer->reasonlen > 0)
      c.why.assign(reinterpret_cast<const char*>(peer->reason), peer->reasonlen);
    if (c.why.empty()) c.why = std::string("QUIC: ") + ngtcp2_strerror(rv);
    return false;
  }
  return !c.dead;
}

bool run_timers(Conn& c, uint64_t now) {
  if (ngtcp2_conn_get_expiry(c.conn) > now) return true;
  const int rv = ngtcp2_conn_handle_expiry(c.conn, now);
  if (rv != 0) {
    if (rv != NGTCP2_ERR_IDLE_CLOSE) send_close(c, now, rv, false);
    c.dead = true;
    if (c.why.empty()) {
      c.why = rv == NGTCP2_ERR_IDLE_CLOSE ? "the connection was silent too long"
                                          : std::string("QUIC: ") + ngtcp2_strerror(rv);
    }
    return false;
  }
  return true;
}

int on_client_hello(ptls_on_client_hello_t*, ptls_t* tls, ptls_on_client_hello_parameters_t* p) {
  for (size_t i = 0; i < p->negotiated_protocols.count; ++i) {
    const ptls_iovec_t& proto = p->negotiated_protocols.list[i];
    if (proto.len == 2 && std::memcmp(proto.base, kAlpn, 2) == 0) {
      return ptls_set_negotiated_protocol(tls, kAlpn, 2) == 0 ? 0 : -1;
    }
  }
  return PTLS_ALERT_NO_APPLICATION_PROTOCOL;
}

ptls_on_client_hello_t g_on_client_hello = {on_client_hello};

ptls_key_exchange_algorithm_t* g_key_exchanges[] = {&ptls_minicrypto_x25519,
                                                    &ptls_minicrypto_secp256r1, nullptr};
ptls_cipher_suite_t* g_cipher_suites[] = {&ptls_minicrypto_aes128gcmsha256,
                                          &ptls_minicrypto_aes256gcmsha384,
                                          &ptls_minicrypto_chacha20poly1305sha256, nullptr};

}  // namespace

// --- the server ---------------------------------------------------------------------------------

struct Server::Impl : Owner {
  ServerConfig config;
  Certificate cert;
  ptls_iovec_t cert_list[1]{};
  ptls_minicrypto_secp256r1sha256_sign_certificate_t signer{};
  ptls_context_t ctx{};
  net::Address local = net::ipv4(127, 0, 0, 1, 0);
  std::unordered_map<std::string, Conn*> by_cid;
  std::vector<std::unique_ptr<Conn>> conns;
  std::unordered_map<uint32_t, Conn*> by_session;
  uint32_t next_session = 1;
  std::vector<Event> events;
  uint64_t now = 0;
  bool started = false;

  void on_cid(Conn& c, const std::string& key) override { by_cid[key] = &c; }
  void on_cid_gone(Conn& c, const std::string& key) override {
    auto it = by_cid.find(key);
    if (it != by_cid.end() && it->second == &c) by_cid.erase(it);
  }
  void on_request(Conn& c, int64_t id, const std::vector<h3::Field>& fields) override {
    h3::Request req;
    std::string error;
    const bool ok = h3::read_request(fields, req, &error);
    int status = 200;
    if (!ok) {
      status = 400;
    } else if (req.path.compare(0, config.path_prefix.size(), config.path_prefix) != 0) {
      status = 404;
    } else if (c.session_open) {
      status = 429;
    }
    queue(c, id, h3::response_frame(status), status != 200);
    if (status != 200) return;
    c.session_stream = id;
    c.session_open = true;
    c.session = next_session++;
    if (next_session == 0) next_session = 1;
    c.session_path = req.path;
    c.origin = req.origin;
    by_session[c.session] = &c;
    Event e;
    e.kind = Event::Kind::opened;
    e.session = c.session;
    e.peer = c.peer;
    e.path = req.path;
    e.origin = req.origin;
    events.push_back(std::move(e));
  }
  void on_datagram(Conn& c, const uint8_t* data, size_t size) override {
    Event e;
    e.kind = Event::Kind::datagram;
    e.session = c.session;
    e.peer = c.peer;
    e.data.assign(data, data + size);
    events.push_back(std::move(e));
  }
  void on_session_end(Conn& c, const std::string& why) override {
    by_session.erase(c.session);
    Event e;
    e.kind = Event::Kind::closed;
    e.session = c.session;
    e.peer = c.peer;
    e.why = why;
    events.push_back(std::move(e));
  }

  Conn* accept(const net::Address& from, const uint8_t* data, size_t size) {
    ngtcp2_pkt_hd hd;
    if (ngtcp2_accept(&hd, data, size) != 0) return nullptr;
    if (conns.size() >= config.max_connections) return nullptr;
    auto c = std::make_unique<Conn>();
    c->owner = this;
    c->server = true;
    c->local = local;
    c->peer = from;
    c->path.set(local, from);
    ngtcp2_cid scid;
    scid.datalen = kCidSize;
    random_bytes(scid.data, scid.datalen);
    ngtcp2_settings settings;
    ngtcp2_settings_default(&settings);
    settings.initial_ts = now;
    settings.handshake_timeout = ms_to_ns(10000);
    ngtcp2_transport_params params;
    transport_params(params, config.idle_ms);
    params.original_dcid = hd.dcid;
    params.original_dcid_present = 1;
    params.stateless_reset_token_present = 1;
    random_bytes(params.stateless_reset_token, sizeof params.stateless_reset_token);
    const ngtcp2_callbacks cb = callbacks(true);
    if (ngtcp2_conn_server_new(&c->conn, &hd.scid, &scid, &c->path.path, hd.version, &cb, &settings,
                               &params, nullptr, c.get()) != 0) {
      return nullptr;
    }
    c->ref.get_conn = get_conn;
    c->ref.user_data = c.get();
    ngtcp2_crypto_picotls_ctx_init(&c->tls);
    c->tls.ptls = ptls_server_new(&ctx);
    if (!c->tls.ptls) return nullptr;
    *ptls_get_data_ptr(c->tls.ptls) = &c->ref;
    c->exts[0].type = UINT16_MAX;
    c->exts[1].type = UINT16_MAX;
    c->tls.handshake_properties.additional_extensions = c->exts;
    if (ngtcp2_crypto_picotls_configure_server_session(&c->tls) != 0) return nullptr;
    ngtcp2_conn_set_tls_native_handle(c->conn, &c->tls);
    const std::string orig(reinterpret_cast<const char*>(hd.dcid.data), hd.dcid.datalen);
    const std::string own(reinterpret_cast<const char*>(scid.data), scid.datalen);
    c->cids = {orig, own};
    by_cid[orig] = c.get();
    by_cid[own] = c.get();
    conns.push_back(std::move(c));
    return conns.back().get();
  }

  void reap() {
    for (size_t i = 0; i < conns.size();) {
      Conn& c = *conns[i];
      if (!c.dead) {
        ++i;
        continue;
      }
      end_session(c, c.why.empty() ? "the connection closed" : c.why);
      for (const std::string& key : c.cids) {
        auto it = by_cid.find(key);
        if (it != by_cid.end() && it->second == &c) by_cid.erase(it);
      }
      by_session.erase(c.session);
      conns.erase(conns.begin() + static_cast<std::ptrdiff_t>(i));
    }
  }
};

Server::Server() : impl_(std::make_unique<Impl>()) {}
Server::~Server() = default;

bool Server::start(const Certificate& cert, const ServerConfig& config, Send send,
                   std::string* error) {
  Impl& s = *impl_;
  if (cert.der.empty()) {
    if (error) *error = "webtransport: the server was given no certificate; make one first";
    return false;
  }
  uECC_set_rng(uecc_random);
  s.config = config;
  s.cert = cert;
  s.send = std::move(send);
  s.cert_list[0] = ptls_iovec_init(s.cert.der.data(), s.cert.der.size());
  if (ptls_minicrypto_init_secp256r1sha256_sign_certificate(
          &s.signer, ptls_iovec_init(s.cert.key.data(), s.cert.key.size())) != 0) {
    if (error) *error = "webtransport: the certificate's key was refused";
    return false;
  }
  s.ctx = ptls_context_t{};
  s.ctx.random_bytes = ptls_minicrypto_random_bytes;
  s.ctx.get_time = &ptls_get_time;
  s.ctx.key_exchanges = g_key_exchanges;
  s.ctx.cipher_suites = g_cipher_suites;
  s.ctx.certificates.list = s.cert_list;
  s.ctx.certificates.count = 1;
  s.ctx.sign_certificate = &s.signer.super;
  s.ctx.on_client_hello = &g_on_client_hello;
  s.ctx.require_dhe_on_psk = 1;
  if (ngtcp2_crypto_picotls_configure_server_context(&s.ctx) != 0) {
    if (error) *error = "webtransport: the TLS context could not be set up for QUIC";
    return false;
  }
  s.started = true;
  return true;
}

void Server::set_local(const net::Address& local) { impl_->local = local; }

void Server::receive(const net::Address& from, const uint8_t* data, size_t size, uint64_t now_ns) {
  Impl& s = *impl_;
  if (!s.started) return;
  s.now = now_ns;
  ngtcp2_version_cid vc;
  if (ngtcp2_pkt_decode_version_cid(&vc, data, size, kCidSize) != 0) return;
  const std::string key(reinterpret_cast<const char*>(vc.dcid), vc.dcidlen);
  Conn* c = nullptr;
  auto it = s.by_cid.find(key);
  if (it != s.by_cid.end()) {
    c = it->second;
  } else {
    c = s.accept(from, data, size);
    if (!c) return;
  }
  if (c->dead) return;
  if (from != c->peer) {  // the page's NAT moved it: QUIC validates the new path
    c->peer = from;
    c->path.set(c->local, from);
  }
  if (read_packet(*c, data, size, now_ns)) flush(*c, now_ns);
  s.reap();
}

void Server::update(uint64_t now_ns) {
  Impl& s = *impl_;
  s.now = now_ns;
  for (auto& c : s.conns) {
    if (!c->dead && run_timers(*c, now_ns)) flush(*c, now_ns);
  }
  s.reap();
}

bool Server::send(uint32_t session, const uint8_t* data, size_t size) {
  Impl& s = *impl_;
  if (size > kMaxDatagram) return false;
  auto it = s.by_session.find(session);
  if (it == s.by_session.end() || it->second->dead) return false;
  Conn& c = *it->second;
  if (c.datagrams_out.size() >= 256) c.datagrams_out.pop_front();  // the oldest is stale anyway
  std::vector<uint8_t> d;
  h3::put_datagram(d, static_cast<uint64_t>(c.session_stream), data, size);
  c.datagrams_out.push_back(std::move(d));
  return true;
}

void Server::close(uint32_t session, std::string_view why) {
  Impl& s = *impl_;
  auto it = s.by_session.find(session);
  if (it == s.by_session.end()) return;
  Conn& c = *it->second;
  flush(c, s.now);
  send_close(c, s.now, 0, true, why);
  end_session(c, std::string(why));
  c.dead = true;
  s.reap();
}

std::vector<Event> Server::take_events() {
  std::vector<Event> out;
  out.swap(impl_->events);
  return out;
}

size_t Server::connections() const { return impl_->conns.size(); }
size_t Server::sessions() const { return impl_->by_session.size(); }

// --- the client ---------------------------------------------------------------------------------

namespace {

// The handshake's signature over the transcript, checked against the certificate's key.
int verify_sign(void* verify_ctx, uint16_t algo, ptls_iovec_t data, ptls_iovec_t sign) {
  if (!data.base) return 0;  // the handshake failed before it: nothing to free
  if (algo != PTLS_SIGNATURE_ECDSA_SECP256R1_SHA256) return PTLS_ALERT_ILLEGAL_PARAMETER;
  const Conn& c = *static_cast<const Conn*>(verify_ctx);
  // DER: SEQUENCE { INTEGER r, INTEGER s } into 32 + 32 bytes.
  uint8_t rs[64] = {};
  const uint8_t* p = sign.base;
  const uint8_t* end = sign.base + sign.len;
  if (end - p < 2 || p[0] != 0x30) return PTLS_ALERT_DECRYPT_ERROR;
  p += 2;
  for (int i = 0; i < 2; ++i) {
    if (end - p < 2 || p[0] != 0x02) return PTLS_ALERT_DECRYPT_ERROR;
    size_t len = p[1];
    p += 2;
    if (static_cast<size_t>(end - p) < len) return PTLS_ALERT_DECRYPT_ERROR;
    const uint8_t* v = p;
    p += len;
    while (len > 32 && *v == 0) ++v, --len;
    if (len > 32) return PTLS_ALERT_DECRYPT_ERROR;
    std::memcpy(rs + size_t{32} * static_cast<size_t>(i) + (32 - len), v, len);
  }
  const std::array<uint8_t, 32> digest = sha256(data.base, data.len);
  return uECC_verify(c.server_key.data(), digest.data(), static_cast<unsigned>(digest.size()), rs,
                     uECC_secp256r1())
             ? 0
             : PTLS_ALERT_DECRYPT_ERROR;
}

int verify_certificate(ptls_verify_certificate_t*, ptls_t* tls, const char*,
                       int (**sign_cb)(void*, uint16_t, ptls_iovec_t, ptls_iovec_t),
                       void** sign_data, ptls_iovec_t* certs, size_t count) {
  auto* ref = static_cast<ngtcp2_crypto_conn_ref*>(*ptls_get_data_ptr(tls));
  Conn& c = *conn_of(ref->user_data);
  if (count < 1) return PTLS_ALERT_BAD_CERTIFICATE;
  if (sha256(certs[0].base, certs[0].len) != c.expect_hash) {
    c.why = "the server's certificate is not the one its hash names";
    return PTLS_ALERT_BAD_CERTIFICATE;
  }
  if (!certificate_public_key(certs[0].base, certs[0].len, c.server_key)) {
    c.why = "the server's certificate has no P-256 key";
    return PTLS_ALERT_BAD_CERTIFICATE;
  }
  *sign_cb = verify_sign;
  *sign_data = &c;
  return 0;
}

const uint16_t g_verify_algos[] = {PTLS_SIGNATURE_ECDSA_SECP256R1_SHA256, UINT16_MAX};
ptls_verify_certificate_t g_verifier = {verify_certificate, g_verify_algos};

}  // namespace

struct Client::Impl : Owner {
  ptls_context_t ctx{};
  std::unique_ptr<Conn> conn;
  State state = State::idle;
  std::string path;
  std::string why;
  uint64_t now = 0;
  ptls_iovec_t alpn[1] = {{reinterpret_cast<uint8_t*>(const_cast<char*>(kAlpn)), 2}};

  void on_settings(Conn& c) override { send_request(c); }
  void on_handshake(Conn& c) override { send_request(c); }
  void send_request(Conn& c) {
    // The CONNECT waits for both: the handshake (streams can open) and the server's SETTINGS
    // (extended CONNECT is on).
    if (c.request_sent || !c.peer_settings || !c.control_open) return;
    int64_t id = -1;
    if (ngtcp2_conn_open_bidi_stream(c.conn, &id, nullptr) != 0) {
      c.dead = true;
      c.why = "the session's stream could not be opened";
      return;
    }
    c.request_sent = true;
    c.session_stream = id;
    queue(c, id, h3::request_frame(c.authority, path, "https://localhost"), false);
  }
  void on_response(Conn& c, int status) override {
    if (status != 200) {
      c.dead = true;
      c.why = "the server answered the session's CONNECT with " + std::to_string(status);
      return;
    }
    c.session_open = true;
    state = State::open;
  }
  void on_datagram(Conn& c, const uint8_t* data, size_t size) override {
    if (c.datagrams_in.size() >= 1024) c.datagrams_in.erase(c.datagrams_in.begin());
    c.datagrams_in.emplace_back(data, data + size);
  }
  void settle() {
    if (conn && conn->dead) {
      why = conn->why.empty() ? "the connection closed" : conn->why;
      state = State::closed;
    }
  }
};

Client::Client() : impl_(std::make_unique<Impl>()) {}
Client::~Client() = default;

bool Client::connect(const net::Address& local, const net::Address& server,
                     std::string_view authority, std::string_view path,
                     const std::array<uint8_t, 32>& hash, Send send, uint64_t now_ns,
                     std::string* error) {
  Impl& s = *impl_;
  auto fail = [&](const char* what) {
    if (error) *error = what;
    s.conn.reset();
    s.state = State::closed;
    s.why = what;
    return false;
  };
  uECC_set_rng(uecc_random);
  s.send = std::move(send);
  s.path = std::string(path);
  s.now = now_ns;
  s.ctx = ptls_context_t{};
  s.ctx.random_bytes = ptls_minicrypto_random_bytes;
  s.ctx.get_time = &ptls_get_time;
  s.ctx.key_exchanges = g_key_exchanges;
  s.ctx.cipher_suites = g_cipher_suites;
  s.ctx.verify_certificate = &g_verifier;
  s.ctx.require_dhe_on_psk = 1;
  if (ngtcp2_crypto_picotls_configure_client_context(&s.ctx) != 0) {
    return fail("webtransport: the client's TLS context could not be set up for QUIC");
  }
  s.conn = std::make_unique<Conn>();
  Conn& c = *s.conn;
  c.owner = &s;
  c.server = false;
  c.local = local;
  c.peer = server;
  c.path.set(local, server);
  c.expect_hash = hash;
  c.authority = std::string(authority);
  ngtcp2_cid dcid, scid;
  dcid.datalen = kCidSize;
  scid.datalen = kCidSize;
  random_bytes(dcid.data, dcid.datalen);
  random_bytes(scid.data, scid.datalen);
  ngtcp2_settings settings;
  ngtcp2_settings_default(&settings);
  settings.initial_ts = now_ns;
  settings.handshake_timeout = ms_to_ns(10000);
  ngtcp2_transport_params params;
  transport_params(params, 30000);
  const ngtcp2_callbacks cb = callbacks(false);
  if (ngtcp2_conn_client_new(&c.conn, &dcid, &scid, &c.path.path, NGTCP2_PROTO_VER_V1, &cb,
                             &settings, &params, nullptr, &c) != 0) {
    return fail("webtransport: the QUIC connection could not be made");
  }
  c.ref.get_conn = get_conn;
  c.ref.user_data = &c;
  ngtcp2_crypto_picotls_ctx_init(&c.tls);
  c.tls.ptls = ptls_client_new(&s.ctx);
  if (!c.tls.ptls) return fail("webtransport: the TLS session could not be made");
  *ptls_get_data_ptr(c.tls.ptls) = &c.ref;
  c.exts[0].type = UINT16_MAX;
  c.exts[1].type = UINT16_MAX;
  c.tls.handshake_properties.additional_extensions = c.exts;
  if (ngtcp2_crypto_picotls_configure_client_session(&c.tls, c.conn) != 0) {
    return fail("webtransport: the TLS session could not be set up for QUIC");
  }
  c.tls.handshake_properties.client.negotiated_protocols.list = s.alpn;
  c.tls.handshake_properties.client.negotiated_protocols.count = 1;
  ptls_set_server_name(c.tls.ptls, "localhost", 9);
  ngtcp2_conn_set_tls_native_handle(c.conn, &c.tls);
  s.state = State::connecting;
  flush(c, now_ns);
  s.settle();
  return s.state != State::closed;
}

void Client::receive(const net::Address& from, const uint8_t* data, size_t size, uint64_t now_ns) {
  Impl& s = *impl_;
  if (!s.conn || s.conn->dead || from != s.conn->peer) return;
  s.now = now_ns;
  if (read_packet(*s.conn, data, size, now_ns)) flush(*s.conn, now_ns);
  s.settle();
}

void Client::update(uint64_t now_ns) {
  Impl& s = *impl_;
  if (!s.conn || s.conn->dead) return;
  s.now = now_ns;
  if (run_timers(*s.conn, now_ns)) flush(*s.conn, now_ns);
  s.settle();
}

bool Client::send(const uint8_t* data, size_t size) {
  Impl& s = *impl_;
  if (s.state != State::open || size > kMaxDatagram) return false;
  Conn& c = *s.conn;
  std::vector<uint8_t> d;
  h3::put_datagram(d, static_cast<uint64_t>(c.session_stream), data, size);
  c.datagrams_out.push_back(std::move(d));
  return true;
}

void Client::close(std::string_view why) {
  Impl& s = *impl_;
  if (!s.conn || s.conn->dead) return;
  flush(*s.conn, s.now);
  send_close(*s.conn, s.now, 0, true, why);
  s.conn->dead = true;
  s.conn->why = std::string(why);
  s.settle();
}

Client::State Client::state() const { return impl_->state; }

std::vector<std::vector<uint8_t>> Client::take_datagrams() {
  std::vector<std::vector<uint8_t>> out;
  if (impl_->conn) out.swap(impl_->conn->datagrams_in);
  return out;
}

const std::string& Client::why() const { return impl_->why; }

// --- the socket ---------------------------------------------------------------------------------

void pump(net::UdpSocket& socket, Server& server, uint64_t now_ns) {
  uint8_t buf[2048];
  net::Address from;
  for (int i = 0; i < 1024; ++i) {
    const int n = net::udp_receive(socket, buf, sizeof buf, from);
    if (n < 0) break;
    server.receive(from, buf, static_cast<size_t>(n), now_ns);
  }
  server.update(now_ns);
}

void pump(net::UdpSocket& socket, Client& client, uint64_t now_ns) {
  uint8_t buf[2048];
  net::Address from;
  for (int i = 0; i < 1024; ++i) {
    const int n = net::udp_receive(socket, buf, sizeof buf, from);
    if (n < 0) break;
    client.receive(from, buf, static_cast<size_t>(n), now_ns);
  }
  client.update(now_ns);
}

uint64_t now_ns() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

}  // namespace ge::webtransport
