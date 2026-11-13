// Tests: every setting a Shooter's Edge player can set explains itself (console/words.hpp): one
// sentence of help and what each value does, for the console's own, the controls, the picture,
// the held weapon, the player's info, the HUD, the menu, coop and screenshots. The window's few
// (client/window_cvars.cpp) and the frame pacing's need a window and are held by their own tests.
//
// ge:summary Tests: every client setting has one sentence of help and what each value does
// ge:keywords test client-settings words_fault cvar-help explanation setting-words
// ge:skill console testing
#include <string>

#include "capture/screenshot.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "console/words.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/coop_mode.hpp"
#include "shooters_edge/client/settings.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/view/hand.hpp"

namespace cl = shooters_edge::client;
namespace con = ge::console;

TEST(every_client_setting_explains_itself) {
  con::Console console{false};
  cl::CoopMode coop;
  cl::add_coop_cvars(console, coop);
  cl::ControlSettings controls;
  cl::add_cvars(console, controls);
  shooters_edge::look::Picture picture;
  shooters_edge::look::add_cvars(console, picture);
  shooters_edge::view::ViewSettings view;
  shooters_edge::view::add_view_cvars(console.cvars(), view);
  cl::UserInfo info;
  cl::add_user_info_cvars(console, info);
  shooters_edge::hud::Hud hud = shooters_edge::hud::make_hud();
  shooters_edge::hud::add_cvars(console, hud);
  shooters_edge::menu::GameMenu menu;
  menu.add_cvars(console);
  ge::capture::ScreenshotSettings shots;
  ge::capture::add_screenshot_cvars(console.cvars(), shots);
  int count = 0, faults = 0;
  for (const con::Cvar* v : console.cvars().sorted()) {
    std::string fault = con::words_fault(*v);
    if (fault.empty() && v->values.empty() && v->kind != con::CvarKind::text)
      fault = "no values say what each value does";
    if (!fault.empty()) faults++;
    CHECK_MSG(fault.empty(), v->name + ": " + fault);
    count++;
  }
  CHECK_MSG(faults == 0, std::to_string(faults) + " of " + std::to_string(count));
  CHECK(count > 50);
}
