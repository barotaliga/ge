// The store's controller layer in memory: controllers connected by a script, held and moved
// control by control, read through the store's own configuration (the game's pad bindings as
// its defaults, then the player's edits made in the configurator), on a virtual clock. It reads
// a pad the way the store does: one action set at a time, the player's configuration per
// controller, the action values the game's own map would give for the same pad. A journal line
// per thing the store did, so a script replays to the same text.
//
// ge:summary The simulated store controller layer: pads in code, its configuration, a journal
// ge:keywords services steam-input simulated-controllers store-configuration configurator rebind
// ge:keywords action-set journal virtual-pad
// ge:skill services input
// ge:test tests/services_controllers_test.cpp
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "services/controllers.hpp"

namespace ge::services {

class SimControllers final : public ControllerLayer {
 public:
  SimControllers();
  ~SimControllers() override;
  SimControllers(const SimControllers&) = delete;
  SimControllers& operator=(const SimControllers&) = delete;

  Result declare(const input::ActionMap& map) override;
  Result activate(std::string_view context) override;
  Result set_layer(std::string_view context, bool on) override;
  std::span<const ControllerInfo> controllers() const override;
  bool read(ControllerHandle c, double dt_s, input::ActionValues& out) override;
  std::vector<Origin> origins(ControllerHandle c, uint16_t action) const override;
  Result rumble(ControllerHandle c, float low, float high) override;
  Result show_bindings(ControllerHandle c) override;

  // The script: a controller connects (it gets the declared defaults) and leaves.
  ControllerHandle connect(input::Family family, std::string name);
  void disconnect(ControllerHandle c);
  // Its controls, by position; they hold until changed.
  void press(ControllerHandle c, input::PadButton b, bool down);
  void move(ControllerHandle c, input::PadAxis a, float value);
  // One frame of the store's reading: everything pressed and moved since the last one lands.
  void frame(double dt_s);
  // The player's edit in the configurator: `action` in `context` is now on `source` alone (on
  // this controller). `invalid` for a source a store cannot read; `unknown_id` for a name.
  Result rebind(ControllerHandle c, std::string_view context, std::string_view action,
                const input::Source& source);
  // What the motors were last told, and whether the configurator is open.
  float rumble_low(ControllerHandle c) const;
  float rumble_high(ControllerHandle c) const;
  bool configurator_open() const;
  void close_configurator();

  const std::vector<std::string>& journal() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ge::services
