// The adapter's choice per game and the console's commands on it (game_adapter.hpp).
//
// ge:summary Picks a game row's adapter, says which forms run, adds mods, maps, demos, game cfgs
// ge:keywords game-adapter adapter_of runs_form add_adapter_commands mods maps demos console
// ge:keywords read_configs_from exec config-file
// ge:skill shooter console
// ge:test tests/shooters_edge_game_adapter_test.cpp
#include "shooters_edge/client/game_adapter.hpp"

namespace shooters_edge::client {

const GameAdapter& adapter_of(const profiles::GameProfile&) {
  return classic_adapter();  // one game today; a second row's adapter is chosen here by its id
}

bool runs_form(profiles::ProgramForm form) { return form == profiles::ProgramForm::qc; }

void add_adapter_commands(ge::console::Console& con, const GameAdapter& adapter,
                          const SearchPath& files, const std::string& basedir) {
  using Args = ge::console::Args;
  using Con = ge::console::Console;
  con.add_command("mods", "mods: the mod folders beside the base game (play one: -game <it>)",
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
