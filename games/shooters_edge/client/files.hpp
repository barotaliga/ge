// THE GAME'S FILES, as the client names them: the search path every shooter game on the classic
// files shares (plugins/shooter/content/files.hpp: the base folder and a mod folder over it, packs,
// zips and loose files in the game's own order, saves, the palette, a map's light, sky boxes, the
// release's words by key), its names brought into the client's namespace so the client's code
// says `SearchPath` and `find_file` as it did. New code outside the client includes the plugin's
// header and names `ge::shooter::content`.
//
// ge:summary The client's names for the shared search path in plugins/shooter/content
// ge:keywords search-path files content-layer using-declarations game-folder
// ge:skill shooter vfs
// ge:test tests/shooter_content_files_test.cpp
#pragma once

#include "shooter/content/files.hpp"
#include "shooters_edge/client/load_order.hpp"

namespace shooters_edge::client {

using ge::shooter::content::add_archive;
using ge::shooter::content::archive_paths;
using ge::shooter::content::classic_save_name;
using ge::shooter::content::client_folder;
using ge::shooter::content::content_packs;
using ge::shooter::content::find_file;
using ge::shooter::content::game_palette;
using ge::shooter::content::localized_text;
using ge::shooter::content::map_lit;
using ge::shooter::content::map_names;
using ge::shooter::content::on_disk_any_case;
using ge::shooter::content::open_game_dir;
using ge::shooter::content::read_save;
using ge::shooter::content::save_label;
using ge::shooter::content::SearchPath;
using ge::shooter::content::sky_box_files;
using ge::shooter::content::SkyBoxFiles;
using ge::shooter::content::write_save;

}  // namespace shooters_edge::client
