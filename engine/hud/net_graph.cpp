// The net graph (net_graph.hpp).
//
// ge:summary The net graph's record of packets, its words and its bars
// ge:keywords net_arrived net_lost draw_net_graph r_netgraph
// ge:skill ui
// ge:test tests/hud_net_graph_test.cpp
#include "hud/net_graph.hpp"

#include <algorithm>

#include "core/num_text.hpp"

namespace ge::hud {

namespace {

void put(NetHistory& h, float ms, PacketMark mark) {
  const uint32_t slot = h.count % kNetPackets;
  h.ms[slot] = ms;
  h.mark[slot] = mark;
  h.count++;
}

int kept(const NetHistory& h) { return static_cast<int>(std::min<uint32_t>(h.count, kNetPackets)); }

int slot_of(const NetHistory& h, int i) {
  return static_cast<int>((h.count - 1 - static_cast<uint32_t>(i)) % kNetPackets);
}

}  // namespace

void net_arrived(NetHistory& h, double round_trip_ms, bool stale) {
  put(h, round_trip_ms < 0 ? -1.0f : static_cast<float>(round_trip_ms),
      stale ? PacketMark::stale : PacketMark::arrived);
}

void net_lost(NetHistory& h, uint32_t n) {
  for (uint32_t i = 0; i < std::min<uint32_t>(n, kNetPackets); i++) put(h, 0, PacketMark::lost);
}

PacketMark net_mark(const NetHistory& h, int i) {
  if (i < 0 || i >= kept(h)) return PacketMark::none;
  return h.mark[slot_of(h, i)];
}

float net_round_trip(const NetHistory& h, int i) {
  if (i < 0 || i >= kept(h)) return 0;
  return h.ms[slot_of(h, i)];
}

double net_ping_ms(const NetHistory& h) {
  double sum = 0;
  int came = 0;
  for (int i = 0; i < kept(h); i++) {
    const PacketMark m = net_mark(h, i);
    if ((m == PacketMark::arrived || m == PacketMark::stale) && net_round_trip(h, i) >= 0) {
      sum += net_round_trip(h, i);
      came++;
    }
  }
  return came > 0 ? sum / came : 0;
}

int net_loss_percent(const NetHistory& h) {
  const int n = kept(h);
  if (n == 0) return 0;
  int lost = 0;
  for (int i = 0; i < n; i++) lost += net_mark(h, i) == PacketMark::lost ? 1 : 0;
  return (100 * lost + n / 2) / n;
}

std::string net_words(const NetHistory& h) {
  bool measured = false;
  for (int i = 0; i < kept(h) && !measured; i++)
    measured = net_mark(h, i) != PacketMark::lost && net_round_trip(h, i) >= 0;
  const std::string ping = measured ? core::num::to_fixed(net_ping_ms(h), 0) : "--";
  return "PING " + ping + "  LOSS " + std::to_string(net_loss_percent(h)) + "%";
}

float draw_net_graph(ui::DrawList& dl, const Frame& frame, const Style& style, const NetHistory& h,
                     int mode, float top) {
  if (mode != 1) return 0;
  const std::string words = net_words(h);
  const float size = frame.px(style.caption_size);
  const float pad = frame.px(4);
  const float gap = frame.px(4);
  const float bar = std::max(1.0f, frame.px(2.5f));
  const float graph_w = bar * kNetPackets;
  const float graph_h = frame.px(50);
  const float w = std::max(text_width(style.caption_face, size, words, 1), graph_w) + 2 * pad;
  const float h_box = size * 1.3f + graph_h + 3 * pad;
  const float y = top > 0 ? top + gap : 0;
  const ui::Rect box = place(frame, Anchor::top_right, w, h_box, 0, y);
  dl.rect(box, style.plate, frame.px(3));
  draw_shadowed(dl, style.caption_face, size, box.right() - pad, box.y + pad + size, words,
                style.ink, style.shadow, frame.px(style.shadow_drop) * 0.5f, ui::Align::right, 1);
  const float base = box.bottom() - pad;
  const float right = box.right() - pad;
  const float per_ms = graph_h / kNetGraphMs;
  const ui::Color late = ui::hex(0xf2c94c);  // a frame no longer held: the picture waited
  for (int i = 0; i < kNetPackets; i++) {
    const PacketMark m = net_mark(h, i);
    if (m == PacketMark::none) break;
    const float x = right - bar * static_cast<float>(i + 1);
    const float tall = m == PacketMark::lost ? graph_h
                                             : std::clamp(net_round_trip(h, i) * per_ms,
                                                          std::max(1.0f, frame.px(1)), graph_h);
    const bool unknown = m != PacketMark::lost && net_round_trip(h, i) < 0;
    const ui::Color c = m == PacketMark::lost    ? style.bad
                        : unknown                ? style.track
                        : m == PacketMark::stale ? late
                                                 : style.good;
    dl.rect({x, base - tall, std::max(1.0f, bar - 1), tall}, c);
  }
  return h_box;
}

}  // namespace ge::hud
