// Tests: the game's own sounds under the classic names (client/own_sounds.hpp): the asset map's
// sound rows read (comments, model and set rows skipped, a broken row refused with its line), the
// tree's map and recordings mounted, and the built-in arena with three bots heard through them:
// the fight's sounds played, no name on its sound list missing a recording.
//
// ge:summary Tests: the game's own WAVs under the classic names, and the arena's bot fight heard
// ge:keywords test own-sounds assetmap sound-map arena bots fight heard silent
// ge:skill audio testing
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "audio/synth.hpp"
#include "check.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/listen_sound.hpp"
#include "shooters_edge/client/own_sounds.hpp"
#include "shooters_edge/net/classic_game.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace sh = ge::shooter;

const std::string kContent = std::string(GE_SOURCE_DIR) + "/games/shooters_edge/content";

TEST(the_asset_map_s_sound_rows_are_read_and_the_rest_skipped) {
  std::vector<std::pair<std::string, std::string>> rows;
  std::string error;
  CHECK_MSG(cl::read_sound_map("# a comment\n"
                               "sound weapons/ax1.wav weapon/melee_swing\n"
                               "model progs/player.mdl player # pin\n"
                               "set func_door sounds=1 noise1 door/stone_open\n"
                               "sound misc/null.wav silence   # trailing\n",
                               rows, error),
            error);
  CHECK(rows.size() == 2);
  if (rows.size() != 2) return;
  CHECK(rows[0].first == "weapons/ax1.wav" && rows[0].second == "weapon/melee_swing");
  CHECK(rows[1].first == "misc/null.wav" && rows[1].second == "silence");
  CHECK(!cl::read_sound_map("sound\nsound only/one.wav\n", rows, error));
  CHECK_MSG(error.find("line 1") != std::string::npos, error);
}

TEST(the_tree_s_recordings_mount_under_the_classic_names) {
  cl::SearchPath files;
  cl::OwnSounds own;
  std::string error;
  CHECK_MSG(cl::mount_own_sounds(files, kContent, own, error), error);
  CHECK(own.mapped > 100);
  CHECK_MSG(own.mounted > own.mapped / 2,
            std::to_string(own.mounted) + " of " + std::to_string(own.mapped) + " mounted");
  std::vector<uint8_t> store;
  CHECK(!cl::find_file(files, "sound/weapons/rocket1i.wav", store).empty());
  CHECK(cl::find_content_dir(kContent) == "games/shooters_edge/content" ||
        cl::find_content_dir(kContent) == kContent);
  cl::SearchPath none;
  CHECK(!cl::mount_own_sounds(none, "/no/such/folder", own, error));
  CHECK_MSG(error.find("classic.assetmap") != std::string::npos, error);
}

TEST(the_built_in_arena_s_bot_fight_is_heard_through_the_game_s_own_recordings) {
  ge::progs::Progs progs;
  std::string error;
  CHECK_MSG(cl::baseline_progs(progs, error), error);
  cl::SearchPath files;
  cl::OwnSounds own;
  CHECK_MSG(cl::mount_own_sounds(files, kContent, own, error), error);
  cl::Listen l;
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.console = {"deathmatch 1"};
  opt.lobby = true;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "arena", error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return;
  CHECK_MSG(cl::listen_start(l, progs, std::move(map), opt, error), error);
  for (int i = 0; i < 3; i++) CHECK(cl::listen_lobby(l, "addbot"));
  CHECK(cl::listen_lobby(l, "warmup"));  // the player on the map, among the bots
  ge::audio::Synth synth;
  cl::ListenHearing hearing(synth);
  std::vector<float> out(size_t{2} * 640);
  const sh::MoveCmd idle;
  for (int f = 0; f < 72 * 20; f++) {  // 20 s of the warmup's free fight
    cl::listen_frame(l, idle);
    hearing.frame(l, files, 1.0f / 72);
    synth.render(out.data(), 640);
  }
  const cl::DemoSoundStats* st = hearing.stats();
  CHECK(st != nullptr);
  if (st == nullptr) return;
  CHECK_MSG(st->started > 20, std::to_string(st->started) + " sounds played in 20 s");
  CHECK_MSG(st->missing == 0, std::to_string(st->missing) + " sound names had no recording");
  cl::listen_stop(l);
}

}  // namespace
