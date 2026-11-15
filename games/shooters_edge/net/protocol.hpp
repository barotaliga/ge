// THE PROTOCOL FAMILY IN THE GAME'S NET CODE: the classic protocol family's wire, net channel and
// info strings (plugins/classic/protocol) named in `shooters_edge::net`, so the first game's
// protocol (classic_*.hpp) and the sequel's (sequel_*.hpp) both read them as their own words. A
// game's own protocol adds only what is its alone: its version, ports, messages, move checksum
// and usercmd (classic_wire.hpp, sequel_net.hpp).
//
// ge:summary The classic protocol family's wire, channel and info strings named in the game's net
// ge:keywords protocol-family wire net-channel info-string using-declarations
// ge:skill net shooter
// ge:test tests/classic_protocol_test.cpp
#pragma once

#include "classic/protocol/channel.hpp"
#include "classic/protocol/info.hpp"
#include "classic/protocol/wire.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;
namespace sp = ge::classic::protocol;

// The wire.
using sp::angle16_from_wire;
using sp::angle16_to_wire;
using sp::angle_from_wire;
using sp::angle_to_wire;
using sp::coord_from_wire;
using sp::coord_grid;
using sp::coord_to_wire;
using sp::Coords;
using sp::crc16;
using sp::from_hex;
using sp::kClientHeader;
using sp::kMaxMsgLen;
using sp::kMaxPacket;
using sp::kServerHeader;
using sp::read_angle;
using sp::read_angle16;
using sp::read_byte;
using sp::read_char;
using sp::read_coord;
using sp::read_float;
using sp::read_long;
using sp::read_origin;
using sp::read_short;
using sp::read_string;
using sp::to_hex;
using sp::write_angle;
using sp::write_angle16;
using sp::write_byte;
using sp::write_char;
using sp::write_coord;
using sp::write_float;
using sp::write_long;
using sp::write_origin;
using sp::write_short;
using sp::write_string;

// The net channel.
using sp::Channel;
using sp::channel_can_packet;
using sp::channel_process;
using sp::channel_reliable;
using sp::channel_reliable_backlog;
using sp::channel_set_rate;
using sp::channel_setup;
using sp::channel_transmit;
using sp::is_out_of_band;
using sp::kBackBuffers;
using sp::kRateBurstBytes;
using sp::MsgBuf;
using sp::peek_qport;

// The info strings.
using sp::info_get;
using sp::info_pairs;
using sp::info_public;
using sp::info_remove;
using sp::info_set;
using sp::info_valid;
using sp::kInfoKeyBytes;
using sp::kInfoMaxKeys;
using sp::kServerInfoBytes;
using sp::kUserInfoBytes;
using sp::sanitize_name;
using sp::unique_name;

}  // namespace shooters_edge::net
