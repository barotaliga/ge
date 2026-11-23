// The adapter's choice per game and the console's commands on it (game_adapter.hpp).
//
// ge:summary Picks a game row's adapter, says which forms run, adds mods, maps, demos, game cfgs
// ge:keywords game-adapter adapter_of runs_form add_adapter_commands mods maps demos console
// ge:keywords read_configs_from exec config-file com_maplist path dir fdir whereis
// ge:skill shooter console
// ge:test tests/shooters_edge_game_adapter_test.cpp
#include "shooters_edge/client/game_adapter.hpp"

#include <algorithm>

#include "vfs/search_commands.hpp"

namespace shooters_edge::client {

const GameAdapter& adapter_of(const profiles::GameProfile& game) {
  if (game.id == q2_adapter().game->id) return q2_adapter();
  return qw_adapter();
}

bool runs_form(profiles::ProgramForm form) { return form == profiles::ProgramForm::qc; }

std::vector<std::string> demo_completions(const GameAdapter& adapter, const SearchPath& files) {
  std::vector<std::string> out = adapter.list_demos(adapter, files);
  const std::string_view folder = adapter.game->demo_folder;
  if (!folder.empty())
    for (std::string& d : out)
      if (d.size() > folder.size() && d.compare(0, folder.size(), folder) == 0)
        d.erase(0, folder.size());
  std::ranges::sort(out);
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

void add_adapter_commands(ge::console::Console& con, const GameAdapter& adapter,
                          const SearchPath& files, const std::string& basedir) {
  using Args = ge::console::Args;
  using Con = ge::console::Console;
  con.add_command("mods", "mods: the mod folders beside the base game (play one: -game <it>)",
                  // A callback held in a std::function: what it throws (a string out of memory)
                  // reaches its caller, as any call's does.
                  // NOLINTNEXTLINE(bugprone-exception-escape)
                  [&adapter, basedir](Con& c, const Args&) {
                    const std::vector<ModFolder> mods = adapter.list_mods(adapter, basedir);
                    if (mods.empty()) c.print("mods: no mod folders in " + basedir);
                    for (const ModFolder& m : mods) {
                      std::string line = m.folder;
                      line += ": " + std::to_string(m.archives) + " archives";
                      for (const std::string& p : m.programs) line += ", " + p;
                      c.print(line);
                    }
                  });
  con.add_command("maps", "maps: the maps the game's files hold, with their titles",
                  [&adapter, &files](Con& c, const Args&) {
                    for (const std::string& name : adapter.list_maps(adapter, files)) {
                      MapInfo info;
                      std::string line = name, why;
                      if (!adapter.map_info(adapter, files, name, info, why))
                        line += ": " + why;
                      else if (!info.title.empty())
                        line += ": " + info.title;
                      c.print(line);
                    }
                  });
  // Tab after `playdemo`, `timedemo` (and Quake II's `demo`) completes the recordings' names.
  const auto names = [&adapter, &files] { return demo_completions(adapter, files); };
  for (const char* word : {"playdemo", "timedemo", "demo"}) con.set_arg_values(word, names);
  ge::vfs::add_search_commands(con, files.vfs);  // path, dir, fdir, whereis
  // $com_maplist: every map the files hold, by name, a space between.
  con.add_macro("com_maplist", [&adapter, &files] {
    std::string all;
    for (const std::string& name : adapter.list_maps(adapter, files))
      all += (all.empty() ? "" : " ") + name;
    return all;
  });
  con.add_command("demos", "demos: the recordings the game's files hold (playdemo <it>)",
                  [&adapter, &files](Con& c, const Args&) {
                    const std::vector<std::string> demos = adapter.list_demos(adapter, files);
                    if (demos.empty()) c.print("demos: none in the game's files");
                    for (const std::string& d : demos) c.print(d);
                  });
}

void read_configs_from(ge::console::Console& con, const GameAdapter& adapter,
                       const SearchPath& files) {
  con.set_fallback_reader([&adapter, &files](std::string_view name, std::string& text) {
    return adapter.find_config(adapter, files, name, text);
  });
}

}  // namespace shooters_edge::client
