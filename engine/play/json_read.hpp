// The flat JSON reader under the play protocol's names: the reader itself is
// `core/json_read.hpp` (shared with the services' Discord link); the protocol, `ge diverge` and
// the cli's answers keep spelling it `play::`.
//
// ge:summary The flat JSON reader (core/json_read.hpp) under the play namespace
// ge:keywords json read parse flat dotted path protocol alias
// ge:skill agent-interface
// ge:test tests/play_session_test.cpp
#pragma once

#include "core/json_read.hpp"

namespace ge::play {

using core::FlatJson;
using core::json_text;
using core::JsonValue;
using core::read_flat;

}  // namespace ge::play
