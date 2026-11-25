// TLS over caller-carried bytes with Mbed TLS (tls_stream.hpp): the only file that includes it.
//
// ge:summary Mbed TLS sessions over caller-carried bytes: the roots read once, a self-signed cert
// ge:keywords tls mbedtls x509 ctr-drbg entropy psa bio want-read session-ticket self-signed
// ge:skill net
// ge:test tests/net_tls_stream_test.cpp
#include "net/tls_stream.hpp"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "net/os_socket.hpp"

namespace ge::net {

namespace {

constexpr const char* kUntrusted = "SSL peer certificate or SSH remote key was not OK";
constexpr const char* kNoRoots = "Problem with the SSL CA cert (path? access rights?)";
constexpr const char* kConnect = "SSL connect error";

// The operating system's trusted roots, read on first use and kept for the process.
struct Roots {
  mbedtls_x509_crt crt;
  bool any = false;
};

bool read_bundle(mbedtls_x509_crt& crt, const char* path) {
  // A bundle may hold certificates the library cannot read: the rest still count.
  int r = mbedtls_x509_crt_parse_file(&crt, path);
  return r >= 0 && crt.version != 0;
}

Roots& system_roots() {
  static Roots* roots = [] {
    auto* r = new Roots;  // never freed: the roots live as long as the process
    mbedtls_x509_crt_init(&r->crt);
    if (const char* file = std::getenv("SSL_CERT_FILE"); file != nullptr && *file != 0) {
      r->any = read_bundle(r->crt, file);
      return r;
    }
    for (const auto& der : os::root_certificates())
      if (mbedtls_x509_crt_parse_der(&r->crt, der.data(), der.size()) == 0) r->any = true;
    if (r->any) return r;
    for (const char* path :
         {"/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt",
          "/etc/ssl/ca-bundle.pem", "/etc/pki/tls/cacert.pem", "/etc/ssl/cert.pem"}) {
      if (read_bundle(r->crt, path)) {
        r->any = true;
        break;
      }
    }
    return r;
  }();
  return *roots;
}

void crypto_once() {
  static std::once_flag once;
  std::call_once(once, [] { psa_crypto_init(); });
}

// A text Mbed TLS reads as PEM: its length counts the terminating zero.
const unsigned char* pem(const std::string& s) {
  return reinterpret_cast<const unsigned char*>(s.c_str());
}

// A random source seeded from the system's entropy.
struct Random {
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context drbg;
  bool ok = false;
  Random() {
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);
    ok = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, nullptr, 0) == 0;
  }
  ~Random() {
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);
  }
  Random(const Random&) = delete;
  Random& operator=(const Random&) = delete;
};

}  // namespace

struct TlsStream {
  Random random;
  mbedtls_ssl_config conf;
  mbedtls_ssl_context ssl;
  mbedtls_x509_crt own;  // ca_pem's certificates, or a server's own
  mbedtls_pk_context key;
  std::vector<uint8_t> in;  // wire bytes not yet read by the library
  size_t in_at = 0;
  std::vector<uint8_t> out;      // wire bytes for the socket
  std::vector<uint8_t> pending;  // plain bytes not yet written
  std::string error;
  TlsState state = TlsState::handshaking;
  std::vector<uint8_t> pin;  // a client's: the server certificate's SHA-256 it must have

  TlsStream() {
    mbedtls_ssl_config_init(&conf);
    mbedtls_ssl_init(&ssl);
    mbedtls_x509_crt_init(&own);
    mbedtls_pk_init(&key);
  }
  ~TlsStream() {
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_x509_crt_free(&own);
    mbedtls_pk_free(&key);
  }
  TlsStream(const TlsStream&) = delete;
  TlsStream& operator=(const TlsStream&) = delete;

  void fail(const char* why) {
    state = TlsState::failed;
    error = why;
  }
};

namespace {

int wire_send(void* ctx, const unsigned char* buf, size_t len) {
  auto* s = static_cast<TlsStream*>(ctx);
  s->out.insert(s->out.end(), buf, buf + len);
  return static_cast<int>(len);
}

int wire_recv(void* ctx, unsigned char* buf, size_t len) {
  auto* s = static_cast<TlsStream*>(ctx);
  size_t have = s->in.size() - s->in_at;
  if (have == 0) return MBEDTLS_ERR_SSL_WANT_READ;
  size_t n = std::min(have, len);
  std::memcpy(buf, s->in.data() + s->in_at, n);
  s->in_at += n;
  if (s->in_at == s->in.size()) {
    s->in.clear();
    s->in_at = 0;
  }
  return static_cast<int>(n);
}

bool want(int r) {
  return r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE ||
         r == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS || r == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS;
}

// The common half of a session: the random source, the defaults, the wire.
bool setup(TlsStream& s, int endpoint) {
  crypto_once();
  if (!s.random.ok || mbedtls_ssl_config_defaults(&s.conf, endpoint, MBEDTLS_SSL_TRANSPORT_STREAM,
                                                  MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    s.fail(kConnect);
    return false;
  }
  mbedtls_ssl_conf_rng(&s.conf, mbedtls_ctr_drbg_random, &s.random.drbg);
  return true;
}

bool start(TlsStream& s) {
  if (mbedtls_ssl_setup(&s.ssl, &s.conf) != 0) {
    s.fail(kConnect);
    return false;
  }
  mbedtls_ssl_set_bio(&s.ssl, &s, wire_send, wire_recv, nullptr);
  return true;
}

}  // namespace

std::shared_ptr<TlsStream> tls_client(const std::string& host, const TlsOptions& options) {
  auto s = std::make_shared<TlsStream>();
  if (!setup(*s, MBEDTLS_SSL_IS_CLIENT)) return s;
  if (!options.verify || options.pin.size() == 32) {
    // Pinned: any chain, the certificate itself checked once the handshake is done.
    mbedtls_ssl_conf_authmode(&s->conf, MBEDTLS_SSL_VERIFY_NONE);
    if (options.verify) s->pin = options.pin;
  } else if (!options.ca_pem.empty()) {
    if (mbedtls_x509_crt_parse(&s->own, pem(options.ca_pem), options.ca_pem.size() + 1) != 0) {
      s->fail(kNoRoots);
      return s;
    }
    mbedtls_ssl_conf_authmode(&s->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&s->conf, &s->own, nullptr);
  } else {
    Roots& roots = system_roots();
    if (!roots.any) {
      s->fail(kNoRoots);
      return s;
    }
    mbedtls_ssl_conf_authmode(&s->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&s->conf, &roots.crt, nullptr);
  }
  if (!start(*s)) return s;
  if (mbedtls_ssl_set_hostname(&s->ssl, host.c_str()) != 0) s->fail(kConnect);
  return s;
}

std::shared_ptr<TlsStream> tls_server(const std::string& cert_pem, const std::string& key_pem,
                                      std::string* error) {
  auto s = std::make_shared<TlsStream>();
  auto refuse = [&](const char* why) -> std::shared_ptr<TlsStream> {
    if (error != nullptr) *error = why;
    return nullptr;
  };
  if (!setup(*s, MBEDTLS_SSL_IS_SERVER)) return refuse("the TLS library did not start");
  if (mbedtls_x509_crt_parse(&s->own, pem(cert_pem), cert_pem.size() + 1) != 0)
    return refuse("the server's certificate is not a PEM certificate");
  if (mbedtls_pk_parse_key(&s->key, pem(key_pem), key_pem.size() + 1, nullptr, 0,
                           mbedtls_ctr_drbg_random, &s->random.drbg) != 0)
    return refuse("the server's key is not a PEM private key");
  if (mbedtls_ssl_conf_own_cert(&s->conf, &s->own, &s->key) != 0)
    return refuse("the server's key does not match its certificate");
  if (!start(*s)) return refuse("the TLS session did not start");
  return s;
}

bool tls_self_signed(const std::string& host, std::string& cert_pem, std::string& key_pem) {
  crypto_once();
  Random random;
  if (!random.ok) return false;
  mbedtls_pk_context key;
  mbedtls_pk_init(&key);
  mbedtls_x509write_cert crt;
  mbedtls_x509write_crt_init(&crt);
  bool ok = false;
  std::string name = "CN=" + host;
  static const unsigned char kSerial[] = {0x01};
  std::vector<unsigned char> buf(4096);
  do {
    if (mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0) break;
    if (mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key), mbedtls_ctr_drbg_random,
                            &random.drbg) != 0)
      break;
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, &key);
    mbedtls_x509write_crt_set_issuer_key(&crt, &key);
    if (mbedtls_x509write_crt_set_subject_name(&crt, name.c_str()) != 0 ||
        mbedtls_x509write_crt_set_issuer_name(&crt, name.c_str()) != 0 ||
        mbedtls_x509write_crt_set_serial_raw(&crt, const_cast<unsigned char*>(kSerial),
                                             sizeof kSerial) != 0 ||
        mbedtls_x509write_crt_set_validity(&crt, "20200101000000", "20991231235959") != 0 ||
        mbedtls_x509write_crt_set_basic_constraints(&crt, 1, -1) != 0)
      break;
    if (mbedtls_x509write_crt_pem(&crt, buf.data(), buf.size(), mbedtls_ctr_drbg_random,
                                  &random.drbg) != 0)
      break;
    cert_pem = reinterpret_cast<const char*>(buf.data());
    if (mbedtls_pk_write_key_pem(&key, buf.data(), buf.size()) != 0) break;
    key_pem = reinterpret_cast<const char*>(buf.data());
    ok = true;
  } while (false);
  mbedtls_x509write_crt_free(&crt);
  mbedtls_pk_free(&key);
  return ok;
}

void tls_put_wire(TlsStream& s, const uint8_t* data, size_t size) {
  s.in.insert(s.in.end(), data, data + size);
}

void tls_take_wire(TlsStream& s, std::vector<uint8_t>& out) {
  out.insert(out.end(), s.out.begin(), s.out.end());
  s.out.clear();
}

void tls_write(TlsStream& s, const uint8_t* data, size_t size) {
  s.pending.insert(s.pending.end(), data, data + size);
}

TlsState tls_step(TlsStream& s, std::vector<uint8_t>& plain) {
  if (s.state == TlsState::handshaking) {
    int r = mbedtls_ssl_handshake(&s.ssl);
    if (r == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
      s.fail(kUntrusted);
    } else if (r != 0 && !want(r)) {
      s.fail(kConnect);
    } else if (r == 0) {
      s.state = TlsState::open;
      if (!s.pin.empty()) {
        const mbedtls_x509_crt* peer = mbedtls_ssl_get_peer_cert(&s.ssl);
        uint8_t sum[32];
        if (peer != nullptr) sha256(peer->raw.p, peer->raw.len, sum);
        if (peer == nullptr || !std::equal(sum, sum + 32, s.pin.begin())) s.fail(kUntrusted);
      }
    }
  }
  if (s.state != TlsState::open) return s.state;
  size_t wrote = 0;  // taken off the front once, not record by record
  while (wrote < s.pending.size()) {
    int r = mbedtls_ssl_write(&s.ssl, s.pending.data() + wrote, s.pending.size() - wrote);
    if (r > 0) {
      wrote += static_cast<size_t>(r);
    } else if (want(r) || r == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
      break;
    } else {
      s.fail(kConnect);
      return s.state;
    }
  }
  s.pending.erase(s.pending.begin(), s.pending.begin() + static_cast<ptrdiff_t>(wrote));
  unsigned char buf[16384];
  for (;;) {
    int r = mbedtls_ssl_read(&s.ssl, buf, sizeof buf);
    if (r > 0) {
      plain.insert(plain.end(), buf, buf + r);
    } else if (r == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
      continue;
    } else if (want(r)) {
      break;
    } else if (r == 0 || r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
      s.state = TlsState::closed;
      break;
    } else {
      s.fail(kConnect);
      break;
    }
  }
  return s.state;
}

const std::string& tls_error(const TlsStream& s) { return s.error; }

bool tls_cert_sha256(const std::string& cert_pem, uint8_t out[32]) {
  crypto_once();
  mbedtls_x509_crt crt;
  mbedtls_x509_crt_init(&crt);
  const bool ok = mbedtls_x509_crt_parse(&crt, pem(cert_pem), cert_pem.size() + 1) == 0;
  if (ok) sha256(crt.raw.p, crt.raw.len, out);
  mbedtls_x509_crt_free(&crt);
  return ok;
}

void sha256(const uint8_t* data, size_t size, uint8_t out[32]) {
  mbedtls_sha256(data, size, out, 0);
}

}  // namespace ge::net
