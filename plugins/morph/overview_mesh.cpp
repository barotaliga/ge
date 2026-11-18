// A vertex-animated model as the overview draws it (overview_mesh.hpp).
//
// ge:summary One frame of a morph model in a skin as the overview's mesh; a group's frame at a time
// ge:keywords overview figure frame-group skin remap
// ge:skill morph map-design
#include "morph/overview_mesh.hpp"

#include <algorithm>
#include <vector>

namespace ge::morph {

ge::overview::Mesh overview_mesh(const Model& m, int frame, int skin, const SkinPaint& paint) {
  ge::overview::Mesh out;
  if (m.frames.empty()) return out;
  const Frame& f = m.frames[size_t(std::clamp(frame, 0, int(m.frames.size()) - 1))];
  out.pos = f.positions;
  out.idx = m.indices;
  if (!m.skins.empty() && paint.palette.size() >= 768) {
    const Skin& s = m.skins[size_t(std::clamp(skin, 0, int(m.skins.size()) - 1))];
    out.tw = s.width;
    out.th = s.height;
    out.tex.resize(size_t(s.width) * size_t(s.height) * 4);
    const bool remap = paint.remap.size() >= 256;
    for (size_t i = 0; i < s.indices.size(); ++i) {
      const size_t c = remap ? paint.remap[s.indices[i]] : s.indices[i];
      for (int q = 0; q < 3; ++q) out.tex[i * 4 + size_t(q)] = paint.palette[c * 3 + size_t(q)];
      out.tex[i * 4 + 3] = 255;
    }
  } else if (!paint.picture.empty()) {
    out.tw = paint.pw;
    out.th = paint.ph;
    out.tex.assign(paint.picture.begin(), paint.picture.end());
  }
  const size_t nv = m.vertex_count();
  out.st.resize(nv * 2);
  out.rect.resize(nv * 4);
  for (size_t i = 0; i < nv; ++i) {
    out.st[i * 2] = m.st[i * 2] * float(out.tw);
    out.st[i * 2 + 1] = m.st[i * 2 + 1] * float(out.th);
    out.rect[i * 4 + 2] = float(out.tw);
    out.rect[i * 4 + 3] = float(out.th);
  }
  return out;
}

int frame_at(const Model& m, int group, double time_s) {
  const std::vector<Frame>& fs = m.frames;
  int first = -1, count = 0;
  double total = 0;
  for (size_t i = 0; i < fs.size(); ++i)
    if (fs[i].group == group) {
      if (first < 0) first = int(i);
      ++count;
      total += fs[i].interval;
    }
  if (first < 0) return 0;
  if (count == 1 || total <= 0) return first;
  double t = time_s - total * double(int64_t(time_s / total));
  for (int i = 0; i < count; ++i) {
    t -= fs[size_t(first) + size_t(i)].interval;
    if (t < 0) return first + i;
  }
  return first + count - 1;
}

}  // namespace ge::morph
