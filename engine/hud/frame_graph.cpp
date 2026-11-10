// The frame rate and its graph (frame_graph.hpp).
//
// ge:summary The frame rate's words and bars
// ge:keywords draw_frame_rate fps
// ge:skill ui
#include "hud/frame_graph.hpp"

#include <algorithm>
#include <string>

#include "core/num_text.hpp"

namespace ge::hud {

ui::Rect draw_frame_rate(ui::DrawList& dl, const Frame& frame, const Style& style,
                         const loop::FrameTimes& times, double refresh_hz, int mode) {
  if (mode <= 0) return {};
  // The rate over the last half second of frames (at least one), so the number is readable.
  double span = 0;
  size_t n = 0;
  for (; n < times.size() && span < 0.5; n++) span += times.recent(n);
  const double fps = span > 0 ? static_cast<double>(n) / span : 0;
  const double last_ms = times.size() > 0 ? times.recent(0) * 1e3 : 0;
  const std::string words =
      core::num::to_fixed(fps, 0) + " FPS  " + core::num::to_fixed(last_ms, 1) + " MS";
  const float size = frame.px(style.caption_size);
  const float pad = frame.px(4);
  const float bar = std::max(1.0f, frame.px(1.5f));
  const float graph_w = mode >= 2 ? bar * kGraphFrames : 0;
  const float graph_h = mode >= 2 ? frame.px(60) : 0;
  const float text_w = text_width(style.caption_face, size, words, 1);
  const float w = std::max(text_w, graph_w) + 2 * pad;
  const float h = size * 1.3f + (mode >= 2 ? graph_h + pad : 0) + 2 * pad;
  const ui::Rect box = place(frame, Anchor::top_right, w, h);
  dl.rect(box, style.plate, frame.px(3));
  draw_shadowed(dl, style.caption_face, size, box.right() - pad, box.y + pad + size, words,
                style.ink, style.shadow, frame.px(style.shadow_drop) * 0.5f, ui::Align::right, 1);
  if (mode < 2) return box;
  const float base = box.bottom() - pad;
  const float left = box.right() - pad - graph_w;
  const float per_ms = graph_h / kGraphMs;
  const double refresh_ms = refresh_hz > 0 ? 1e3 / refresh_hz : 0;
  const size_t bars = std::min<size_t>(times.size(), kGraphFrames);
  for (size_t i = 0; i < bars; i++) {
    const double ms = times.recent(i) * 1e3;
    const float tall = std::min(graph_h, static_cast<float>(ms) * per_ms);
    const bool late = refresh_ms > 0 ? ms > 1.5 * refresh_ms : false;
    const float x = left + graph_w - bar * static_cast<float>(i + 1);
    dl.rect({x, base - tall, bar, tall}, late ? style.bad : style.good);
  }
  if (refresh_ms > 0) {  // the refresh's interval, dashed
    const float y = base - std::min(graph_h, static_cast<float>(refresh_ms) * per_ms);
    for (int i = 0; i < kGraphFrames; i += 4) {
      const float x = left + bar * static_cast<float>(i);
      dl.line({x, y}, {std::min(x + 2 * bar, left + graph_w), y}, std::max(1.0f, frame.px(1)),
              style.ink);
    }
  }
  return box;
}

}  // namespace ge::hud
