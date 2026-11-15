// THE ORDER EACH CLASSIC GAME LOADS IN, as the client names it: the table in
// plugins/shooter/content/load_order.hpp (each game's files' ranks and its startup configs), its
// names brought into the client's namespace for client/install_configs.hpp and the window.
//
// ge:summary The client's names for each classic game's load order in plugins/shooter/content
// ge:keywords load-order startup-order using-declarations
// ge:skill shooter console
// ge:test tests/shooters_edge_install_configs_test.cpp
#pragma once

#include "shooter/content/load_order.hpp"

namespace shooters_edge::client {

using ge::shooter::content::load_order_of;
using ge::shooter::content::LoadOrder;
using ge::shooter::content::MainWhere;
using ge::shooter::content::StartStep;

}  // namespace shooters_edge::client
