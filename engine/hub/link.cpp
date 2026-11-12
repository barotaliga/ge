// The hub's links, defined: reading and writing them, a demo fetched over HTTP, the scheme's
// registration per system.
//
// ge:summary The hub's links defined: parse, write, fetch a demo by hash, the registration plans
// ge:keywords hub link parse fetch http-get sha256 register desktop-entry applescript registry
// ge:skill net
// ge:test tests/hub_link_test.cpp
#include "hub/link.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>

#include "hub/archive.hpp"
#include "mod/sha256.hpp"
#include "net/address.hpp"
#include "net/tcp_socket.hpp"
#include "net/udp_socket.hpp"

namespace ge::hub {

namespace gn = ge::net;

namespace {

bool host_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
         c == '-';
}

bool sha_text(std::string_view s) {
  if (s.size() != 64) return false;
  for (char c : s) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

bool digits(std::string_view s, uint64_t max, uint64_t& out) {
  if (s.empty() || s.size() > 10) return false;
  out = 0;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
    out = out * 10 + static_cast<uint64_t>(c - '0');
  }
  return out <= max;
}

}  // namespace

bool link_host(std::string_view text) {
  const size_t colon = text.find(':');
  const std::string_view host = text.substr(0, colon);
  if (host.empty() || host.size() > 253 || host.front() == '-' || host.front() == '.') return false;
  for (char c : host) {
    if (!host_char(c)) return false;
  }
  if (colon == std::string_view::npos) return true;
  uint64_t port = 0;
  return digits(text.substr(colon + 1), 65535, port) && port > 0;
}

bool parse_link(std::string_view url, std::string_view scheme, HubLink& out, std::string& error) {
  out = HubLink{};
  const std::string head = std::string(scheme) + "://";
  if (url.size() < head.size() || url.substr(0, head.size()) != head) {
    error = "'" + std::string(url.substr(0, 64)) + "' is not a " + std::string(scheme) + ":// link";
    return false;
  }
  std::string_view rest = url.substr(head.size());
  std::string_view query;
  if (const size_t q = rest.find('?'); q != std::string_view::npos) {
    query = rest.substr(q + 1);
    rest = rest.substr(0, q);
  }
  while (!rest.empty() && rest.back() == '/') rest.remove_suffix(1);  // a browser's trailing slash
  const size_t slash = rest.find('/');
  const std::string_view verb = rest.substr(0, slash);
  const std::string_view what = slash == std::string_view::npos ? "" : rest.substr(slash + 1);
  if (verb == "join" || verb == "spectate") {
    if (!link_host(what)) {
      error = "the link's server '" + std::string(what.substr(0, 64)) + "' is not host[:port]";
      return false;
    }
    out.kind = verb == "join" ? LinkKind::join : LinkKind::spectate;
    out.address = std::string(what);
    return true;
  }
  if (verb == "tv") {
    const size_t s = what.rfind('/');
    uint64_t stream = 0;
    if (s == std::string_view::npos || !link_host(what.substr(0, s)) ||
        !digits(what.substr(s + 1), 0xffffffffu, stream)) {
      error = "a tv link is " + std::string(scheme) + "://tv/<host:port>/<stream>";
      return false;
    }
    out.kind = LinkKind::tv;
    out.address = std::string(what.substr(0, s));
    out.stream = static_cast<uint32_t>(stream);
    return true;
  }
  if (verb == "demo") {
    if (!sha_text(what)) {
      error = "a demo link names the demo by its SHA-256: 64 lower-case hex characters";
      return false;
    }
    out.kind = LinkKind::demo;
    out.sha = std::string(what);
    const std::string_view key = "hub=";
    for (size_t at = 0; at <= query.size();) {
      const size_t amp = std::min(query.find('&', at), query.size());
      const std::string_view part = query.substr(at, amp - at);
      if (part.substr(0, key.size()) == key) out.hub = std::string(part.substr(key.size()));
      at = amp + 1;
    }
    if (!link_host(out.hub)) {
      error = "a demo link names the hub it comes from: ?hub=<host[:port]>";
      return false;
    }
    return true;
  }
  error = "the link asks for '" + std::string(verb.substr(0, 32)) +
          "'; a link is join, spectate, tv or demo";
  return false;
}

std::string link_url(std::string_view scheme, const HubLink& link) {
  std::string u = std::string(scheme) + "://";
  switch (link.kind) {
    case LinkKind::join:
      return u + "join/" + link.address;
    case LinkKind::spectate:
      return u + "spectate/" + link.address;
    case LinkKind::tv:
      return u + "tv/" + link.address + "/" + std::to_string(link.stream);
    case LinkKind::demo:
      return u + "demo/" + link.sha + "?hub=" + link.hub;
  }
  return u;
}

bool fetch_demo(std::string_view hub, std::string_view sha, std::vector<uint8_t>& out,
                std::string& error, int timeout_ms) {
  out.clear();
  if (!link_host(hub) || !sha_text(sha)) {
    error = "the hub must be host[:port] and the demo a SHA-256";
    return false;
  }
  gn::Address at;
  std::string why;
  if (!gn::resolve_ipv4(hub, 80, at, &why)) {
    error = "the hub " + std::string(hub) + " cannot be found: " + why;
    return false;
  }
  gn::TcpStream s;
  if (!gn::tcp_connect(s, at, &why)) {
    error = "the hub " + std::string(hub) + " cannot be reached: " + why;
    return false;
  }
  std::string req = "GET /";
  req += archive_path(sha);
  req += " HTTP/1.1\r\nHost: ";
  req += hub;
  req += "\r\nConnection: close\r\n\r\n";
  gn::tcp_send_bytes(s, reinterpret_cast<const uint8_t*>(req.data()), req.size());
  std::vector<uint8_t> got;
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  size_t body_at = 0;
  int64_t length = -1;
  int code = 0;
  for (;;) {
    gn::tcp_pump(s);
    gn::tcp_take_bytes(s, got);
    if (body_at == 0) {
      const std::string_view text(reinterpret_cast<const char*>(got.data()), got.size());
      const size_t end = text.find("\r\n\r\n");
      if (end != std::string_view::npos) {
        body_at = end + 4;
        if (text.size() > 12 && text.substr(0, 5) == "HTTP/") {
          code = std::atoi(std::string(text.substr(9, 3)).c_str());
        }
        const std::string_view head = text.substr(0, end);
        for (size_t line = 0; line < head.size();) {
          const size_t eol = std::min(head.find("\r\n", line), head.size());
          const size_t colon = std::min(head.find(':', line), eol);
          std::string name(head.substr(line, colon - line));
          for (char& c : name) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
          if (name == "content-length" && colon < eol) {
            length = std::atoll(std::string(head.substr(colon + 1, eol - colon - 1)).c_str());
          }
          line = eol + 2;
        }
      }
    }
    if (body_at && length >= 0 && got.size() >= body_at + static_cast<size_t>(length)) break;
    if (s.closed) break;
    if (std::chrono::steady_clock::now() > until) {
      gn::tcp_close(s);
      error = "the hub " + std::string(hub) + " did not send the demo within " +
              std::to_string(timeout_ms / 1000) + " s";
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  gn::tcp_close(s);
  if (body_at == 0 || code != 200) {
    error = code == 404 ? "the hub " + std::string(hub) + " does not have that demo"
                        : "the hub " + std::string(hub) + " answered " + std::to_string(code) +
                              " for the demo";
    return false;
  }
  out.assign(got.begin() + static_cast<std::ptrdiff_t>(body_at), got.end());
  if (length >= 0 && out.size() > static_cast<size_t>(length))
    out.resize(static_cast<size_t>(length));
  if (mod::sha256_hex(out.data(), out.size()) != sha) {
    out.clear();
    error = "the file the hub sent is not that demo (its hash differs)";
    return false;
  }
  return true;
}

LinkOs this_os() {
#if defined(_WIN32)
  return LinkOs::windows;
#elif defined(__APPLE__)
  return LinkOs::macos;
#else
  return LinkOs::linux_desktop;
#endif
}

LinkPlan link_plan(std::string_view scheme, std::string_view title, std::string_view cli_path,
                   LinkOs os, std::string_view home) {
  LinkPlan p;
  const std::string s(scheme), t(title), cli(cli_path), h(home);
  switch (os) {
    case LinkOs::linux_desktop: {
      const std::string apps = h + "/.local/share/applications";
      const std::string entry = s + "-link.desktop";
      p.handler = apps + "/" + entry;
      p.files.push_back(
          {p.handler, "[Desktop Entry]\nType=Application\nName=" + t + " links\nExec=\"" + cli +
                          "\" link open %u\n"
                          "NoDisplay=true\nTerminal=false\nMimeType=x-scheme-handler/" +
                          s + ";\n"});
      p.commands.push_back({"xdg-mime", "default", entry, "x-scheme-handler/" + s});
      break;
    }
    case LinkOs::macos: {
      const std::string app = h + "/Applications/" + t + " Links.app";
      const std::string script = h + "/Library/Application Support/" + s + "/link.applescript";
      p.handler = app;
      // The CLI's path and the link each quoted for the shell by AppleScript itself.
      std::string quoted_cli;
      for (char c : cli)
        quoted_cli += (c == '"' || c == '\\') ? std::string("\\") + c : std::string(1, c);
      p.files.push_back({script, "on open location this_link\n  do shell script quoted form of \"" +
                                     quoted_cli +
                                     "\" & \" link open \" & quoted form of this_link & "
                                     "\" > /dev/null 2>&1 &\"\nend open location\n"});
      p.commands.push_back({"osacompile", "-o", app, script});
      p.commands.push_back(
          {"plutil", "-replace", "CFBundleURLTypes", "-json",
           "[{\"CFBundleURLName\":\"" + s + "\",\"CFBundleURLSchemes\":[\"" + s + "\"]}]",
           app + "/Contents/Info.plist"});
      p.commands.push_back(
          {"/System/Library/Frameworks/CoreServices.framework/Frameworks/"
           "LaunchServices.framework/Support/lsregister",
           "-f", app});
      break;
    }
    case LinkOs::windows: {
      const std::string key = "HKCU\\Software\\Classes\\" + s;
      p.handler = key;
      p.commands.push_back({"reg", "add", key, "/ve", "/d", "URL:" + t + " link", "/f"});
      p.commands.push_back({"reg", "add", key, "/v", "URL Protocol", "/d", "", "/f"});
      p.commands.push_back({"reg", "add", key + "\\shell\\open\\command", "/ve", "/d",
                            "\"" + cli + "\" link open \"%1\"", "/f"});
      break;
    }
  }
  return p;
}

}  // namespace ge::hub
