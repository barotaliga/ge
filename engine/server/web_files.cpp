// A dedicated server's files over HTTPS (web_files.hpp).
//
// ge:summary The web files: the settings read, the certificate read or made, the address told
// ge:keywords web-files sv_http_port certificate-pin self-signed
// ge:skill net
// ge:test tests/server_web_files_test.cpp
#include "server/web_files.hpp"

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <utility>

#include "net/tls_stream.hpp"

namespace ge::server {

namespace {

bool whole(const ServerConfig& c, const char* name, int64_t low, int64_t high, int64_t& out,
           std::string& error) {
  const auto it = c.rules.find(name);
  if (it == c.rules.end() || it->second.empty()) return true;
  const std::string& v = it->second;
  int64_t n = 0;
  bool ok = v.size() <= 12;
  for (const char ch : v) {
    if (ch < '0' || ch > '9') ok = false;
    if (ok) n = n * 10 + (ch - '0');
  }
  if (!ok || n < low || n > high) {
    error = std::string(name) + " \"" + v + "\" is not a whole number in " + std::to_string(low) +
            ".." + std::to_string(high) + ". Set it to one (--explain=" + name +
            " shows where it was set)";
    return false;
  }
  out = n;
  return true;
}

std::string text_rule(const ServerConfig& c, const char* name) {
  const auto it = c.rules.find(name);
  return it == c.rules.end() ? std::string() : it->second;
}

bool read_text(const std::string& path, std::string& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return !out.empty();
}

}  // namespace

bool web_files_config(const ServerConfig& c, WebFilesConfig& out, std::string& error) {
  out = {};
  int64_t port = 0, plain = 0;
  if (!whole(c, "sv_http_port", 0, 65535, port, error) ||
      !whole(c, "sv_http_rate", 0, 100000000, out.rate_kb, error) ||
      !whole(c, "sv_http_rate_player", 0, 100000000, out.rate_player_kb, error) ||
      !whole(c, "sv_http_plain", 0, 1, plain, error))
    return false;
  out.port = static_cast<int>(port);
  out.plain = plain != 0;
  out.cert_file = text_rule(c, "sv_http_cert");
  out.key_file = text_rule(c, "sv_http_key");
  out.url = text_rule(c, "sv_http_url");
  if (out.cert_file.empty() != out.key_file.empty()) {
    error =
        "sv_http_cert and sv_http_key go together: set both to the certificate's and "
        "its key's PEM files, or neither for a certificate the server makes itself";
    return false;
  }
  if (!out.url.empty() && out.url.rfind("https://", 0) != 0 && out.url.rfind("http://", 0) != 0) {
    error = "sv_http_url \"" + out.url +
            "\" is not a web address: set it to https://<host>[:port]/";
    return false;
  }
  if (!out.url.empty() && out.url.back() != '/') out.url += '/';
  // sv_publicaddr's host: "host:port" with the port dropped ("[v6]:port" keeps its brackets).
  std::string host = c.public_addr;
  const size_t colon = host.rfind(':');
  if (colon != std::string::npos && host.find(':') == colon) host.resize(colon);
  if (colon != std::string::npos && !host.empty() && host.front() == '[')
    host.resize(host.rfind(']') + 1);
  out.public_host = host;
  return true;
}

bool web_files_open(WebFiles& w, const WebFilesConfig& cfg, const net::Address& ip,
                    net::FileLookup lookup, std::string& error) {
  web_files_close(w);
  w.cfg = cfg;
  if (cfg.port <= 0) return true;
  net::FileServeOptions o;
  o.rate = cfg.rate_kb * 1000;
  o.rate_person = cfg.rate_player_kb * 1000;
  o.name = "ge-server";
  if (!cfg.plain) {
    if (!cfg.cert_file.empty()) {
      if (!read_text(cfg.cert_file, o.cert_pem) || !read_text(cfg.key_file, o.key_pem)) {
        error = "the web files' certificate or key does not read (sv_http_cert \"" + cfg.cert_file +
                "\", sv_http_key \"" + cfg.key_file + "\"): point both at readable PEM files";
        return false;
      }
    } else {
      if (!net::tls_self_signed("ge-server", o.cert_pem, o.key_pem)) {
        error =
            "the web files' own certificate could not be made: set sv_http_cert and "
            "sv_http_key, or sv_http_plain 1 behind a TLS proxy";
        return false;
      }
      uint8_t sum[32];
      net::tls_cert_sha256(o.cert_pem, sum);
      char hex[65];
      for (int i = 0; i < 32; ++i)
        std::snprintf(hex + static_cast<ptrdiff_t>(i) * 2, 3, "%02x", sum[i]);
      w.pin = hex;
    }
  }
  net::Address bind = ip;
  bind.port = static_cast<uint16_t>(cfg.port);
  if (!net::file_server_open(w.server, bind, o, std::move(lookup), error)) return false;
  if (!cfg.url.empty()) {
    w.url = cfg.url;
  } else {
    w.url = std::string(cfg.plain ? "http://" : "https://") +
            (cfg.public_host.empty() ? std::string("{server}") : cfg.public_host) + ":" +
            std::to_string(w.server.listener.local.port) + "/";
  }
  return true;
}

void web_files_frame(WebFiles& w, double now_s) { net::file_server_step(w.server, now_s); }

void web_files_close(WebFiles& w) {
  net::file_server_close(w.server);
  w.url.clear();
  w.pin.clear();
}

bool web_files_told(const std::string& url) {
  return !url.empty() && url.find("{server}") == std::string::npos;
}

std::string web_files_at(const std::string& url, const std::string& host) {
  std::string out = url;
  const size_t at = out.find("{server}");
  if (at != std::string::npos) out.replace(at, 8, host);
  return out;
}

}  // namespace ge::server
