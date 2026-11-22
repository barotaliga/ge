// engine/net/tls_stream.hpp: a client and a server session carried to each other in memory (no
// socket): the handshake on a certificate made on the spot, bytes both ways, a client that trusts
// only that certificate, one that trusts another and fails in the words a player reads, one asked
// for another name, and one that checks nothing.
//
// ge:summary Test: TLS sessions in memory: handshake, bytes both ways, trust and names refused
// ge:keywords test net tls mbedtls self-signed handshake certificate trust insecure
// ge:skill net testing
#include "net/tls_stream.hpp"

#include <string>
#include <vector>

#include "check.hpp"

using namespace ge::net;

namespace {

struct Pair {
  std::shared_ptr<TlsStream> client, server;
  std::string got_client, got_server;
};

// Carries the wire bytes between the two until both are open or one failed.
void carry(Pair& p, int rounds = 200) {
  std::vector<uint8_t> wire, plain;
  for (int i = 0; i < rounds; ++i) {
    plain.clear();
    TlsState c = tls_step(*p.client, plain);
    p.got_client.append(plain.begin(), plain.end());
    wire.clear();
    tls_take_wire(*p.client, wire);
    tls_put_wire(*p.server, wire.data(), wire.size());
    plain.clear();
    TlsState s = tls_step(*p.server, plain);
    p.got_server.append(plain.begin(), plain.end());
    wire.clear();
    tls_take_wire(*p.server, wire);
    tls_put_wire(*p.client, wire.data(), wire.size());
    if (c == TlsState::failed || s == TlsState::failed) return;
  }
}

void say(TlsStream& s, const std::string& text) {
  tls_write(s, reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

}  // namespace

TEST(tls_handshake_and_bytes_both_ways) {
  std::string cert, key;
  CHECK(tls_self_signed("localhost", cert, key));
  CHECK(cert.rfind("-----BEGIN CERTIFICATE-----", 0) == 0);
  std::string err;
  Pair p;
  p.server = tls_server(cert, key, &err);
  CHECK_MSG(p.server != nullptr, err);
  TlsOptions o;
  o.ca_pem = cert;
  p.client = tls_client("localhost", o);
  say(*p.client, "GET / HTTP/1.1\r\n\r\n");  // queued until the handshake is done
  carry(p);
  CHECK_MSG(tls_error(*p.client).empty(), tls_error(*p.client));
  CHECK(p.got_server == "GET / HTTP/1.1\r\n\r\n");
  say(*p.server, "HTTP/1.1 200 OK\r\n\r\nhello");
  carry(p);
  CHECK(p.got_client == "HTTP/1.1 200 OK\r\n\r\nhello");
  std::vector<uint8_t> plain;
  CHECK(tls_step(*p.client, plain) == TlsState::open);
}

TEST(tls_refuses_an_untrusted_certificate_or_another_name) {
  std::string cert, key, other, other_key;
  CHECK(tls_self_signed("localhost", cert, key));
  CHECK(tls_self_signed("localhost", other, other_key));
  std::string err;
  std::vector<uint8_t> plain;

  Pair p;  // trusts another certificate
  p.server = tls_server(cert, key, &err);
  TlsOptions o;
  o.ca_pem = other;
  p.client = tls_client("localhost", o);
  carry(p);
  CHECK(tls_step(*p.client, plain) == TlsState::failed);
  CHECK(tls_error(*p.client) == "SSL peer certificate or SSH remote key was not OK");

  Pair q;  // trusts it, under another name
  q.server = tls_server(cert, key, &err);
  o.ca_pem = cert;
  q.client = tls_client("example.org", o);
  carry(q);
  CHECK(tls_step(*q.client, plain) == TlsState::failed);
  CHECK(tls_error(*q.client) == "SSL peer certificate or SSH remote key was not OK");

  Pair r;  // checks nothing: a player's insecure setting
  r.server = tls_server(cert, key, &err);
  TlsOptions insecure;
  insecure.verify = false;
  r.client = tls_client("example.org", insecure);
  say(*r.client, "hi");
  carry(r);
  CHECK_MSG(tls_error(*r.client).empty(), tls_error(*r.client));
  CHECK(r.got_server == "hi");
}

TEST(tls_server_refuses_what_does_not_read) {
  std::string err;
  CHECK(tls_server("not a certificate", "not a key", &err) == nullptr);
  CHECK(err == "the server's certificate is not a PEM certificate");
  std::vector<uint8_t> plain;
  TlsOptions o;
  o.ca_pem = "not a certificate";
  auto c = tls_client("localhost", o);
  CHECK(tls_step(*c, plain) == TlsState::failed);
  CHECK(tls_error(*c) == "Problem with the SSL CA cert (path? access rights?)");
}
