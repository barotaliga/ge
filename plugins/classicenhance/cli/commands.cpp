// Classic enhance's commands in a game's player CLI (commands.hpp).
//
// ge:summary Classic enhance's content command: the install's packs, their release, the cache
// ge:keywords classicenhance gamecli content open_source pack-sha256 later-packs cache stages
// ge:skill classicenhance agent-interface
// ge:test tests/gamecli_cli_test.cpp
#include "classicenhance/cli/commands.hpp"

#include "classicenhance/cache.hpp"
#include "classicenhance/source.hpp"
#include "core/json_write.hpp"

namespace ge::classicenhance::cli {

namespace {

using core::json_arr;
using core::json_bool;
using core::json_obj;
using core::json_str;

int cmd_content(gamecli::Ctx& ctx) {
  if (!gamecli::need_places(ctx)) return 1;
  const std::string folder = ctx.words.empty() ? ctx.places.install : ctx.words[0];
  Source src;
  std::string error;
  if (!open_source(folder, &src, &error))
    return gamecli::refuse(ctx, 1, "no classic packs open in " + folder + ": " + error,
                           "point the game at its install (LOAD GAME in the menu, or --basedir)");
  const std::string release = src.set != nullptr     ? src.set->what
                              : src.known != nullptr ? src.known->what
                                                     : "";
  std::vector<std::string> later;
  std::string later_text;
  for (const LaterPack& p : src.later) {
    later.push_back(json_obj({{"name", json_str(p.name)},
                              {"sha256", json_str(p.sha256)},
                              {"release", json_str(p.known ? p.known->what : "")}}));
    later_text += "  " + p.name + "  " + (p.known ? p.known->what : "the player's own copy") + "\n";
  }
  const std::filesystem::path root = default_cache_root();
  const std::filesystem::path dir = root.empty() ? root : pack_dir(root, src.set_sha256);
  std::vector<std::string> stages;
  std::string stage_text;
  for (const StageRecord& r : read_records(dir)) {
    stages.push_back(json_str(r.stage));
    stage_text += (stage_text.empty() ? "" : ", ") + r.stage;
  }
  const bool full = src.set != nullptr || !src.later.empty();
  return gamecli::answer(
      ctx,
      json_obj({{"game", json_str(src.profile->what)},
                {"profile", json_str(src.profile->id)},
                {"route", json_str(route_name(src.route))},
                {"path", json_str(src.path)},
                {"pack_sha256", json_str(src.pack_sha256)},
                {"release", json_str(release)},
                {"verified", json_bool(!release.empty())},
                {"full_game", json_bool(full)},
                {"later_packs", json_arr(later)},
                {"note", json_str(src.set_note)},
                {"cache", json_str(dir.string())},
                {"stages", json_arr(stages)}}),
      std::string(src.profile->what) + " from " + src.path + "\n  " +
          (release.empty() ? "the player's own copy (unverified)" : release) + "\n" + later_text +
          "  cache " + dir.string() + ": " +
          (stage_text.empty() ? "nothing enhanced yet" : stage_text),
      full ? "the full game" : "the first pack alone",
      stage_text.empty() ? "the game enhances the packs the first time it opens them" : "");
}

}  // namespace

std::vector<gamecli::Command> commands() {
  return {{"content", "[<folder>]",
           "the classic packs the game plays: which game and release, the full game or not, and "
           "what the enhanced cache holds",
           "content --json", "classicenhance", cmd_content}};
}

}  // namespace ge::classicenhance::cli
