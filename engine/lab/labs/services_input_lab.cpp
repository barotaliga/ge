// LAB `store-input`: a store's controller layer (Steam Input) reading a pad for a game, beside
// the same pad read by engine/input directly and a keyboard. One script on a virtual 60 Hz
// clock: the pad steers, tucks and jumps, the keyboard steers the other way, the player rebinds
// the jump in the store's configurator, the game opens its menu (a set with no pad steering)
// and comes back. It answers whether a store-read pad gives the game exactly the actions the
// same pad gives read directly (the gap must be 0), whether the merge with the keyboard follows
// the actions' rules, and what each action's prompt shows on each family of pad.
//
// ge:summary Lab: the store's controller layer against engine/input on one pad, with glyphs
// ge:keywords lab services steam-input controllers origins glyph merge configurator action-set
// ge:skill services input labs
// ge:test tests/services_controllers_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "input/actions.hpp"
#include "input/bindings_text.hpp"
#include "input/names.hpp"
#include "input/sim.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "services/controllers.hpp"
#include "services/sim_controllers.hpp"

namespace {

namespace gl = ge::lab;
namespace gs = ge::services;
namespace in = ge::input;

constexpr double kDt = 1.0 / 60;
constexpr int kFrames = 300;

// A racing game's map with a menu: the actions a store reads and one it does not (the console).
const char* const kBindings = R"(ge-bindings 1
context ride
bind steer key:a value=-1 ramp=6,9
bind steer key:d ramp=6,9
bind steer pad:left_x deadzone=0.12,0.97
bind throttle key:w ramp=4,12
bind throttle pad:right_trigger
bind look pad:right_stick rate=3
bind jump key:space
bind jump pad:south
bind pause key:escape
bind pause pad:start
context menu
bind confirm key:enter
bind confirm pad:south
bind back key:escape
bind back pad:east
bind move pad:left_stick
context app
bind console key:grave
)";

in::ActionMap make_map() {
  in::ActionMap map;
  map.defs = {
      {"steer", in::ActionKind::axis, false, "turn"},
      {"throttle", in::ActionKind::axis, true, "go"},
      {"look", in::ActionKind::delta, false, "look around"},
      {"jump", in::ActionKind::button, false, "jump"},
      {"pause", in::ActionKind::button, false, "the menu"},
      {"confirm", in::ActionKind::button, false, "choose"},
      {"back", in::ActionKind::button, false, "go back"},
      {"move", in::ActionKind::stick, false, "move the highlight"},
      {"console", in::ActionKind::button, false, "the console"},
  };
  std::string error;
  in::read_bindings(kBindings, map, error);
  map.set_active("menu", false);
  return map;
}

std::string num(double v, int digits) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.*f", digits, v);
  return buf;
}

void run(gl::Lab& lab) {
  const in::ActionMap base = make_map();
  in::ActionMap map = base;  // the game's own map: its menu switched with the store's set
  gs::SimControllers store;
  if (store.declare(base) != gs::Result::ok)
    lab.fail("store-input: the map was refused", "next: check its names; skill: services");
  const auto pad = store.connect(in::Family::xbox, "pad one");

  in::SimInput sim;
  in::InputState state;
  const in::DeviceId direct_pad = in::sim_plug(sim, in::sim_pad(in::Family::xbox));
  in::SimInput keys_sim;
  in::InputState keys_state;
  const in::DeviceId kb = in::sim_plug(keys_sim, in::sim_keyboard());
  const uint16_t a_key = in::key_from_name("a").value_or(0);

  // The direct pad reads only through the store's configuration's own bindings (the player's
  // edit included), so the two are compared on the same configuration.
  in::ActionMap direct_map = base;
  for (in::Context& c : direct_map.contexts)
    std::erase_if(c.bindings, [](const in::Binding& b) { return !gs::store_readable(b.source); });

  in::ActionValues from_store = in::make_values(base), from_direct = in::make_values(base),
                   from_keys = in::make_values(base), merged = in::make_values(base);
  const auto steer = static_cast<size_t>(base.find_action("steer"));
  const auto jump = static_cast<size_t>(base.find_action("jump"));
  gl::Trace t_store{"pad read by the store", {}, {}}, t_direct{"pad read directly", {}, {}},
      t_keys{"keyboard", {}, {}}, t_merged{"merged (what the game uses)", {}, {}};
  double gap = 0;
  int jumps_store = 0, jumps_merged = 0;

  auto both_axis = [&](in::PadAxis a, float v) {
    store.move(pad, a, v);
    in::sim_axis(sim, direct_pad, static_cast<int>(a), v);
  };
  auto both_button = [&](in::PadButton b, bool down) {
    store.press(pad, b, down);
    in::sim_button(sim, direct_pad, static_cast<int>(b), down);
  };

  for (int f = 0; f < kFrames; f++) {
    const double t = f * kDt;
    // The script.
    if (f >= 20 && f < 120)
      both_axis(in::PadAxis::left_x, static_cast<float>(ge::core::libm::sin((f - 20) * 0.05)));
    if (f == 120) both_axis(in::PadAxis::left_x, 0);
    if (f == 30) both_axis(in::PadAxis::right_trigger, 0.8f);
    if (f == 40 || f == 200) both_button(in::PadButton::south, true);
    if (f == 44 || f == 204) both_button(in::PadButton::south, false);
    if (f == 140) in::sim_key(keys_sim, kb, a_key, true);
    if (f == 180) in::sim_key(keys_sim, kb, a_key, false);
    if (f == 160) {
      // The player moves the jump to the east button in the store's configurator.
      store.show_bindings(pad);
      in::Source east;
      east.kind = in::SourceKind::pad_button;
      east.code = static_cast<uint16_t>(in::PadButton::east);
      store.rebind(pad, "ride", "jump", east);
      store.close_configurator();
      for (in::Binding& b : direct_map.find_context("ride")->bindings)
        if (b.action == jump) b = in::make_binding(static_cast<uint16_t>(jump), east);
    }
    if (f == 210) both_button(in::PadButton::east, true);
    if (f == 214) both_button(in::PadButton::east, false);
    if (f == 230) {  // the menu: a set of its own, the ride's steering not read
      store.activate("menu");
      map.set_active("ride", false);
      map.set_active("menu", true);
      direct_map.set_active("ride", false);
      direct_map.set_active("menu", true);
    }
    if (f == 232) both_axis(in::PadAxis::left_x, 0.9f);
    if (f == 270) {
      store.activate("ride");
      map.set_active("menu", false);
      map.set_active("ride", true);
      direct_map.set_active("menu", false);
      direct_map.set_active("ride", true);
      both_axis(in::PadAxis::left_x, 0);
    }
    if (f == 250) store.rumble(pad, 0.4f, 0.2f);
    if (f == 260) store.rumble(pad, 0, 0);

    // One frame: the store reads its pad, engine/input the same pad and the keyboard.
    store.frame(kDt);
    in::sim_advance(sim, kDt);
    in::sim_frame(sim, state);
    in::sim_advance(keys_sim, kDt);
    in::sim_frame(keys_sim, keys_state);
    store.read(pad, kDt, from_store);
    in::Sources pads_only;
    pads_only.keyboard = pads_only.mouse = false;
    in::evaluate(direct_map, state, kDt, from_direct, pads_only);
    in::evaluate(map, keys_state, kDt, from_keys);
    merged = from_keys;
    gs::merge_store_values(map, from_store, merged);

    for (size_t i = 0; i < base.defs.size(); i++) {
      const in::ActionValue& a = from_store[i];
      const in::ActionValue& b = from_direct[i];
      const double edge = a.held != b.held || a.pressed != b.pressed ? 1.0 : 0.0;
      gap = std::max({gap, static_cast<double>(std::fabs(a.x - b.x)),
                      static_cast<double>(std::fabs(a.y - b.y)), edge});
    }
    jumps_store += from_store[jump].pressed ? 1 : 0;
    jumps_merged += merged[jump].pressed ? 1 : 0;
    t_store.x.push_back(t);
    t_store.y.push_back(from_store[steer].x);
    t_direct.x.push_back(t);
    t_direct.y.push_back(from_direct[steer].x);
    t_keys.x.push_back(t);
    t_keys.y.push_back(from_keys[steer].x);
    t_merged.x.push_back(t);
    t_merged.y.push_back(merged[steer].x);
  }

  if (gap != 0)
    lab.fail(
        "store-input: the store-read pad differs from the same pad read directly by " + num(gap, 6),
        "next: compare sim_controllers.cpp's configuration with the map; skill: services");
  if (jumps_store != 2 || jumps_merged != 2)
    lab.fail("store-input: " + std::to_string(jumps_store) +
                 " jumps reached the game, owed 2 (south before the rebind, east after)",
             "next: check rebind and the merge's edges; skill: services");
  lab.metric("store-vs-direct-gap", gap, "value");
  lab.metric("jumps", jumps_merged, "count");

  gl::LineChart lc;
  lc.frame = {"Steer by source: store-read pad, direct pad, keyboard, merged",
              "60 Hz; the menu from 3.83 s to 4.5 s reads no steering", "t (s)", "steer (-1 .. 1)"};
  lc.traces = {t_store, t_direct, t_keys, t_merged};
  lc.marks = {{160 * kDt, "jump rebound"}, {230 * kDt, "menu"}, {270 * kDt, "ride"}};
  const gl::Chart ch = gl::line_chart(lc);
  lab.shot(
      {"steer",
       ch.title,
       "The store-read pad's steer lies exactly under the directly-read pad's and the merged line "
       "(drawn last, on top); the keyboard steers left from 2.3 s to 3 s, and the merge takes "
       "the larger push of the two.",
       "the store and direct lines identical; the merged line following whichever is "
       "further from 0; no steer during the menu",
       {{"rate", "60 Hz"}, {"family", "xbox"}}},
      gl::to_picture(ch.plot));

  // The prompts: what each action shows on each family, from a fresh pad of each.
  gl::LabTable glyphs{"Each action's prompt on each family of pad (the store's defaults)",
                      {"set", "action", "xbox", "playstation", "nintendo", "generic"},
                      {}};
  const in::Family fams[] = {in::Family::xbox, in::Family::playstation, in::Family::nintendo,
                             in::Family::generic};
  std::vector<gs::ControllerHandle> each;
  for (const in::Family f : fams) each.push_back(store.connect(f, std::string(in::family_name(f))));
  int missing = 0;
  for (const char* set : {"ride", "menu"}) {
    store.activate(set);
    for (size_t i = 0; i < base.defs.size(); i++) {
      std::vector<std::string> row{set, base.defs[i].name};
      bool any = false;
      for (const auto h : each) {
        const auto o = store.origins(h, static_cast<uint16_t>(i));
        std::string cell;
        for (const gs::Origin& g : o)
          cell += (cell.empty() ? "" : ", ") + g.glyph + " (" + g.label + ")";
        any = any || !o.empty();
        row.push_back(cell.empty() ? "-" : cell);
      }
      if (!any) continue;
      for (size_t k = 2; k < row.size(); k++) missing += row[k] == "-" ? 1 : 0;
      glyphs.rows.push_back(std::move(row));
    }
  }
  if (missing)
    lab.fail("store-input: " + std::to_string(missing) + " prompts have no glyph",
             "next: check origin_of in controllers.cpp; skill: services");
  lab.table(std::move(glyphs));

  gl::LabTable journal{"The store's journal", {"line"}, {}};
  for (const std::string& l : store.journal()) journal.rows.push_back({l});
  lab.table(std::move(journal));
  lab.attach("game_actions.vdf", [&] {
    const std::string s = gs::action_manifest_text(base);
    return std::vector<uint8_t>(s.begin(), s.end());
  }());
  lab.attach("default_config.txt", [&] {
    const std::string s = gs::default_config_text(base);
    return std::vector<uint8_t>(s.begin(), s.end());
  }());
}

const gl::LabSpec kSpec{
    "store-input",
    gl::LabKind::scenario,
    "engine",
    "engine/services/controllers.hpp (the store's controller layer, Steam Input's shape)",
    "services",
    "Does a pad read by the store's controller layer give the game exactly the actions the same "
    "pad gives read directly, merged with the keyboard by the actions' rules, through a rebind "
    "and a change of set, and does every action show a glyph on every family of pad?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
