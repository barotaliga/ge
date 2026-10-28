// THE 2D PASS (render/ui_pass.hpp): one quad an instance (a box, a glyph, a segment, an arc or a
// convex polygon), blended over the frame. Each shape's coverage is its signed distance at the
// pixel's centre, as the CPU raster (ui/raster.cpp) works it: a box's rounded rectangle in its
// own turned frame, its fill a paint (times an image, whole or nine-sliced) and its border a ring
// inside its edge; a segment's capsule; an arc's annular sector; a polygon's largest edge
// distance. A glyph reads its coverage from the atlas. Colours stay sRGB-encoded and leave
// premultiplied, so the blend (one, one minus source alpha) is the raster's straight "over".

struct Screen {
  size: vec4f,  // width, height, 1 / width, 1 / height (px)
}

// The fields by kind (render/ui_pass.hpp's UiItem says the same):
//   box:     rect x, y, w, h; uv the image's u, v at the corner and du/dx, dv/dy (sliced: u, v,
//            and the uv's width and height); ink the border; shape radius, feather, border,
//            opacity; turn cos, sin, pivot; src / dst the slice's source (uv) and box (px) insets
//   glyph:   rect the mask's place; uv its atlas x, y (texels); ink the colour
//   segment: rect a.xy, b.xy; ink the colour; shape half width, feather, -, opacity
//   arc:     rect centre, radius, half width; uv the ends' directions; turn the sweep's middle,
//            cos of half the sweep, round ends; src.xy the paint's frame
//   polygon: info.z, info.w its first point and count; src.xy the paint's frame
struct Item {
  rect: vec4f,
  clip: vec4f,   // x0, y0, x1, y1 (px)
  uv: vec4f,
  ink: vec4f,
  shape: vec4f,  // radius, feather, border, opacity
  info: vec4u,   // kind, paint index, a box: sliced / a polygon: first point, its count
  turn: vec4f,
  src: vec4f,
  dst: vec4f,
  bounds: vec4f,  // x0, y0, x1, y1: what the quad covers (px), feather included
}

struct Paint {
  line: vec4f,  // linear: from, to; radial: the centre
  form: vec4f,  // radius, aspect, kind (0 solid, 1 linear, 2 radial), stop count
  at: array<vec4f, 2>,
  colors: array<vec4f, 8>,
}

@group(0) @binding(0) var<uniform> screen: Screen;
@group(0) @binding(1) var<storage, read> items: array<Item>;
@group(0) @binding(2) var<storage, read> paints: array<Paint>;
@group(0) @binding(3) var atlas: texture_2d<f32>;
@group(0) @binding(4) var lin: sampler;
@group(0) @binding(5) var<storage, read> points: array<vec4f>;  // a polygon's: at, its edge's normal
@group(1) @binding(0) var image: texture_2d<f32>;

const kGlyph = 1u;
const kSampled = 2u;
const kSegment = 3u;
const kArc = 4u;
const kPolygon = 5u;

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) @interpolate(flat) item: u32,
}

@vertex
fn vs_main(@builtin(vertex_index) v: u32, @builtin(instance_index) i: u32) -> VertexOut {
  let it = items[i];
  // What the quad must cover: the shape's bounds (worked on the CPU), inside the clip.
  let lo = max(it.bounds.xy, it.clip.xy);
  let hi = max(min(it.bounds.zw, it.clip.zw), lo);
  let corner = array<vec2f, 6>(vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
                               vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0));
  let p = mix(lo, hi, corner[v]);
  var out: VertexOut;
  out.position = vec4f(p.x * screen.size.z * 2.0 - 1.0, 1.0 - p.y * screen.size.w * 2.0, 0.0, 1.0);
  out.item = i;
  return out;
}

// ui/raster.cpp's rounded_distance: from p to a box of half-size h, corner radius r, centred at
// the origin; negative inside.
fn rounded_distance(p: vec2f, h: vec2f, r: f32) -> f32 {
  let q = abs(p) - h + r;
  return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

fn coverage(d: f32, feather: f32) -> f32 {
  return clamp(0.5 - d / feather, 0.0, 1.0);
}

fn stop_at(p: Paint, k: u32) -> f32 {
  return p.at[k / 4u][k % 4u];
}

// ui/paint.cpp's lookup: the stops' colour at t.
fn lookup(p: Paint, t: f32) -> vec4f {
  let n = u32(p.form.w);
  if (n <= 1u || t <= stop_at(p, 0u)) {
    return p.colors[0];
  }
  for (var k = 1u; k < n; k++) {
    let b = stop_at(p, k);
    if (t <= b) {
      let a = stop_at(p, k - 1u);
      let span = b - a;
      if (span > 0.0) {
        return mix(p.colors[k - 1u], p.colors[k], (t - a) / span);
      }
      return p.colors[k];
    }
  }
  return p.colors[n - 1u];
}

// ui/paint.cpp's Paint::at, at (x, y) in the box's frame.
fn paint_at(p: Paint, x: vec2f) -> vec4f {
  let kind = u32(p.form.z);
  if (kind == 1u) {
    let d = p.line.zw - p.line.xy;
    let len2 = dot(d, d);
    var t = 0.0;
    if (len2 > 0.0) {
      t = dot(x - p.line.xy, d) / len2;
    }
    return lookup(p, t);
  }
  if (kind == 2u) {
    let d = vec2f(x.x - p.line.x, (x.y - p.line.y) / p.form.y);
    return lookup(p, length(d) / p.form.x);
  }
  return p.colors[0];
}

// ui/raster.cpp's segment_distance: from p to the segment a..b.
fn segment_distance(p: vec2f, a: vec2f, b: vec2f) -> f32 {
  let pa = p - a;
  let ba = b - a;
  let len2 = dot(ba, ba);
  var h = 0.0;
  if (len2 > 0.0) {
    h = clamp(dot(pa, ba) / len2, 0.0, 1.0);
  }
  return length(pa - ba * h);
}

// ui/raster.cpp's arc_distance: a ring's inside the sweep, its ends' outside it.
fn arc_distance(it: Item, p: vec2f) -> f32 {
  let c = it.rect.xy;
  let radius = it.rect.z;
  let half = it.rect.w;
  let d = p - c;
  let len = length(d);
  var inside = it.turn.z < -1.0;
  if (len > 0.0) {
    inside = dot(d, it.turn.xy) >= it.turn.z * len;
  }
  if (inside) {
    return abs(len - radius) - half;
  }
  let ef = it.uv.xy;
  let et = it.uv.zw;
  if (it.turn.w > 0.5) {
    return min(length(p - (c + ef * radius)), length(p - (c + et * radius))) - half;
  }
  let r0 = radius - half;
  let r1 = radius + half;
  return min(segment_distance(p, c + ef * r0, c + ef * r1), segment_distance(p, c + et * r0, c + et * r1));
}

// ui/raster.cpp's polygon_distance: the largest of the edges' signed distances.
fn polygon_distance(it: Item, p: vec2f) -> f32 {
  var d = -1e30;
  for (var k = 0u; k < it.info.w; k++) {
    let e = points[it.info.z + k];
    d = max(d, dot(p - e.xy, e.zw));
  }
  return d;
}

// ui/draw.cpp's slice_map: a nine-sliced box's source coordinate for `l` px across `size`.
fn slice_map(l: f32, size: f32, d0: f32, d1: f32, origin: f32, span: f32, s0: f32, s1: f32) -> f32 {
  if (l < d0) {
    var k = 0.0;
    if (d0 > 0.0) {
      k = s0 / d0;
    }
    return origin + l * k;
  }
  if (l > size - d1) {
    var k = 0.0;
    if (d1 > 0.0) {
      k = s1 / d1;
    }
    return origin + span - (size - l) * k;
  }
  let mid = size - d0 - d1;
  var t = 0.0;
  if (mid > 0.0) {
    t = (l - d0) * (span - s0 - s1) / mid;
  }
  return origin + s0 + t;
}

// A filled shape (a segment's colour, an arc's or polygon's paint) at coverage `cov`.
fn filled(c: vec4f, cov: f32, opacity: f32, gradient: bool, p: vec2f) -> vec4f {
  let a = c.a * cov * opacity;
  var rgb = c.rgb * a;
  if (gradient) {
    rgb += vec3f(dither(p) / 255.0 * a);
  }
  return vec4f(rgb, a);
}

// Interleaved gradient noise, -0.5 .. 0.5: half a level of dither on a gradient's fill.
fn dither(p: vec2f) -> f32 {
  return fract(52.9829189 * fract(dot(p, vec2f(0.06711056, 0.00583715)))) - 0.5;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
  let it = items[in.item];
  let p = in.position.xy;
  if (p.x < it.clip.x || p.x >= it.clip.z || p.y < it.clip.y || p.y >= it.clip.w) {
    discard;
  }
  let kind = it.info.x;
  if (kind == kGlyph) {
    let t = vec2i(it.uv.xy) + vec2i(floor(p - it.rect.xy));
    let a = it.ink.a * textureLoad(atlas, t, 0).r;
    return vec4f(it.ink.rgb * a, a);
  }
  let feather = max(it.shape.y, 0.5);
  if (kind == kSegment) {
    let cov = coverage(segment_distance(p, it.rect.xy, it.rect.zw) - it.shape.x, feather);
    if (cov <= 0.0) {
      discard;
    }
    return filled(it.ink, cov, it.shape.w, false, p);
  }
  if (kind == kArc || kind == kPolygon) {
    var d = 0.0;
    if (kind == kArc) {
      d = arc_distance(it, p);
    } else {
      d = polygon_distance(it, p);
    }
    let cov = coverage(d, feather);
    if (cov <= 0.0) {
      discard;
    }
    let paint = paints[it.info.y];
    return filled(paint_at(paint, p - it.src.xy), cov, it.shape.w, u32(paint.form.z) != 0u, p);
  }
  // A box, in its own (turned) frame.
  let dp = p - it.turn.zw;
  let local = it.turn.zw + vec2f(dp.x * it.turn.x + dp.y * it.turn.y,
                                 -dp.x * it.turn.y + dp.y * it.turn.x) - it.rect.xy;
  let half = it.rect.zw * 0.5;
  let r = clamp(it.shape.x, 0.0, min(half.x, half.y));
  let border = it.shape.z;
  let d = rounded_distance(local - half, half, r);
  let outer = coverage(d, feather);
  if (outer <= 0.0) {
    discard;
  }
  var inner = outer;
  if (border > 0.0) {
    inner = coverage(d + border, feather);
  }
  let paint = paints[it.info.y];
  var c = paint_at(paint, local);
  if (kind == kSampled) {
    var uv = it.uv.xy + local * it.uv.zw;
    if (it.info.z != 0u) {
      uv = vec2f(slice_map(local.x, it.rect.z, it.dst.x, it.dst.z, it.uv.x, it.uv.z, it.src.x, it.src.z),
                 slice_map(local.y, it.rect.w, it.dst.y, it.dst.w, it.uv.y, it.uv.w, it.src.y, it.src.w));
    }
    c *= textureSampleLevel(image, lin, uv, 0.0);
  }
  let fill = c.a * inner * it.shape.w;
  var ring = 0.0;
  if (border > 0.0) {
    ring = it.ink.a * (outer - inner) * it.shape.w;
  }
  // The fill, then the border over it: two "over"s folded into one premultiplied colour.
  var rgb = it.ink.rgb * ring + c.rgb * fill * (1.0 - ring);
  let alpha = fill + ring - fill * ring;
  if (u32(paint.form.z) != 0u) {
    rgb += vec3f(dither(p) / 255.0 * fill * (1.0 - ring));
  }
  return vec4f(rgb, alpha);
}
