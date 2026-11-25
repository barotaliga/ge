// TLS OVER THE ENGINE'S OWN BYTES, for a game that fetches from a web's secure address (an
// `https://` download, a GitHub repository's files): a client session that never touches a socket.
// The caller carries its wire bytes both ways (`tls_put_wire` what the socket brought,
// `tls_take_wire` what it must send) and reads and writes the plain bytes, so it runs over the
// non-blocking TCP stream (tcp_socket.hpp's raw bytes) a frame at a time and never blocks.
//
// - THE LIBRARY: Mbed TLS (cmake/Tls.cmake), TLS 1.2 and 1.3; this file's .cpp the only place it
//   is included.
// - WHO IS TRUSTED: the operating system's roots, read once: the file `SSL_CERT_FILE` names when it
//   is set; on Windows the system's ROOT store; elsewhere the first of the usual bundles
//   (`/etc/ssl/certs/ca-certificates.crt`, `/etc/pki/tls/certs/ca-bundle.crt`,
//   `/etc/ssl/ca-bundle.pem`, `/etc/pki/tls/cacert.pem`, `/etc/ssl/cert.pem`: Debian and Ubuntu,
//   Fedora, openSUSE, older Red Hat, macOS and Alpine). `TlsOptions::ca_pem` trusts those
//   certificates alone instead (a test's own); `verify` false checks nothing (a player's
//   `cl_http_insecure 1`). The server's name is checked against its certificate. A PIN
//   (`TlsOptions::pin`) trusts one certificate by its SHA-256 instead: a game server's own,
//   made by itself (`tls_self_signed`), its digest told to its clients beside its address.
// - A CERTIFICATE'S DIGEST (`tls_cert_sha256`): the SHA-256 of a PEM certificate's DER, the pin.
// - THE FAILURES, in words a caller can print (`tls_error`): "SSL peer certificate or SSH remote
//   key was not OK" (not trusted, expired, another name), "Problem with the SSL CA cert (path?
//   access rights?)" (no roots found), "SSL connect error" (the handshake failed otherwise).
// - A SERVER (`tls_server`) with an operator's certificate and key (a dedicated server's own web
//   files, net/file_server.hpp), or a test's or a lab's made on the spot (`tls_self_signed`): the
//   same calls carry its bytes.
// - SHA-256 (`sha256`): a file's digest, the one a web download is checked against.
//
// ge:summary TLS 1.2 and 1.3 over caller-carried bytes (Mbed TLS), the system roots, a test server
// ge:keywords tls https ssl mbedtls certificate trusted-roots ca-bundle ssl_cert_file handshake
// ge:keywords tls_client tls_server tls_self_signed insecure sha256 sha-256 digest
// ge:skill net
// ge:test tests/net_tls_stream_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ge::net {

struct TlsStream;  // opaque: the library's session

struct TlsOptions {
  bool verify = true;  // the server's chain and name checked
  std::string ca_pem;  // "": the system's roots; else these certificates alone
  // 32 bytes: the server's certificate must be the one with this SHA-256 (of its DER), whoever
  // signed it and whatever name it bears (a game server's own certificate, made by itself and
  // named in what it tells its clients); empty: the chain and the name checked.
  std::vector<uint8_t> pin;
};

enum class TlsState : uint8_t { handshaking, open, closed, failed };

// A client session for the server named `host` (its certificate's name checked against it).
std::shared_ptr<TlsStream> tls_client(const std::string& host, const TlsOptions& options);
// A server session with `cert_pem` (the chain, leaf first) and its `key_pem`; null and `error`
// when they do not read.
std::shared_ptr<TlsStream> tls_server(const std::string& cert_pem, const std::string& key_pem,
                                      std::string* error);
// A certificate for `host` signed by its own new key (P-256), valid 2020 to 2099, as PEM.
bool tls_self_signed(const std::string& host, std::string& cert_pem, std::string& key_pem);

// The bytes the socket brought, in.
void tls_put_wire(TlsStream& s, const uint8_t* data, size_t size);
// The bytes for the socket, appended to `out`.
void tls_take_wire(TlsStream& s, std::vector<uint8_t>& out);
// Plain bytes to send, queued until the handshake is done.
void tls_write(TlsStream& s, const uint8_t* data, size_t size);
// Moves the session on (the handshake, then what came): the plain bytes received appended to
// `plain`; the state after.
TlsState tls_step(TlsStream& s, std::vector<uint8_t>& plain);
// Why it failed ("" while it has not).
const std::string& tls_error(const TlsStream& s);

// The SHA-256 of the first certificate in `cert_pem` (its DER) into `out`; false when it does not
// read.
bool tls_cert_sha256(const std::string& cert_pem, uint8_t out[32]);
// The SHA-256 of `size` bytes at `data` into `out`.
void sha256(const uint8_t* data, size_t size, uint8_t out[32]);

}  // namespace ge::net
