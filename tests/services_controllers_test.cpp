// services/controllers.hpp and sim_controllers.hpp: a pad the store reads drives the game's
// actions exactly as the same pad read by engine/input does, merges with the keyboard, names
// its controls with a glyph and its family's label, follows the player's edits, reads one set
// and its layers, and the action manifest and default configuration are written from the map.
//
// ge:summary Test: the store controller layer reads Fall Line's actions like engine/input does
// ge:keywords test services steam-input controllers origins glyph merge action-set layer manifest
// ge:skill services input testing
#include <string>

#include "check.hpp"
#include "fall_line/controls/bindings.hpp"
#include "input/actions.hpp"
#include "input/names.hpp"
#include "input/sim.hpp"
#include "services/controllers.hpp"
#include "services/sim_controllers.hpp"

using namespace ge;
using services::Result;

namespace {

bool same(const input::ActionValue& a, const input::ActionValue& b) {
  return a.x == b.x && a.y == b.y && a.held == b.held && a.pressed == b.pressed &&
         a.released == b.released;
}

struct Step {
  int frame;
  bool button;
  int code;
  float value;
};

}  // namespace

TEST(a_store_read_pad_gives_the_actions_the_same_pad_gives_read_directly) {
  const input::ActionMap map = fall_line::controls::default_map();
  services::SimControllers store;
  CHECK(store.declare(map) == Result::ok);
  const auto c = store.connect(input::Family::xbox, "pad one");

  input::SimInput sim;
  input::InputState state;
  const input::DeviceId pad = input::sim_plug(sim, input::sim_pad(input::Family::xbox));

  const Step script[] = {
      {2, false, static_cast<int>(input::PadAxis::left_x), 0.6f},
      {3, true, static_cast<int>(input::PadButton::south), 1},
      {5, false, static_cast<int>(input::PadAxis::right_trigger), 0.8f},
      {6, true, static_cast<int>(input::PadButton::south), 0},
      {7, false, static_cast<int>(input::PadAxis::left_x), -1},
      {8, true, static_cast<int>(input::PadButton::north), 1},
      {9, true, static_cast<int>(input::PadButton::north), 0},
      {10, true, static_cast<int>(input::PadButton::back), 1},
      {11, false, static_cast<int>(input::PadAxis::left_trigger), 0.3f},
  };
  input::ActionValues direct = input::make_values(map), read = input::make_values(map);
  int compared = 0;
  for (int f = 0; f < 14; f++) {
    for (const Step& s : script) {
      if (s.frame != f) continue;
      if (s.button) {
        store.press(c, static_cast<input::PadButton>(s.code), s.value > 0);
        input::sim_button(sim, pad, s.code, s.value > 0);
      } else {
        store.move(c, static_cast<input::PadAxis>(s.code), s.value);
        input::sim_axis(sim, pad, s.code, s.value);
      }
    }
    store.frame(1.0 / 60);
    input::sim_advance(sim, 1.0 / 60);
    input::sim_frame(sim, state);
    input::Sources from;
    from.keyboard = from.mouse = false;
    input::evaluate(map, state, 1.0 / 60, direct, from);
    CHECK(store.read(c, 1.0 / 60, read));
    for (size_t i = 0; i < map.defs.size(); i++) {
      CHECK_MSG(same(direct[i], read[i]), "frame " + std::to_string(f) + " " + map.defs[i].name);
      compared++;
    }
  }
  CHECK(compared == 14 * static_cast<int>(map.defs.size()));
  // The tap of `north` (frame 8 down, 9 up) reached the reset, and the steer went full left.
  CHECK(read[static_cast<size_t>(map.find_action("steer"))].x == -1);
}

TEST(store_values_merge_with_the_keyboard_by_the_actions_rules) {
  const input::ActionMap map = fall_line::controls::default_map();
  services::SimControllers store;
  store.declare(map);
  const auto c = store.connect(input::Family::playstation, "pad");

  input::SimInput sim;
  input::InputState state;
  const input::DeviceId kb = input::sim_plug(sim, input::sim_keyboard());
  const auto d_key = input::key_from_name("d");
  CHECK(d_key.has_value());
  input::sim_key(sim, kb, d_key.value_or(0), true);
  store.press(c, input::PadButton::south, true);
  store.move(c, input::PadAxis::left_x, -0.3f);
  input::ActionValues mine = input::make_values(map), theirs = input::make_values(map);
  for (int f = 0; f < 30; f++) {
    input::sim_advance(sim, 1.0 / 60);
    input::sim_frame(sim, state);
    store.frame(1.0 / 60);
    input::evaluate(map, state, 1.0 / 60, mine);
    store.read(c, 1.0 / 60, theirs);
  }
  const auto steer = static_cast<size_t>(map.find_action("steer"));
  const auto jump = static_cast<size_t>(map.find_action("jump"));
  const float key_push = mine[steer].x;
  CHECK(key_push > 0.9f && !mine[jump].held);
  services::merge_store_values(map, theirs, mine);
  // The keys' push (near 1) is larger than the pad's -0.3, so it stays; the pad's jump is held.
  CHECK(mine[steer].x == key_push && mine[jump].held);
}

TEST(origins_name_each_control_with_a_glyph_and_follow_the_players_edit) {
  const input::ActionMap map = fall_line::controls::default_map();
  services::SimControllers store;
  store.declare(map);
  const auto x = store.connect(input::Family::xbox, "xbox pad");
  const auto p = store.connect(input::Family::playstation, "ps pad");
  const auto jump = static_cast<uint16_t>(map.find_action("jump"));

  // Every action the map gives a pad control has an origin with a glyph on both families.
  for (size_t i = 0; i < map.defs.size(); i++) {
    bool on_pad = false;
    for (const input::Context& k : map.contexts)
      for (const input::Binding& b : k.bindings)
        on_pad = on_pad || (b.action == i && services::store_readable(b.source));
    if (!on_pad) continue;
    for (const auto h : {x, p}) {
      const auto o = store.origins(h, static_cast<uint16_t>(i));
      CHECK_MSG(!o.empty() && !o[0].glyph.empty() && o[0].glyph != "unknown", map.defs[i].name);
    }
  }
  CHECK(store.origins(x, jump)[0].glyph == "xbox_a" && store.origins(x, jump)[0].label == "A");
  CHECK(store.origins(p, jump)[0].glyph == "ps_cross");

  input::Source east;
  east.kind = input::SourceKind::pad_button;
  east.code = static_cast<uint16_t>(input::PadButton::east);
  CHECK(store.rebind(p, "ride", "jump", east) == Result::ok);
  CHECK(store.origins(p, jump).size() == 1 && store.origins(p, jump)[0].glyph == "ps_circle");
  CHECK(store.origins(x, jump)[0].glyph == "xbox_a");  // one player's edit is his pad's alone

  input::ActionValues v = input::make_values(map);
  store.press(p, input::PadButton::east, true);
  store.frame(1.0 / 60);
  store.read(p, 1.0 / 60, v);
  CHECK(v[jump].pressed);

  input::Source key;
  key.kind = input::SourceKind::key;
  CHECK(store.rebind(p, "ride", "jump", key) == Result::invalid);
  CHECK(store.rebind(p, "ride", "fly", east) == Result::unknown_id);
  CHECK(store.rebind(p, "nowhere", "jump", east) == Result::unknown_id);
}

TEST(the_store_reads_one_set_and_its_layers) {
  const input::ActionMap map = fall_line::controls::default_map();
  services::SimControllers store;
  store.declare(map);  // ride and app are both on in the map: ride the set, app a layer
  const auto c = store.connect(input::Family::xbox, "pad");
  const auto camera = static_cast<size_t>(map.find_action("camera"));
  input::ActionValues v;
  store.press(c, input::PadButton::back, true);
  store.frame(1.0 / 60);
  store.read(c, 1.0 / 60, v);
  CHECK(v[camera].held);

  CHECK(store.activate("ride") == Result::ok);  // no layers now
  store.frame(1.0 / 60);
  store.read(c, 1.0 / 60, v);
  CHECK(!v[camera].held);
  CHECK(store.set_layer("app", true) == Result::ok);
  store.frame(1.0 / 60);
  store.read(c, 1.0 / 60, v);
  CHECK(v[camera].held);
  CHECK(store.activate("menu") == Result::unknown_id);

  CHECK(store.rumble(c, 2, 0.5f) == Result::ok && store.rumble_low(c) == 1);
  CHECK(store.show_bindings(c) == Result::ok && store.configurator_open());
  store.close_configurator();
  store.disconnect(c);
  CHECK(!store.read(c, 1.0 / 60, v) && store.controllers().empty());
  CHECK(store.rumble(c, 0, 0) == Result::not_found);
  CHECK(!store.journal().empty());

  input::ActionMap bad = map;
  bad.defs[1].name = bad.defs[0].name;
  CHECK(store.declare(bad) == Result::invalid);
}

TEST(the_manifest_and_default_configuration_are_written_from_the_map) {
  const input::ActionMap map = fall_line::controls::default_map();
  const std::string iga = services::action_manifest_text(map);
  CHECK(iga.find("\"In Game Actions\"") == 0);
  CHECK(iga.find("\"ride\"") != std::string::npos && iga.find("\"app\"") != std::string::npos);
  CHECK(iga.find("\"steer\"") != std::string::npos);
  CHECK(iga.find("\"input_mode\"\t\"joystick_move\"") != std::string::npos);
  CHECK(iga.find("\"tuck\"\t\"#Action_ride_tuck\"") != std::string::npos);
  CHECK(iga.find("\"jump\"\t\"#Action_ride_jump\"") != std::string::npos);
  CHECK(iga.find("rebind") == std::string::npos);  // a key-only action is not the store's
  const std::string cfg = services::default_config_text(map);
  CHECK(cfg.find("ride steer = pad:left_x") != std::string::npos);
  CHECK(cfg.find("ride jump = pad:south") != std::string::npos);
  CHECK(cfg.find("app camera = pad:back") != std::string::npos);
}
