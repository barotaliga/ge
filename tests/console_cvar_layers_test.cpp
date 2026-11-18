// Cvar layers: one name, one meaning. A name a later layer meets is the same setting (`same`),
// the game's own (`own`) or a clash; a layer dropped gives back what it replaced.
//
// ge:summary Tests cvar layers: same and own overlaps, clashes recorded, a dropped layer restored
// ge:keywords test cvar-layer layer drop_layer overlap clash same own retired per-game-cvar
// ge:skill console testing
#include <filesystem>

#include "check.hpp"
#include "console/catalog.hpp"
#include "console/console.hpp"
#include "console/cvar.hpp"

using namespace ge::console;

// A check the rest of the test stands on: fail and stop.
#define REQUIRE(cond) \
  do {                \
    if (!(cond)) {    \
      CHECK(cond);    \
      return;         \
    }                 \
  } while (0)

TEST(a_name_met_without_a_declaration_is_a_clash_not_a_merge) {
  Cvars cv;
  double engine = 0, game = 7;
  {
    CvarLayer l(cv, "engine");
    cv.bind({.name = "r_shadows", .fallback = "1", .help = "Shadows of the lights."}, &engine);
  }
  CvarLayer l(cv, "game:first");
  Cvar& c =
      cv.bind({.name = "r_shadows", .fallback = "0", .help = "A blob under each model."}, &game);
  CHECK(c.layer == "engine" && c.value == "1" && c.help == "Shadows of the lights.");
  CHECK(game == 7);  // never written: nothing merged
  REQUIRE(cv.clashes().size() == 1);
  const CvarClash& k = cv.clashes()[0];
  CHECK(k.name == "r_shadows" && k.earlier_layer == "engine" && k.layer == "game:first");
  const std::string line = clash_text(k);
  CHECK(line.find("r_shadows") != std::string::npos);
  CHECK(line.find("next:") != std::string::npos);
}

TEST(the_same_layer_twice_is_the_first_as_before) {
  Cvars cv;
  double a = 0, b = 0;
  cv.bind({.name = "x", .fallback = "1"}, &a);
  Cvar& c = cv.bind({.name = "x", .fallback = "2"}, &b);
  CHECK(c.value == "1" && a == 1 && b == 0);
  CHECK(cv.clashes().empty());
}

TEST(same_takes_the_games_default_writes_both_fields_and_keeps_a_players_value) {
  Cvars cv;
  double engine = -1, game = -1;
  {
    CvarLayer l(cv, "engine");
    cv.bind({.name = "fov", .fallback = "100", .help = "Across.", .min = 10, .max = 170}, &engine);
  }
  {
    CvarLayer l(cv, "game:first");
    Cvar& c = cv.bind({.name = "fov",
                       .fallback = "90",
                       .help = "The view across, degrees.",
                       .min = 10,
                       .max = 140,
                       .overlap = Overlap::same},
                      &game);
    CHECK(c.layer == "game:first" && c.value == "90" && c.fallback == "90");
    CHECK(engine == 90 && game == 90);
    CHECK(c.help == "The view across, degrees.");
    cv.set("fov", "160", Source::local);
    CHECK(c.value == "140" && engine == 140 && game == 140);  // the game's range
  }
  CHECK(cv.clashes().empty());
  // A player's value set before the game's layer came stays when it does.
  Cvars cv2;
  double e2 = 0;
  {
    CvarLayer l(cv2, "engine");
    cv2.bind({.name = "fov", .fallback = "100"}, &e2);
  }
  cv2.set("fov", "110", Source::local);
  CvarLayer l(cv2, "game:first");
  cv2.add({.name = "fov", .fallback = "90", .overlap = Overlap::same});
  CHECK(cv2.find("fov")->value == "110" && e2 == 110);
}

TEST(same_of_another_kind_is_a_clash) {
  Cvars cv;
  cv.add({.name = "crosshair", .fallback = "1", .kind = CvarKind::boolean});
  CvarLayer l(cv, "game:second");
  cv.add(
      {.name = "crosshair", .fallback = "ch1", .kind = CvarKind::text, .overlap = Overlap::same});
  REQUIRE(cv.clashes().size() == 1);
  CHECK(cv.clashes()[0].why.find("kind") != std::string::npos);
}

TEST(own_sets_the_earlier_aside_and_dropping_the_layer_brings_it_back) {
  Cvars cv;
  double engine = 0, game = 0;
  {
    CvarLayer l(cv, "engine");
    cv.bind({.name = "r_shadows", .fallback = "1", .help = "Shadows of the lights."}, &engine);
  }
  cv.set("r_shadows", "0.5", Source::local);
  CHECK(engine == 0.5);
  {
    CvarLayer l(cv, "game:first");
    Cvar& c = cv.bind({.name = "r_shadows",
                       .fallback = "0",
                       .help = "A blob under each model.",
                       .kind = CvarKind::boolean,
                       .overlap = Overlap::own},
                      &game);
    CHECK(c.layer == "game:first" && c.value == "0" && c.kind == CvarKind::boolean);
    cv.set("r_shadows", "1", Source::local);
    CHECK(game == 1 && engine == 0.5);  // the engine's field is not this setting
  }
  CHECK(cv.clashes().empty());
  CHECK(cv.drop_layer("game:first") == 1);
  const Cvar* back = cv.find("r_shadows");
  REQUIRE(back != nullptr);
  CHECK(back->layer == "engine" && back->value == "0.5" && back->help == "Shadows of the lights.");
  cv.set("r_shadows", "0.25", Source::local);
  CHECK(engine == 0.25 && game == 1);
}

TEST(a_dropped_layers_own_cvars_are_gone_until_registered_again) {
  Cvars cv;
  double quad = 0;
  {
    CvarLayer l(cv, "game:first");
    cv.bind({.name = "v_quadcshift", .fallback = "1"}, &quad);
  }
  const size_t live = cv.size();
  CHECK(cv.drop_layer("game:first") == 1);
  CHECK(cv.find("v_quadcshift") == nullptr);
  CHECK(cv.size() == live - 1);
  CHECK(cv.set("v_quadcshift", "0", Source::local) == SetResult::unknown);
  CHECK(catalog_text(cv).find("v_quadcshift") == std::string::npos);
  {
    CvarLayer l(cv, "game:first");
    Cvar& c = cv.bind({.name = "v_quadcshift", .fallback = "0.5"}, &quad);
    CHECK(c.value == "0.5" && quad == 0.5 && cv.find("v_quadcshift") == &c);
  }
}

TEST(a_layers_other_names_go_with_it) {
  Cvars cv;
  double gamma = 1;
  cv.bind({.name = "gamma", .fallback = "1"}, &gamma);
  {
    CvarLayer l(cv, "game:second");
    CHECK(cv.add_synonym("vid_gamma", "gamma"));
  }
  CHECK(cv.set("vid_gamma", "0.7", Source::local) == SetResult::changed);
  CHECK(gamma == 0.7);
  CHECK(cv.drop_layer("game:second") == 1);
  CHECK(cv.find("vid_gamma") == nullptr && cv.synonym("vid_gamma") == nullptr);
  CHECK(cv.set("vid_gamma", "0.5", Source::local) == SetResult::unknown);
}

TEST(a_layer_stack_unwinds_in_order) {
  Cvars cv;
  double e = 0, p = 0, g = 0;
  {
    CvarLayer l(cv, "engine");
    cv.bind({.name = "cl_bob", .fallback = "0.01"}, &e);
  }
  {
    CvarLayer l(cv, "shooter");
    cv.bind({.name = "cl_bob", .fallback = "0.02", .overlap = Overlap::same}, &p);
  }
  {
    CvarLayer l(cv, "game:second");
    cv.bind({.name = "cl_bob", .fallback = "0", .overlap = Overlap::own}, &g);
  }
  CHECK(cv.find("cl_bob")->value == "0" && e == 0.02 && p == 0.02);
  cv.drop_layer("game:second");
  CHECK(cv.find("cl_bob")->value == "0.02" && cv.find("cl_bob")->layer == "shooter");
  cv.drop_layer("shooter");
  CHECK(cv.find("cl_bob")->value == "0.01" && cv.find("cl_bob")->layer == "engine");
  cv.set("cl_bob", "0.03", Source::local);
  CHECK(e == 0.03 && p == 0.02);
  CHECK(cv.clashes().empty());
}

TEST(dropping_a_same_layer_keeps_the_players_value_and_restores_an_unset_one) {
  Cvars cv;
  cv.add({.name = "fov", .fallback = "100"});
  cv.add({.name = "scale", .fallback = "2"});
  {
    CvarLayer l(cv, "game:first");
    cv.add({.name = "fov", .fallback = "90", .overlap = Overlap::same});
    cv.add({.name = "scale", .fallback = "2.5", .overlap = Overlap::same});
  }
  cv.set("fov", "120", Source::local);  // one setting: the player's value goes on
  cv.drop_layer("game:first");
  CHECK(cv.find("fov")->value == "120" && cv.find("fov")->fallback == "100");
  CHECK(cv.find("scale")->value == "2");  // never set: the earlier default again
}

// A game's own spelling of one of our commands runs ours while the game's layer lasts.
TEST(a_games_command_synonym_goes_with_its_layer) {
  Console c(false);
  int ran = 0;
  c.add_command("screenshot", "A picture of the screen.", [&ran](Console&, const Args&) { ++ran; });
  c.cvars().set_layer("game:second");
  c.add_command_synonym("snap", "screenshot");
  c.cvars().set_layer("engine");
  c.execute("snap");
  CHECK(ran == 1 && c.has_command("snap"));
  CHECK(c.drop_layer("game:second") == 1);
  CHECK(!c.has_command("snap"));
  c.execute("snap");  // unknown now
  CHECK(ran == 1);
}

// SCOPES: a game's own value over a common setting. In a config the game's spelling wins in any
// order; typed, our name sets the common value and clears the loaded game's.
TEST(a_games_own_value_stands_over_the_common_one_in_a_config_in_any_order) {
  Console c(false);
  c.cvars().add({.name = "gamma", .fallback = "1", .flags = kArchive});
  c.cvars().add({.name = "sensitivity", .fallback = "3", .flags = kArchive});
  c.cvars().set_scope("second");
  c.cvars().set_layer("game:second");
  c.cvars().add_synonym("vid_gamma", "gamma");
  c.cvars().set_layer("engine");
  const Origin cfg{"config.cfg", 1};
  c.append("vid_gamma 0.9\ngamma 0.5\nfirst:sensitivity 7\n", Source::local, cfg);
  c.run();
  CHECK(c.cvars().find("gamma")->value == "0.9");  // the game's, though our name came after
  CHECK(c.cvars().find("gamma")->common == "0.5");
  CHECK(c.cvars().find("sensitivity")->value == "3");  // another game's: kept for it
  const std::string text = c.save_text();
  CHECK_MSG(text.find("seta gamma \"0.5\"") != std::string::npos, text);
  CHECK(text.find("seta second:gamma \"0.9\"") != std::string::npos);
  CHECK(text.find("seta first:sensitivity \"7\"") != std::string::npos);
  CHECK(c.save_text({.scopes = [](std::string_view s) {
           return s == "first";
         }}).find("second:gamma") == std::string::npos);
  // Another game loaded: the common value, and that game's own.
  c.drop_layer("game:second");
  c.cvars().set_scope("first");
  CHECK(c.cvars().find("gamma")->value == "0.5");
  CHECK(c.cvars().find("sensitivity")->value == "7");
  c.cvars().set_scope("second");
  CHECK(c.cvars().find("gamma")->value == "0.9" && c.cvars().find("sensitivity")->value == "3");
  // Typed: game:gamma is this game's; plain gamma the common one, this game's given up for it.
  c.execute("game:gamma 0.8");
  CHECK(c.cvars().find("gamma")->value == "0.8" && c.cvars().find("gamma")->common == "0.5");
  c.execute("gamma 1.2");
  CHECK(c.cvars().find("gamma")->value == "1.2" && c.cvars().find("gamma")->scoped.empty());
  c.execute("game:gamma 0.7");
  c.execute("unset game:gamma");
  CHECK(c.cvars().find("gamma")->value == "1.2");
  // A saved config read back gives the same.
  c.execute("game:gamma 0.6");
  const std::string saved = c.save_text();
  Console d(false);
  d.cvars().add({.name = "gamma", .fallback = "1", .flags = kArchive});
  d.cvars().add({.name = "sensitivity", .fallback = "3", .flags = kArchive});
  d.cvars().set_scope("second");
  d.append(saved, Source::local, cfg);
  d.run();
  CHECK(d.cvars().find("gamma")->value == "0.6" && d.cvars().find("gamma")->common == "1.2");
  CHECK(d.cvars().find("sensitivity")->scoped.at("first") == "7");
}

TEST(a_named_save_under_a_game_is_a_pair_and_exec_reads_both) {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "ge_console_cvar_layers_pair_test";
  std::filesystem::remove_all(dir);
  const auto make = [&dir](Console& c) {
    c.set_search_path({dir});
    c.cvars().add({.name = "gamma", .fallback = "1", .flags = kArchive});
    c.cvars().set_scope("baseq2");
  };
  CHECK(game_file("empezar.cfg", "baseq2") == "empezar.baseq2.cfg");
  CHECK(game_file("empezar.baseq2.cfg", "baseq2").empty() && game_file("a.cfg", "").empty());
  Console c(false);
  make(c);
  c.execute("gamma 1.1");
  c.execute("game:gamma 0.9");
  c.execute("first:gamma 0.7");
  c.execute("cfg_save empezar");
  std::string common;
  std::string own;
  CHECK(c.read_file("empezar.cfg", common) && c.read_file("empezar.baseq2.cfg", own));
  CHECK_MSG(common.find("seta gamma \"1.1\"") != std::string::npos &&
                common.find("first:gamma") != std::string::npos &&
                common.find("baseq2:gamma") == std::string::npos,
            common);
  CHECK_MSG(own.find("seta baseq2:gamma \"0.9\"") != std::string::npos &&
                own.find("seta gamma") == std::string::npos,
            own);
  Console d(false);
  make(d);
  d.execute("exec empezar");
  CHECK(d.cvars().find("gamma")->value == "0.9" && d.cvars().find("gamma")->common == "1.1");
  std::filesystem::remove_all(dir);
}
