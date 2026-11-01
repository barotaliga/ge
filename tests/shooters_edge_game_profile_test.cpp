// A classic game's game half (games/shooters_edge/profiles/game_profile.hpp): every game row has
// its content row; the classic game's multiplayer program files put the multiplayer form's own
// name first; the dedicated server's base folder is the content row's; and `find_program` (in
// client/listen.hpp) finds a mod's multiplayer program over the base's, passes over a program of
// the other form and says so, and finds the base's single-player program through the mod; and
// the item rows give the bots a map's items (variants first, ammunition doubled, any other
// armour), their call and heard words, and the sound table.
//
// ge:summary Tests the game profile rows and finding each mode's program through a mod folder
// ge:keywords test shooters-edge game-profile program qwprogs find_program multiplayer
// ge:skill shooter testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "progs/assembler.hpp"
#include "progs/layout.hpp"
#include "shooters_edge/bots/ears.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/dedicated.hpp"

namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;
namespace pg = ge::progs;
namespace fs = std::filesystem;

namespace {

void write(const fs::path& p, const std::vector<uint8_t>& b) {
  fs::create_directories(p.parent_path());
  std::ofstream f(p, std::ios::binary);
  f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

std::vector<uint8_t> program(const pg::Layout& form) { return pg::Assembler(form).bytes(); }

}  // namespace

TEST(every_game_row_has_its_content_row) {
  for (const pf::GameProfile& g : pf::kGameProfiles) {
    CHECK(ge::classicenhance::find_profile(g.id) != nullptr);
    CHECK(&pf::content_of(g) == ge::classicenhance::find_profile(g.id));
    CHECK(!g.multiplayer_programs.empty() && !g.single_player_programs.empty());
    CHECK(!g.fallback_map.empty());
  }
  CHECK(pf::find_game(pf::classic_game().id) == &pf::classic_game());
  CHECK(pf::find_game("no-such-game") == nullptr);
  CHECK(pf::classic_game().multiplayer_programs[0] == "qwprogs.dat");
  CHECK(std::string(shooters_edge::server::dedicated_game().base_dir) ==
        std::string(ge::classicenhance::classic_profile().base_dir));
}

TEST(each_mode_finds_its_program_through_the_mod_folder) {
  const fs::path root = fs::temp_directory_path() / "ge_se_game_profile_test";
  fs::remove_all(root);
  const fs::path base = root / std::string(ge::classicenhance::classic_profile().base_dir);
  write(base / "progs.dat", program(pg::kSinglePlayerLayout));  // the base game's campaign
  write(root / "mod" / "qwprogs.dat", program(pg::kMultiplayerLayout));
  const pf::GameProfile& g = pf::classic_game();
  cl::SearchPath sp;
  std::string error, found, why;
  CHECK_MSG(cl::open_game_dir(root.string(), "mod", sp, error), error);
  pg::Progs p;
  CHECK_MSG(cl::find_program(sp, g.multiplayer_programs, false, p, found, why), why);
  CHECK(found == "qwprogs.dat" && !cl::single_player(p));
  CHECK_MSG(cl::find_program(sp, g.single_player_programs, true, p, found, why), why);
  CHECK(found == "progs.dat" && cl::single_player(p));

  // The base game alone: its progs.dat is the single-player form, so multiplayer finds none.
  cl::SearchPath only_base;
  CHECK_MSG(
      cl::open_game_dir(root.string(), std::string(ge::classicenhance::classic_profile().base_dir),
                        only_base, error),
      error);
  CHECK(!cl::find_program(only_base, g.multiplayer_programs, false, p, found, why));
  CHECK_MSG(why.find("progs.dat is a single-player program") != std::string::npos, why);

  // A file that is no program is passed over with its reason.
  write(root / "mod" / "qwprogs.dat", {1, 2, 3});
  CHECK(!cl::find_program(sp, {g.multiplayer_programs.data(), 1}, false, p, found, why));
  CHECK_MSG(why.find("qwprogs.dat did not load") != std::string::npos, why);
  fs::remove_all(root);
}

TEST(the_item_rows_read_a_maps_items_and_their_words) {
  namespace bt = shooters_edge::bots;
  const auto item = [](const char* cn, int flags) {
    bt::ItemSpot it;
    CHECK(bt::item_of(cn, flags, it));
    return it;
  };
  const bt::ItemSpot mega = item("item_health", 3);  // the mega's bit wins over the small one's
  CHECK(mega.kind == pf::ItemKind::mega && mega.amount == 100 && mega.name == "mega health");
  CHECK(item("item_health", 1).amount == 15 && item("item_health", 0).amount == 25);
  CHECK(item("item_rockets", 1).amount == 10 && item("item_rockets", 0).amount == 5);
  const bt::ItemSpot ra = item("item_armorInv", 0);
  CHECK(ra.kind == pf::ItemKind::armor3 && ra.name == "item_armorInv");
  CHECK(std::string(bt::item_call(ra)) == "ra" && std::string(bt::hears_as(ra)) == "armor");
  const bt::ItemSpot odd = item("item_armorX", 0);  // any other armour: the bot's default kind
  CHECK(odd.kind == bt::ItemSpot{}.kind && odd.name == "item_armorX");
  const bt::ItemSpot rl = item("weapon_rocketlauncher", 0);
  CHECK(rl.weapon == 32 && rl.respawn_s == 30 && std::string(bt::item_call(rl)) == "rl");
  CHECK(std::string(bt::item_call(item("weapon_lightning", 0))) == "lg");
  CHECK(item("item_artifact_super_damage", 0).respawn_s == 60);
  bt::ItemSpot none;
  CHECK(!bt::item_of("info_player_start", 0, none));
  CHECK(pf::classic_game().tables.sounds.data() == bt::sound_table().data());
}
