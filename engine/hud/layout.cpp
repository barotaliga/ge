// The HUD as data, defined: the anchors' words, the table, the frame, the stacks and the two
// clearance rules, each kind's draw, the fade, and the cvars bound into the places.
//
// ge:summary The HUD layout defined: elements placed and drawn, their cvars and hud_reset
// ge:keywords hud layout draw stack under-corners fade anchor-name anchor-from add_hud_cvars
// ge:keywords hud_reset cvars bind
// ge:skill ui console
// ge:test tests/hud_layout_test.cpp
#include "hud/layout.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

#include "core/num_text.hpp"

namespace ge::hud {

namespace {

constexpr std::array<std::string_view, 9> kAnchorNames = {"top_left",    "top",    "top_right",
                                                          "left",        "center", "right",
                                                          "bottom_left", "bottom", "bottom_right"};

bool meets(const ui::Rect& a, const ui::Rect& b) {
  return a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
}

// The nine stacks of a frame's draw and the boxes each has taken.
struct Stacks {
  const Frame* f;
  float gap;
  std::array<std::optional<Stack>, 9> stack;
  std::array<std::vector<ui::Rect>, 9> boxes;

  // A box of `w` x `h` at `a`, the next in its stack. `under_corners` decides the stack's drop
  // when it is the first box at a middle anchor of the top or bottom row.
  ui::Rect next(Anchor a, float w, float h, bool under_corners) {
    const auto i = static_cast<size_t>(a);
    if (!stack[i]) {
      float drop = 0;
      const size_t row = i / 3;
      if (under_corners && i % 3 == 1 && row != 1) {
        const ui::Rect first = place(*f, a, w, h);
        const size_t l = row * 3, r = row * 3 + 2;
        bool met = false;
        for (const size_t c : {l, r})
          for (const ui::Rect& b : boxes[c]) met |= meets(first, b);
        if (met) {
          const float used =
              std::max(stack[l] ? stack[l]->used() : 0.0f, stack[r] ? stack[r]->used() : 0.0f);
          drop = (used + gap) * (row == 0 ? 1.0f : -1.0f);
        }
      }
      stack[i].emplace(*f, a, gap, 0.0f, drop);
    }
    Stack& s = *stack[i];  // NOLINT(bugprone-unchecked-optional-access): emplaced above
    const ui::Rect r = s.next(w, h);
    boxes[i].push_back(r);
    return r;
  }
};

ui::Rect moved(ui::Rect r, float dx, float dy) {
  r.x += dx;
  r.y += dy;
  return r;
}

}  // namespace

std::string_view anchor_name(Anchor a) { return kAnchorNames[static_cast<size_t>(a)]; }

std::optional<Anchor> anchor_from(std::string_view word) {
  for (size_t i = 0; i < kAnchorNames.size(); ++i)
    if (kAnchorNames[i] == word) return static_cast<Anchor>(i);
  return std::nullopt;
}

int Layout::add(const ElementSpec& spec) {
  Element& e = elements_.emplace_back();
  e.id = std::string(spec.id);
  e.help = std::string(spec.help);
  e.parent = std::string(spec.parent);
  e.spec = spec;
  e.spec.id = e.id;
  e.spec.help = e.help;
  e.spec.parent = e.parent;
  e.under = spec.under.empty() ? -1 : find(spec.under);
  e.spec.under = {};
  e.place = {
      spec.show, std::string(anchor_name(spec.anchor)), spec.x, spec.y, spec.scale, spec.alpha,
      e.parent,  std::string(anchor_name(spec.attach))};
  return static_cast<int>(elements_.size() - 1);
}

int Layout::find(std::string_view id) const {
  for (size_t i = 0; i < elements_.size(); ++i)
    if (elements_[i].id == id) return static_cast<int>(i);
  return -1;
}

Frame Layout::frame(int width, int height, const Style& style, Insets insets) const {
  Style s = style;
  s.margin = static_cast<float>(margin);
  Frame f = frame_for(width, height, s, insets);
  if (scale != 1) f.scale *= static_cast<float>(scale);
  return f;
}

namespace {

// The point `a` of `r`.
ui::Vec2 point_of(const ui::Rect& r, Anchor a) {
  const int col = static_cast<int>(a) % 3, row = static_cast<int>(a) / 3;
  return {r.x + r.w * 0.5f * static_cast<float>(col), r.y + r.h * 0.5f * static_cast<float>(row)};
}

Strip strip_of(const Element& e, const Style& st) {
  const Content& c = e.content;
  return Strip{&c.cells, c.density, c.face != nullptr ? *c.face : st.value_face, st.shadow};
}

bool picture_kind(Kind k) { return k == Kind::picture || k == Kind::digits || k == Kind::strip; }

// The picture kinds' size at one screen px per picture pixel.
Size natural(const Element& e, const Style& st) {
  const Content& c = e.content;
  switch (e.spec.kind) {
    case Kind::picture:
      return measure(Picture{c.image, c.color, c.density}, 1);
    case Kind::digits:
      return measure(Digits{c.sheet, c.value, e.spec.digit_cells, c.alt, c.color}, 1);
    case Kind::strip:
      return measure(e.spec.strip, strip_of(e, st), 1);
    default:
      return {0, 0};
  }
}

}  // namespace

std::vector<Placed> Layout::draw(ui::DrawList& dl, const Frame& f, const Style& st) {
  std::vector<Placed> placed;
  Stacks stacks{&f, f.px(st.gap), {}, {}};
  for (size_t ei = 0; ei < elements_.size(); ++ei) {
    Element& e = elements_[ei];
    e.shown = false;
    const Content& c = e.content;
    if (!c.on || !e.place.show || (e.spec.kind == Kind::feed && c.feed == nullptr)) continue;
    const Anchor a = anchor_from(e.place.anchor).value_or(e.spec.anchor);
    // A parent declared before this element and shown this frame; else it stands alone.
    const Element* parent = nullptr;
    if (!e.place.parent.empty()) {
      const int pi = find(e.place.parent);
      if (pi >= 0 && static_cast<size_t>(pi) < ei && elements_[static_cast<size_t>(pi)].shown)
        parent = &elements_[static_cast<size_t>(pi)];
    }
    Frame fe = f;
    fe.scale = (parent ? parent->drawn_scale : f.scale) * static_cast<float>(e.place.scale);
    const float dx = parent ? static_cast<float>(e.place.x) * parent->drawn_unit
                            : static_cast<float>(e.place.x) * f.safe.w;
    const float dy = parent ? static_cast<float>(e.place.y) * parent->drawn_unit
                            : static_cast<float>(e.place.y) * f.safe.h;
    // The picture kinds' unit: screen px per picture pixel, fitted and snapped.
    float u = fe.scale * e.spec.unit;
    if (picture_kind(e.spec.kind)) {
      const Size n = natural(e, st);
      if (e.spec.fit > 0 && n.w * u > e.spec.fit * f.safe.w && n.w > 0)
        u = e.spec.fit * f.safe.w / n.w;
      if (e.spec.snap && u >= 1) u = static_cast<float>(static_cast<int>(u));
      fe.scale = u / e.spec.unit;
    }
    const ui::Align align = align_of(a);
    const size_t mark = dl.size();
    const bool corners = e.spec.under_corners;
    ui::Rect box;
    auto slot = [&](float w, float h) {
      ui::Rect r;
      if (parent != nullptr) {
        const Anchor at = anchor_from(e.place.attach).value_or(e.spec.attach);
        const ui::Vec2 p = point_of(parent->drawn, a);
        const ui::Vec2 o = point_of({0, 0, w, h}, at);
        r = {p.x - o.x + dx, p.y - o.y + dy, w, h};
      } else {
        r = moved(stacks.next(a, w, h, corners), dx, dy);
      }
      if (e.under >= 0) {
        const Element& un = elements_[static_cast<size_t>(e.under)];
        if (un.shown) r.y = std::max(r.y, un.drawn.bottom() + stacks.gap);
      }
      return r;
    };
    switch (e.spec.kind) {
      case Kind::readout: {
        const Readout r{c.value, c.caption, c.color};
        const Size z = measure(fe, st, r);
        box = slot(z.w, z.h);
        hud::draw(dl, fe, st, r, box, align);
        break;
      }
      case Kind::big: {
        const Big b{c.value, c.unit, c.caption, c.color};
        const Size z = measure(fe, st, b);
        box = slot(z.w, z.h);
        hud::draw(dl, fe, st, b, box, align);
        break;
      }
      case Kind::bar: {
        Meter m = e.spec.bar;
        m.value = c.level;
        m.fill = c.color;
        const Size mz = measure(fe, m);
        const ui::Rect rb = slot(mz.w, mz.h);
        hud::draw(dl, fe, st, m, rb);
        box = rb;
        if (!c.caption.empty()) {
          const Readout said{"", c.caption, c.color};
          const Size sz = measure(fe, st, said);
          const ui::Rect rs = slot(std::max(sz.w, mz.w), sz.h);
          hud::draw(dl, fe, st, said, rs, align);
          box.h = rs.bottom() - rb.y;
        }
        break;
      }
      case Kind::pips: {
        Pips p = e.spec.pips;
        p.count = c.count;
        p.lit = c.lit;
        p.blink_off = c.blink_off;
        p.color = c.color;
        const Size z = measure(fe, p);
        box = slot(z.w, z.h);
        hud::draw(dl, fe, st, p, box);
        break;
      }
      case Kind::banner: {
        const Banner b{c.value, c.caption, c.color};
        const Size z = measure(fe, st, b);
        box = slot(z.w, z.h);
        hud::draw(dl, fe, st, b, box);
        break;
      }
      case Kind::feed: {
        const float w = std::min(e.spec.feed_w, (fe.safe.w / fe.scale) * e.spec.feed_share);
        box = draw_feed(dl, fe, st, *c.feed, c.t, a, e.spec.feed_size, w, dx, dy);
        break;
      }
      case Kind::picture: {
        const Picture p{c.image, c.color, c.density};
        const Size z = measure(p, u);
        box = slot(z.w, z.h);
        hud::draw(dl, p, box, u);
        break;
      }
      case Kind::digits: {
        const Digits d{c.sheet, c.value, e.spec.digit_cells, c.alt, c.color};
        const Size z = measure(d, u);
        box = slot(z.w, z.h);
        hud::draw(dl, d, box, u);
        break;
      }
      case Kind::strip: {
        const Strip sp = strip_of(e, st);
        const Size z = measure(e.spec.strip, sp, u);
        box = slot(z.w, z.h);
        hud::draw(dl, e.spec.strip, sp, box, u);
        break;
      }
      case Kind::ring: {
        const Ring r{c.level, c.color, c.over, st.track};
        const Size z = measure(fe, e.spec.ring);
        box = slot(z.w, z.h);
        hud::draw(dl, fe, e.spec.ring, r, box);
        break;
      }
    }
    const float fade = static_cast<float>(e.place.alpha * alpha);
    if (fade != 1) dl.fade(mark, fade);
    e.drawn = box;
    e.drawn_scale = fe.scale;
    e.drawn_unit = picture_kind(e.spec.kind) ? u : fe.scale;
    e.shown = true;
    placed.push_back({e.id, box});
  }
  return placed;
}

void add_hud_cvars(console::Console& con, Layout& layout) {
  using console::CvarKind;
  console::Cvars& cv = con.cvars();
  const auto text = [](double v) { return core::num::to_string(v); };
  // Each element's cvars by its id, then the whole HUD's under "": what hud_reset puts back.
  auto owned = std::make_shared<std::vector<std::pair<std::string, std::string>>>();
  for (size_t i = 0; i < layout.size(); ++i) {
    Element& e = layout[i];
    const std::string p = "hud_" + e.id + "_";
    const std::string what = e.help.empty() ? e.id : e.help;
    const std::string names[8] = {p + "show",  p + "anchor", p + "x",      p + "y",
                                  p + "scale", p + "alpha",  p + "parent", p + "attach"};
    const bool child = !e.parent.empty();
    const std::string helps[8] = {
        "show " + what + " (0 hides it)",
        "where " + what +
            " hangs: top_left top top_right left center right bottom_left bottom "
            "bottom_right",
        child ? what + "'s offset across, in its parent's units"
              : what + "'s offset across, a share of the safe area's width",
        child ? what + "'s offset down, in its parent's units"
              : what + "'s offset down, a share of the safe area's height",
        what + "'s size, times the HUD's",
        what + "'s opacity, 0..1",
        "the element " + what + " hangs from (empty: none), one declared before it",
        "the point of " + what + "'s own box set on its parent's anchor point"};
    const std::string fallbacks[8] = {e.spec.show ? "1" : "0",
                                      std::string(anchor_name(e.spec.anchor)),
                                      text(e.spec.x),
                                      text(e.spec.y),
                                      text(e.spec.scale),
                                      text(e.spec.alpha),
                                      e.parent,
                                      std::string(anchor_name(e.spec.attach))};
    // A child's offsets are in its parent's units (picture px), a top-level one's shares.
    const double reach = child ? 4096 : 1;
    cv.bind(
        {.name = names[0], .fallback = fallbacks[0], .help = helps[0], .kind = CvarKind::boolean},
        &e.place.show);
    cv.bind({.name = names[1], .fallback = fallbacks[1], .help = helps[1], .kind = CvarKind::text},
            &e.place.anchor);
    cv.bind(
        {.name = names[2], .fallback = fallbacks[2], .help = helps[2], .min = -reach, .max = reach},
        &e.place.x);
    cv.bind(
        {.name = names[3], .fallback = fallbacks[3], .help = helps[3], .min = -reach, .max = reach},
        &e.place.y);
    cv.bind({.name = names[4], .fallback = fallbacks[4], .help = helps[4], .min = 0.25, .max = 4},
            &e.place.scale);
    cv.bind({.name = names[5], .fallback = fallbacks[5], .help = helps[5], .min = 0, .max = 1},
            &e.place.alpha);
    cv.bind({.name = names[6], .fallback = fallbacks[6], .help = helps[6], .kind = CvarKind::text},
            &e.place.parent);
    cv.bind({.name = names[7], .fallback = fallbacks[7], .help = helps[7], .kind = CvarKind::text},
            &e.place.attach);
    for (const std::string& n : names) owned->emplace_back(e.id, n);
  }
  cv.bind({.name = "hud_scale",
           .fallback = "1",
           .help = "the whole HUD's size, times the frame's",
           .min = 0.25,
           .max = 4},
          &layout.scale);
  cv.bind({.name = "hud_alpha",
           .fallback = "1",
           .help = "the whole HUD's opacity, 0..1",
           .min = 0,
           .max = 1},
          &layout.alpha);
  const std::string margin = text(layout.margin);
  cv.bind({.name = "hud_margin",
           .fallback = margin,
           .help = "px at the reference size between the screen's safe edge and the HUD",
           .min = 0,
           .max = 200},
          &layout.margin);
  for (const char* n : {"hud_scale", "hud_alpha", "hud_margin"}) owned->emplace_back("", n);
  con.add_command("hud_reset",
                  "hud_reset [<id>]: an element (or the whole HUD) back to the game's place",
                  [&layout, owned](console::Console& c, const console::Args& a) {
                    const bool one = a.size() > 1;
                    if (one && layout.find(a[1]) < 0) {
                      c.error("hud_reset: no HUD element \"" + a[1] + "\"",
                              "type cvarlist hud_ to see the elements' cvars");
                      return;
                    }
                    for (const auto& [id, name] : *owned) {
                      if (one && id != a[1]) continue;
                      console::Cvar* v = c.cvars().find(name);
                      if (v == nullptr) continue;
                      v->flags &= ~uint32_t{console::kArchive};
                      c.cvars().reset(*v, console::Source::code);
                    }
                  });
}

}  // namespace ge::hud
