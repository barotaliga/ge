// THE HUB'S LINKS (docs/hub.md § Open in the game): the Join, Spectate, Watch and Play buttons on
// a hub's pages are links in the game's own scheme, its HubSpec's id, which the player's machine
// hands to the game's CLI (`<game>_cli link open <link>`, engine/gamecli) once the CLI has
// registered the scheme (`<game>_cli link register`):
//
//   <id>://join/<host[:port]>             start the game on that server
//   <id>://spectate/<host[:port]>         ... as a spectator
//   <id>://tv/<host:port>/<stream>        watch a broadcast from a TV node
//   <id>://demo/<sha256>?hub=<host:port>  fetch the demo from that hub and play it
//
// A link comes from a web page, so it is read strictly: the host is letters, digits, dots, dashes
// and one port; the hash is 64 lower-case hex characters; anything else is refused, never passed
// on to a command line. A fetched demo is kept only when its bytes hash to its name.
//
// REGISTERING THE SCHEME is per system, and `link_plan` writes it out as files and commands, pure,
// so a test reads it and `--dry-run` shows it:
// - Linux: a desktop entry for `x-scheme-handler/<id>` under ~/.local/share/applications, made the
//   default by `xdg-mime`.
// - macOS: a small handler app under ~/Applications compiled by `osacompile` from a two-line
//   script that hands the link to the CLI (macOS gives a link to an app as an event, never as an
//   argument), its Info.plist given the scheme by `plutil`, registered by `lsregister`.
// - Windows: the scheme's keys under HKEY_CURRENT_USER\Software\Classes by `reg add`.
//
// ge:summary The hub's links (join, spectate, tv, demo): read, written, a demo fetched, the scheme
// ge:keywords hub link url scheme join spectate watch tv demo play fetch register xdg-mime
// ge:keywords desktop-entry osacompile lsregister registry url-protocol open-in-game
// ge:skill net
// ge:test tests/hub_link_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::hub {

enum class LinkKind : uint8_t { join, spectate, tv, demo };

struct HubLink {
  LinkKind kind = LinkKind::join;
  std::string address;  // join, spectate: the server; tv: the TV node
  uint32_t stream = 0;  // tv: the node's stream
  std::string sha;      // demo: its SHA-256
  std::string hub;      // demo: the hub that serves it, host[:port]
};

// `url` read as a link in `scheme`; false with `error` (what is wrong with it).
bool parse_link(std::string_view url, std::string_view scheme, HubLink& out, std::string& error);
// The link written in `scheme`.
std::string link_url(std::string_view scheme, const HubLink& link);
// Whether `text` is a host the links accept: name or IPv4, an optional port 1..65535.
bool link_host(std::string_view text);

// The demo `sha` fetched from the hub at `hub` (host[:port], 80 when none) over HTTP into `out`;
// false with `error` when the hub cannot be reached, answers anything but the file, takes longer
// than `timeout_ms`, or sends bytes that do not hash to `sha`.
bool fetch_demo(std::string_view hub, std::string_view sha, std::vector<uint8_t>& out,
                std::string& error, int timeout_ms = 15000);

enum class LinkOs : uint8_t { linux_desktop, macos, windows };
LinkOs this_os();

struct LinkFile {
  std::string path, text;
};

// What registering the scheme does: the files written, then each command run in order.
struct LinkPlan {
  std::vector<LinkFile> files;
  std::vector<std::vector<std::string>> commands;
  std::string handler;  // where the handler lives (the desktop entry, the app, the key)
};

// The plan for `scheme` (the game's id), `title` (its name for people), handing a link to
// `cli_path` as `<cli> link open <link>`; `home` is the user's home folder (unused on Windows).
LinkPlan link_plan(std::string_view scheme, std::string_view title, std::string_view cli_path,
                   LinkOs os, std::string_view home);

}  // namespace ge::hub
