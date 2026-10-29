// The simulated store controller layer: each controller a simulated pad (input/sim.hpp) in one
// input state of the store's own, read through that controller's copy of the store's
// configuration with engine/input's own `evaluate`, so a store-read pad gives what the game's
// map would give for the same pad.
//
// ge:summary The simulated store controller layer: pads read through the store configuration
// ge:keywords services steam-input simulated-controllers configurator rebind action-set journal
// ge:skill services input
// ge:test tests/services_controllers_test.cpp
#include "services/sim_controllers.hpp"

#include <algorithm>
#include <string>

#include "core/num_text.hpp"
#include "input/bindings_text.hpp"
#include "input/names.hpp"
#include "input/sim.hpp"

namespace ge::services {

namespace {

struct Pad {
  ControllerInfo info;
  input::DeviceId device = 0;
  input::ActionMap config;  // the store's configuration for this controller: pad sources only
  float low = 0, high = 0;
};

bool has_store_binding(const input::Context& c) {
  return std::any_of(c.bindings.begin(), c.bindings.end(),
                     [](const input::Binding& b) { return store_readable(b.source); });
}

// The store's configuration a controller starts with: the game's map with only the bindings a
// store reads; which sets are read is set by `reading`.
input::ActionMap store_defaults(const input::ActionMap& map) {
  input::ActionMap out;
  out.defs = map.defs;
  for (const input::Context& c : map.contexts) {
    input::Context k;
    k.name = c.name;
    k.active = false;
    for (const input::Binding& b : c.bindings)
      if (store_readable(b.source) && b.with.kind == input::SourceKind::none)
        k.bindings.push_back(b);
    out.contexts.push_back(std::move(k));
  }
  return out;
}

}  // namespace

struct SimControllers::Impl {
  input::ActionMap declared;
  bool has_map = false;
  std::string active;
  std::vector<std::string> layers;
  std::vector<Pad> pads;
  std::vector<ControllerInfo> infos;
  ControllerHandle next = 1;
  input::SimInput sim;
  input::InputState state;
  bool configurator = false;
  std::vector<std::string> journal;

  Pad* find(ControllerHandle c) {
    for (Pad& p : pads)
      if (p.info.handle == c) return &p;
    return nullptr;
  }
  const Pad* find(ControllerHandle c) const {
    for (const Pad& p : pads)
      if (p.info.handle == c) return &p;
    return nullptr;
  }
  bool reads(std::string_view name) const {
    return name == active || std::find(layers.begin(), layers.end(), name) != layers.end();
  }
  void reading(Pad& p) const {
    for (input::Context& c : p.config.contexts) c.active = reads(c.name);
  }
  void refresh_infos() {
    infos.clear();
    for (const Pad& p : pads) infos.push_back(p.info);
  }
  void log(std::string line) {
    journal.push_back(core::num::to_fixed(static_cast<double>(sim.t_ns) / 1e9, 3) + " " +
                      std::move(line));
  }
};

SimControllers::SimControllers() : impl_(std::make_unique<Impl>()) {}
SimControllers::~SimControllers() = default;

Result SimControllers::declare(const input::ActionMap& map) {
  for (size_t i = 0; i < map.defs.size(); i++) {
    if (map.defs[i].name.empty()) return Result::invalid;
    for (size_t j = 0; j < i; j++)
      if (map.defs[j].name == map.defs[i].name) return Result::invalid;
  }
  for (size_t i = 0; i < map.contexts.size(); i++) {
    if (map.contexts[i].name.empty()) return Result::invalid;
    for (size_t j = 0; j < i; j++)
      if (map.contexts[j].name == map.contexts[i].name) return Result::invalid;
  }
  Impl& m = *impl_;
  m.declared = map;
  m.has_map = true;
  m.active.clear();
  m.layers.clear();
  for (const input::Context& c : map.contexts) {
    if (!c.active || !has_store_binding(c)) continue;
    if (m.active.empty())
      m.active = c.name;
    else
      m.layers.push_back(c.name);
  }
  for (Pad& p : m.pads) {
    p.config = store_defaults(map);
    m.reading(p);
  }
  std::string line = "declare " + std::to_string(map.defs.size()) + " actions in " +
                     std::to_string(map.contexts.size()) + " sets, active " +
                     (m.active.empty() ? "none" : m.active);
  for (const std::string& l : m.layers) line += " +" + l;
  m.log(std::move(line));
  return Result::ok;
}

Result SimControllers::activate(std::string_view context) {
  Impl& m = *impl_;
  if (!m.has_map || !m.declared.find_context(context)) return Result::unknown_id;
  if (m.active == context && m.layers.empty()) return Result::ok;
  m.active = std::string(context);
  m.layers.clear();
  for (Pad& p : m.pads) m.reading(p);
  m.log("activate " + m.active);
  return Result::ok;
}

Result SimControllers::set_layer(std::string_view context, bool on) {
  Impl& m = *impl_;
  if (!m.has_map || !m.declared.find_context(context)) return Result::unknown_id;
  const auto it = std::find(m.layers.begin(), m.layers.end(), context);
  if (on == (it != m.layers.end())) return Result::ok;
  if (on)
    m.layers.emplace_back(context);
  else
    m.layers.erase(it);
  for (Pad& p : m.pads) m.reading(p);
  m.log(std::string(on ? "layer on " : "layer off ") + std::string(context));
  return Result::ok;
}

std::span<const ControllerInfo> SimControllers::controllers() const { return impl_->infos; }

bool SimControllers::read(ControllerHandle c, double dt_s, input::ActionValues& out) {
  Impl& m = *impl_;
  Pad* p = m.find(c);
  if (!p || !m.has_map) return false;
  if (out.v.size() != m.declared.defs.size()) out = input::make_values(m.declared);
  input::Sources from;
  from.only = std::span<const input::DeviceId>(&p->device, 1);
  from.keyboard = false;
  from.mouse = false;
  input::evaluate(p->config, m.state, dt_s, out, from);
  return true;
}

std::vector<Origin> SimControllers::origins(ControllerHandle c, uint16_t action) const {
  std::vector<Origin> out;
  const Pad* p = impl_->find(c);
  if (!p) return out;
  for (const input::Context& k : p->config.contexts) {
    if (!k.active) continue;
    for (const input::Binding& b : k.bindings)
      if (b.action == action) out.push_back(origin_of(p->info.family, b.source));
  }
  return out;
}

Result SimControllers::rumble(ControllerHandle c, float low, float high) {
  Pad* p = impl_->find(c);
  if (!p) return Result::not_found;
  low = std::clamp(low, 0.0f, 1.0f);
  high = std::clamp(high, 0.0f, 1.0f);
  if (low != p->low || high != p->high)
    impl_->log("rumble " + std::to_string(c) + " " + core::num::to_fixed(low, 2) + " " +
               core::num::to_fixed(high, 2));
  p->low = low;
  p->high = high;
  return Result::ok;
}

Result SimControllers::show_bindings(ControllerHandle c) {
  Impl& m = *impl_;
  if (!m.find(c)) return Result::not_found;
  if (!m.has_map) return Result::unsupported;
  m.configurator = true;
  m.log("configurator " + std::to_string(c));
  return Result::ok;
}

ControllerHandle SimControllers::connect(input::Family family, std::string name) {
  Impl& m = *impl_;
  Pad p;
  p.info.handle = m.next++;
  p.info.family = family;
  p.info.name = std::move(name);
  input::DeviceInfo d = input::sim_pad(family);
  d.name = p.info.name;
  p.device = input::sim_plug(m.sim, d);
  if (m.has_map) {
    p.config = store_defaults(m.declared);
    m.reading(p);
  }
  m.log("connect " + std::to_string(p.info.handle) + " " + std::string(input::family_name(family)) +
        " " + p.info.name);
  m.pads.push_back(std::move(p));
  m.refresh_infos();
  return m.pads.back().info.handle;
}

void SimControllers::disconnect(ControllerHandle c) {
  Impl& m = *impl_;
  auto it =
      std::find_if(m.pads.begin(), m.pads.end(), [c](const Pad& p) { return p.info.handle == c; });
  if (it == m.pads.end()) return;
  input::sim_unplug(m.sim, it->device);
  m.pads.erase(it);
  m.refresh_infos();
  m.log("disconnect " + std::to_string(c));
}

void SimControllers::press(ControllerHandle c, input::PadButton b, bool down) {
  if (const Pad* p = impl_->find(c))
    input::sim_button(impl_->sim, p->device, static_cast<int>(b), down);
}

void SimControllers::move(ControllerHandle c, input::PadAxis a, float value) {
  if (const Pad* p = impl_->find(c))
    input::sim_axis(impl_->sim, p->device, static_cast<int>(a), value);
}

void SimControllers::frame(double dt_s) {
  input::sim_advance(impl_->sim, dt_s);
  input::sim_frame(impl_->sim, impl_->state);
}

Result SimControllers::rebind(ControllerHandle c, std::string_view context, std::string_view action,
                              const input::Source& source) {
  Impl& m = *impl_;
  Pad* p = m.find(c);
  if (!p) return Result::not_found;
  if (!store_readable(source)) return Result::invalid;
  input::Context* k = p->config.find_context(context);
  const int a = p->config.find_action(action);
  if (!k || a < 0) return Result::unknown_id;
  const auto i = static_cast<uint16_t>(a);
  std::erase_if(k->bindings, [i](const input::Binding& b) { return b.action == i; });
  k->bindings.push_back(input::make_binding(i, source));
  m.log("rebind " + std::to_string(c) + " " + std::string(context) + " " + std::string(action) +
        " = " + input::source_text(source));
  return Result::ok;
}

float SimControllers::rumble_low(ControllerHandle c) const {
  const Pad* p = impl_->find(c);
  return p ? p->low : 0;
}
float SimControllers::rumble_high(ControllerHandle c) const {
  const Pad* p = impl_->find(c);
  return p ? p->high : 0;
}
bool SimControllers::configurator_open() const { return impl_->configurator; }
void SimControllers::close_configurator() {
  if (!impl_->configurator) return;
  impl_->configurator = false;
  impl_->log("configurator closed");
}

const std::vector<std::string>& SimControllers::journal() const { return impl_->journal; }

}  // namespace ge::services
