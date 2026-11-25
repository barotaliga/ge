// The window's own words over the game it plays (window_words.hpp).
//
// ge:summary The window's own words over the game it plays: map, sv, chat, v_cshift, vote, the rest
// ge:keywords map sv chat say v_cshift vote unknown-word window-words
// ge:skill shooter console
// ge:test tests/shooters_edge_game_commands_test.cpp
#include "shooters_edge/net/window_words.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "classic/content/downloads.hpp"
#include "classic/content/files.hpp"
#include "classic/content/map_versions.hpp"
#include "console/console.hpp"
#include "console/view.hpp"
#include "core/output.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/scores.hpp"
#include "shooters_edge/client/team_chat.hpp"
#include "shooters_edge/client/team_point.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/profiles/game_profile.hpp"

namespace shooters_edge::net {

void add_window_words(ge::console::Console& con, ge::console::ConsoleView& view,
                      client::Listen& listen, Remote& remote, std::string& next_map,
                      const std::function<bool(const std::string& line)>* first) {
  using CArgs = ge::console::Args;
  using Con = ge::console::Console;
  con.add_command("map", "map <name>: play the map (\"map arena\" is built in)",
                  [&](Con& c, const CArgs& a) {
                    if (a.size() < 2)
                      c.print(listen.map ? "map: " + listen.map->name : "map: none");
                    else
                      next_map = a[1];
                  });

  con.add_command("sv", "sv <line>: a line for the server's console", [&](Con& c, const CArgs& a) {
    std::string out;  // the administrator's words first (users, speedjump)
    if (client::listen_admin(listen, a.rest(0), out)) {
      for (size_t at = 0; at < out.size();) {
        const size_t end = std::min(out.find('\n', at), out.size());
        c.print(out.substr(at, end - at));
        at = end + 1;
      }
      return;
    }
    listen.opt.console.push_back(a.rest(0));
    if (listen.server) listen.rules->console(a.rest(0));
  });

  view.set_chat([&](ge::console::ChatTo to, std::string_view, std::string_view text) {
    using ge::console::ChatTo;
    std::string said(text);
    std::erase(said, '"');  // the line goes in quotes
    const std::string line = std::string(to == ChatTo::team     ? "say_team"
                                         : to == ChatTo::nearby ? "say_near"
                                                                : "say") +
                             " \"" + said + "\"";
    if (remote.active && remote.watch) {
      con.print("You can't chat while watching a broadcast.");
      return false;
    }
    if (remote.active) {
      remote_command(remote, line);
      return true;
    }
    if (client::listen_command(listen, line)) return true;
    con.print("Nobody is here to read that: chat works in a multiplayer game.");
    return false;
  });

  // A mod's own tint over the view in open air (it stuffs it; a player may type it too).
  con.add_command("v_cshift",
                  "v_cshift <red> <green> <blue> <strength>: a tint over the view in open air, "
                  "each 0 to 255; v_cshift 0 0 0 0 clears it",
                  [&](Con&, const CArgs& a) {
                    for (size_t k = 0; k < 4; k++)
                      listen.cshift[k] = std::clamp(
                          a.size() > k + 1 ? std::atof(a[k + 1].c_str()) : 0.0, 0.0, 255.0);
                  });

  // Any other line the window does not know goes to Quake II's rules (`use Railgun`, `drop`).
  con.set_unknown([&, first](const CArgs& a) {
    if (first != nullptr && *first && (*first)(a.raw)) return true;
    return remote_rules_word(remote, a.raw) || client::listen_rules_word(listen, a.raw) ||
           client::listen_campaign_word(listen, a.raw);
  });

  // The vote on a setting (engine/server/vote.hpp): to the server joined, else the window's own.
  for (const char* word : {"vote", "yes", "no"})
    con.add_command(word,
                    "vote <setting> <value>: ask everyone to change a setting for this map; "
                    "vote yes, vote no (or yes, no): answer; vote: what is open; vote rules",
                    [&](Con&, const CArgs& a) {
                      std::string line = a[0];
                      if (const std::string rest = a.rest(0); !rest.empty()) line += " " + rest;
                      if (remote.active)
                        remote_command(remote, line);
                      else
                        client::listen_lobby(listen, line);
                    });
}

void wire_team_chat(client::TeamChat& chat, client::Listen& listen, Remote& remote) {
  chat.send = [&listen, &remote](const std::string& line) {
    if (!remote.active) return client::listen_say(listen, line);
    remote_command(remote, line);
    return true;
  };
  chat.teammates = [&listen, &remote] {
    return !remote.active && client::teammate_in_game(client::scores_of(listen));
  };
  chat.scene = [&listen, &remote] {
    return remote.active ? client::point_scene(remote.demo.state) : client::point_scene(listen);
  };
}

void wire_remote_files(Remote& remote, ge::classic::content::SearchPath& files,
                       const std::string& basedir, const std::string& game,
                       const std::string& profile, bool& locs_fetched) {
  const auto content = [&profile]() -> const ge::classicenhance::Profile& {
    const profiles::GameProfile* g = profiles::find_game(profile);
    return profiles::content_of(g != nullptr ? *g : profiles::q1_game());
  };
  remote.read_file =
      [&files, &basedir, &game, content,
       other = std::make_shared<std::pair<std::string, ge::classic::content::SearchPath>>()](
          const std::string& gamedir, const std::string& path) {
        std::vector<uint8_t> store;
        const ge::classic::content::SearchPath* from = &files;
        if (!gamedir.empty() && gamedir != game && gamedir != content().base_dir) {
          if (other->first != gamedir) {
            std::string why;
            other->second = {};
            if (!ge::classic::content::open_game_dir(basedir, gamedir, other->second, why,
                                                     content()))
              return store;
            other->first = gamedir;
          }
          from = &other->second;
        }
        const std::span<const uint8_t> got = ge::classic::content::find_file(*from, path, store);
        return std::vector<uint8_t>(got.begin(), got.end());
      };
  remote.held = ge::classic::content::held_in_folder([&files] { return files.folder; });
  // A map's other versions, kept hidden in the window's game folder (map_versions.hpp).
  namespace cc = ge::classic::content;
  remote.map_versions = [&files](const std::string& map) {
    return cc::stored_versions(files.folder, map);
  };
  remote.keep_version = [&files](const std::string& map, uint32_t sum,
                                 const std::vector<uint8_t>& bytes) {
    std::string why;
    const std::string path = cc::keep_version(files.folder, map, sum, bytes, why);
    if (path.empty()) ge::core::output::status("shooters_edge: " + why);
    return path;
  };
  remote.use_map = [&files](const std::string& map, const std::string& version) {
    cc::choose_version(files, map, version);
  };
  remote.remembered = [&files](const std::string& server, const std::string& map) {
    return cc::remembered_key(files.folder, server, map);
  };
  remote.keep_choice = [&files](const std::string& server, const std::string& map, uint32_t sum) {
    cc::keep_choice(files.folder, server, map, sum);
  };
  remote.keep_file = [&files, &basedir, &game, content, &locs_fetched](
                         const std::string& gamedir, const std::string& path,
                         const std::vector<uint8_t>& bytes, const ge::vfs::ArchiveOrigin* origin) {
    if (path.starts_with("locs/")) locs_fetched = true;
    const std::string base(content().base_dir);
    const bool ours = gamedir.empty() || gamedir == game || gamedir == base;
    ge::classic::content::KeptDownload kept;
    std::string why;
    if (!ge::classic::content::keep_download(files, basedir, base, gamedir, ours, path, bytes,
                                             origin, &kept, why))
      ge::core::output::status("shooters_edge: " + why);
  };
}

}  // namespace shooters_edge::net
