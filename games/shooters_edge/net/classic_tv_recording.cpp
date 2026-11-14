// A client recording played into the classic clients' face (classic_tv_recording.hpp): its records
// walked by their times, packets handed on, commands' angles as the view, round and round.
//
// ge:summary A client recording's records walked by time into the classic clients' face
// ge:keywords classic-tv client-recording records udp_tv_recording_update loop command-size
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_tv_udp_test.cpp
#include "shooters_edge/net/classic_tv_recording.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>
#include <utility>

#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_scan.hpp"

namespace shooters_edge::net {

namespace {

constexpr uint32_t kMostPacket = 1u << 16;
constexpr int64_t kRoundGapUs = 2000000;
constexpr double kMostGapS = 1;  // a longer silence in the recording plays as this long
constexpr size_t kChunk = 4096;  // source bytes a view's conversion is fed at a time
constexpr double kAheadS = 0.5;  // how far ahead of the clock a view is converted
constexpr double kIdleS = 10;    // an unwatched view is let go after this long

float f32_at(const std::vector<uint8_t>& b, size_t at) {
  float v = 0;
  std::memcpy(&v, b.data() + at, 4);  // little-endian
  return v;
}

uint32_t u32_at(const std::vector<uint8_t>& b, size_t at) {
  uint32_t v = 0;
  std::memcpy(&v, b.data() + at, 4);
  return v;
}

// The record at `at` and its size; 0 when none whole is there.
size_t record_size(const std::vector<uint8_t>& b, size_t at, int cmd_size) {
  if (at + 5 > b.size()) return 0;
  const float t = f32_at(b, at);
  const uint8_t type = b[at + 4];
  if (!std::isfinite(t)) return 0;
  size_t n = 5;
  if (type == 0) {
    n += static_cast<size_t>(cmd_size) + 12;
  } else if (type == 2) {
    n += 8;
  } else if (type == 1) {
    if (at + 9 > b.size()) return 0;
    const uint32_t len = u32_at(b, at + 5);
    if (len < 8 || len > kMostPacket) return 0;
    n += 4 + len;
  } else {
    return 0;
  }
  return at + n <= b.size() ? n : 0;
}

// True when the records from `at` walk whole with `cmd_size` for `count` records or to the end.
bool walks(const std::vector<uint8_t>& b, size_t at, int cmd_size, int count) {
  for (int i = 0; i < count && at < b.size(); i++) {
    const size_t n = record_size(b, at, cmd_size);
    if (n == 0) return false;
    at += n;
  }
  return true;
}

}  // namespace

bool udp_tv_recording_open(UdpTvRecording& d, std::vector<uint8_t> bytes, bool loop,
                           std::string& error) {
  d = UdpTvRecording{};
  d.loop = loop;
  client::Demo demo;
  if (!client::open_demo(std::move(bytes), demo, error)) return false;
  if (demo.state.format != client::DemoFormat::client) {
    error = "the file is not a multiplayer client's recording";
    return false;
  }
  d.bytes = std::move(demo.bytes);
  d.first = d.at = demo.pos;
  d.cmd_size = walks(d.bytes, d.first, 24, 64) ? 24 : 28;
  if (!walks(d.bytes, d.first, d.cmd_size, 64)) {
    error = "the recording's first records do not read: the file is damaged";
    return false;
  }
  if (d.first + 5 <= d.bytes.size()) d.first_time = f32_at(d.bytes, d.first);
  return true;
}

void udp_tv_recording_update(UdpTvRecording& d, ClassicTvUdp& face, int64_t now_us) {
  if (d.start_us < 0) d.start_us = now_us;
  for (int k = 0; k < 4096 && !d.ended; k++) {
    const size_t n = record_size(d.bytes, d.at, d.cmd_size);
    if (n == 0) {
      if (d.growing) return;  // the next records are still to come
      if (!d.loop || d.packets == 0) {
        d.ended = true;
        return;
      }
      d.at = d.first;
      d.skipped = 0;
      d.rounds++;
      d.start_us = now_us + kRoundGapUs;  // a pause between rounds, however short the recording
      face.count_raise = static_cast<int32_t>(d.rounds);
      continue;
    }
    const double t = static_cast<double>(f32_at(d.bytes, d.at));
    if (d.at == d.first) d.last_time = t;
    if (d.cut && t - d.last_time > kMostGapS)
      d.skipped += t - d.last_time - kMostGapS;  // dead air cut
    d.last_time = t;
    const double due = t - static_cast<double>(d.first_time) - d.skipped;
    if (due > static_cast<double>(now_us - d.start_us) / 1e6) return;
    const uint8_t type = d.bytes[d.at + 4];
    if (type == 0) {
      const size_t a = d.at + 5 + static_cast<size_t>(d.cmd_size);
      udp_tv_view(face, d.stream,
                  {f32_at(d.bytes, a), f32_at(d.bytes, a + 4), f32_at(d.bytes, a + 8)});
    } else if (type == 1) {
      const std::span<const uint8_t> packet(d.bytes.data() + d.at + 9, n - 9);
      d.packets++;
      if (!udp_tv_packet(face, d.stream, packet)) {
        d.ended = true;  // the face's log says why
        return;
      }
    }
    d.at += n;
  }
}

// THE MULTIVIEW RECORDING'S VIEWS.

namespace {

// The newest record's time in a view's converted bytes so far (after `from`), or `none`.
double newest_time(const UdpTvRecording& p, double none) {
  double t = none;
  size_t at = p.at;
  for (size_t n; (n = record_size(p.bytes, at, p.cmd_size)) != 0; at += n)
    t = static_cast<double>(f32_at(p.bytes, at));
  return t;
}

// The view's conversion started: from the start of the source with no view written (`slot` >= 0)
// or the lowest slot playing (-1).
void begin_view(UdpTvMultiview::View& v, int slot, int stream, int64_t start_us, float t0) {
  v = UdpTvMultiview::View{};
  v.slot = slot;
  v.on = true;
  client::ConvertOptions o;
  o.no_end = true;
  o.pov = slot >= 0 ? -2 : -1;
  std::string error;
  client::convert_begin(v.convert, client::DemoFormat::multiview, client::DemoFormat::client, o,
                        error);
  v.play.loop = false;
  v.play.growing = true;
  v.play.cut = false;
  v.play.stream = stream;
  v.play.cmd_size = 24;  // the converter writes the oldest clients' command
  v.play.start_us = start_us;
  v.play.first_time = t0;
}

// More of the source converted into the view: false when its conversion failed.
bool feed(UdpTvMultiview& d, UdpTvMultiview::View& v, size_t upto) {
  upto = std::min(upto, d.source.size());
  if (v.fed < upto) {
    client::convert_feed(v.convert, std::span(d.source).subspan(v.fed, upto - v.fed));
    v.fed = upto;
  }
  if (v.fed == d.source.size() && !v.source_ended) {
    client::convert_end(v.convert);
    v.source_ended = true;
  }
  std::string error;
  if (!client::convert_take(v.convert, v.play.bytes, error)) {
    d.error = error;
    return false;
  }
  return true;
}

// The view converted until its newest record is `ahead_s` past the clock, or its source ends.
bool pump(UdpTvMultiview& d, UdpTvMultiview::View& v, double clock, bool mark) {
  for (int k = 0; k < 4096 && !v.source_ended; k++) {
    if (!v.play.bytes.empty() && newest_time(v.play, -1e9) >= clock + kAheadS) break;
    if (!feed(d, v, v.fed + kChunk)) return false;
    if (mark && !v.play.bytes.empty()) d.marks.emplace_back(v.fed, newest_time(v.play, clock));
  }
  return true;
}

// The stream whose view is `slot`'s now (its level come), or -1.
int stream_of(const UdpTvMultiview& d, const ClassicTvUdp& face, int slot) {
  for (size_t s = 0; s < d.views.size() && s < face.streams.size(); s++)
    if (d.views[s].on && face.streams[s].level && face.streams[s].pov == slot)
      return static_cast<int>(s);
  return -1;
}

// `slot`'s view turned on a free stream when none carries it, read to the match's moment: false
// when its conversion failed.
bool turn_on(UdpTvMultiview& d, ClassicTvUdp& face, int slot, double clock) {
  for (size_t s = 0; s < d.views.size(); s++) {
    const UdpTvMultiview::View& v = d.views[s];
    if (v.on && (v.slot == slot || (s == 0 && face.streams[0].pov == slot))) return true;
  }
  for (size_t s = 1; s < d.views.size(); s++) {
    if (d.views[s].on) continue;
    UdpTvMultiview::View& v = d.views[s];
    begin_view(v, slot, static_cast<int>(s), d.start_us, d.views[0].play.first_time);
    size_t upto = 0;
    for (const auto& [fed, t] : d.marks)
      if (t <= clock) upto = fed;
    if (!feed(d, v, upto)) return false;
    client::convert_set_pov(v.convert, slot);
    udp_tv_clear(face, static_cast<int>(s));  // its last player's view is gone
    d.started++;
    return true;
  }
  return true;  // every stream busy: the view waits for one to be let go
}

// The auto track's pick at `t` on the recording's timeline: his view turned on, and followed once
// it has a level. False when a conversion failed.
bool follow_pick(UdpTvMultiview& d, ClassicTvUdp& face, double t) {
  const int slot = client::autotrack_at(d.plan, t);
  if (slot < 0 || !face.streams[0].level) return true;  // the first stream's player not known yet
  const int s = stream_of(d, face, slot);
  if (s < 0) return turn_on(d, face, slot, static_cast<double>(d.views[0].play.first_time) + t);
  if (face.follow != s) {
    udp_tv_follow(face, s);
    if (slot != d.tracked) d.cuts++;
  }
  d.tracked = slot;
  return true;
}

}  // namespace

client::AutotrackSettings tv_autotrack_settings() {
  client::AutotrackSettings a;
  a.dwell_s = 8;
  a.gap_s = 4;
  return a;
}

bool udp_tv_multiview_open(UdpTvMultiview& d, std::vector<uint8_t> bytes, int streams, bool loop,
                           std::string& error, const client::AutotrackSettings* autotrack) {
  d = UdpTvMultiview{};
  d.loop = loop;
  client::Demo demo;
  if (!client::open_demo(std::move(bytes), demo, error)) return false;
  if (demo.state.format != client::DemoFormat::multiview) {
    error = "the file is not a multiview recording";
    return false;
  }
  if (autotrack != nullptr) {  // the hottest player's cuts, over the whole recording
    client::DemoScan scan;
    if (!client::scan_demo(demo, scan, error) ||
        !client::autotrack_plan(demo, scan, *autotrack, d.plan, error))
      return false;
    d.autotrack = true;
  }
  d.source.assign(demo.bytes.begin() + static_cast<std::ptrdiff_t>(demo.pos), demo.bytes.end());
  d.views.resize(static_cast<size_t>(std::max(1, streams)));
  return true;
}

void udp_tv_multiview_update(UdpTvMultiview& d, ClassicTvUdp& face, int64_t now_us) {
  if (d.ended || d.views.empty()) return;
  UdpTvMultiview::View& lead = d.views[0];
  if (d.start_us < 0 || !lead.on) {  // a round begins: the followed view from the start
    d.start_us = now_us + (d.rounds > 0 ? kRoundGapUs : 0);
    d.marks.clear();
    begin_view(lead, -1, 0, d.start_us, 0);
    if (!pump(d, lead, 0, true) || lead.play.bytes.empty()) {
      d.ended = true;
      if (d.error.empty()) d.error = "the recording's first frames gave no view";
      return;
    }
    lead.play.first_time = f32_at(lead.play.bytes, 0);
    d.tracked = -1;  // the plan picks again from the start
    for (size_t s = 1; s < d.views.size(); s++)
      if (d.views[s].on) {  // a view watched last round goes on from the start
        const int slot = d.views[s].slot;
        begin_view(d.views[s], slot, static_cast<int>(s), d.start_us, lead.play.first_time);
        client::convert_set_pov(d.views[s].convert, slot);
      }
  }
  const float t0 = lead.play.first_time;
  const double clock = static_cast<double>(t0) + static_cast<double>(now_us - d.start_us) / 1e6;
  const double now_s = static_cast<double>(now_us) / 1e6;
  // The views wanted and not carried: each turned on a free stream, read to the match's moment.
  for (const int slot : udp_tv_wanted(face))
    if (!turn_on(d, face, slot, clock)) return;
  if (d.autotrack && !follow_pick(d, face, clock - static_cast<double>(t0))) return;
  for (size_t s = 0; s < d.views.size(); s++) {
    UdpTvMultiview::View& v = d.views[s];
    if (!v.on) continue;
    if (s > 0) {  // let go of a view nobody watches
      if (udp_tv_watching(face, static_cast<int>(s)) > 0 || v.idle_since < 0) v.idle_since = now_s;
      if (now_s - v.idle_since > kIdleS) {
        v = UdpTvMultiview::View{};
        udp_tv_clear(face, static_cast<int>(s));
        d.let_go++;
        continue;
      }
    }
    if (!pump(d, v, clock, s == 0)) {
      d.ended = true;
      return;
    }
    udp_tv_recording_update(v.play, face, now_us);
  }
  // The followed view played out: the round is over.
  if (lead.source_ended && record_size(lead.play.bytes, lead.play.at, lead.play.cmd_size) == 0) {
    if (!d.loop) {
      d.ended = true;
      return;
    }
    d.rounds++;
    face.count_raise = static_cast<int32_t>(d.rounds);
    lead.on = false;
  }
}

}  // namespace shooters_edge::net
