// THE TAA-U RESOLVE (render/taa.hpp): this frame's jittered render-size samples and the
// reprojected history into a new history at the output size.
#include "render/wgsl/frame.wgsl"

struct TaaParams {
  output: vec4f,  // width, height, 1/width, 1/height
  blend: vec4f,   // the new frame's share (1: no history), the clip box's gamma
}

@group(1) @binding(0) var<uniform> taa: TaaParams;
@group(1) @binding(1) var hdr: texture_2d<f32>;
@group(1) @binding(2) var motion: texture_2d<f32>;
@group(1) @binding(3) var depth: texture_depth_2d;
@group(1) @binding(4) var history_in: texture_2d<f32>;
@group(1) @binding(5) var history_out: texture_storage_2d<rgba16float, write>;

fn to_ycocg(c: vec3f) -> vec3f {
  return vec3f(0.25 * c.r + 0.5 * c.g + 0.25 * c.b, 0.5 * c.r - 0.5 * c.b,
               -0.25 * c.r + 0.5 * c.g - 0.25 * c.b);
}

fn from_ycocg(c: vec3f) -> vec3f {
  return vec3f(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z);
}

// The weight that keeps a lone bright sample from flickering: the inverse of its brightness.
fn tone_weight(c: vec3f) -> f32 {
  return 1.0 / (1.0 + dot(c, vec3f(0.2126, 0.7152, 0.0722)));
}

// The history at `uv`, Catmull-Rom from five bilinear taps (the corners' weights dropped).
fn history_at(uv: vec2f) -> vec3f {
  let pos = uv * taa.output.xy;
  let c = floor(pos - 0.5) + 0.5;
  let f = pos - c;
  let w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
  let w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
  let w2 = f * (0.5 + f * (2.0 - 1.5 * f));
  let w3 = f * f * (-0.5 + 0.5 * f);
  let w12 = w1 + w2;
  let t0 = (c - 1.0) * taa.output.zw;
  let t12 = (c + w2 / w12) * taa.output.zw;
  let t3 = (c + 2.0) * taa.output.zw;
  var sum = vec3f(0.0);
  var wsum = 0.0;
  let taps = array<vec3f, 5>(
    vec3f(t12.x, t0.y, w12.x * w0.y), vec3f(t0.x, t12.y, w0.x * w12.y),
    vec3f(t12.x, t12.y, w12.x * w12.y), vec3f(t3.x, t12.y, w3.x * w12.y),
    vec3f(t12.x, t3.y, w12.x * w3.y));
  for (var i = 0; i < 5; i++) {
    sum += textureSampleLevel(history_in, linear_clamp, taps[i].xy, 0.0).rgb * taps[i].z;
    wsum += taps[i].z;
  }
  return max(sum / wsum, vec3f(0.0));
}

// `h` pulled toward the box's centre until it lies inside the box.
fn clip_box(h: vec3f, lo: vec3f, hi: vec3f) -> vec3f {
  let centre = 0.5 * (lo + hi);
  let extent = 0.5 * (hi - lo) + vec3f(1e-5);
  let v = h - centre;
  let a = abs(v / extent);
  let m = max(a.x, max(a.y, a.z));
  return select(h, centre + v / m, m > 1.0);
}

@compute @workgroup_size(8, 8)
fn resolve(@builtin(global_invocation_id) id: vec3u) {
  if (f32(id.x) >= taa.output.x || f32(id.y) >= taa.output.y) {
    return;
  }
  let uv = (vec2f(id.xy) + 0.5) * taa.output.zw;
  let render = vec2i(frame.size.xy);
  let rp = uv * frame.size.xy;  // the output pixel's centre in unjittered render pixels
  let j = frame.jitter.xy;
  // Render pixel i's sample stands at i + 0.5 - j: the nearest is floor(rp + j).
  let centre = vec2i(floor(rp + j));
  let ratio = taa.output.x * frame.size.z;  // output pixels a render pixel
  let e = exposure();
  var sum = vec3f(0.0);
  var wsum = 0.0;
  var m1 = vec3f(0.0);
  var m2 = vec3f(0.0);
  var nearest = -1.0;
  var nearest_px = clamp(centre, vec2i(0), render - 1);
  for (var dy = -1; dy <= 1; dy++) {
    for (var dx = -1; dx <= 1; dx++) {
      let p = clamp(centre + vec2i(dx, dy), vec2i(0), render - 1);
      let c = textureLoad(hdr, p, 0).rgb * e;
      let y = to_ycocg(c);
      m1 += y;
      m2 += y * y;
      let d = (vec2f(p) + 0.5 - j - rp) * ratio;
      let w = exp(-2.29 * dot(d, d)) * tone_weight(c);
      sum += c * w;
      wsum += w;
      let z = textureLoad(depth, p, 0);
      if (z > nearest) {
        nearest = z;
        nearest_px = p;
      }
    }
  }
  let current = sum / max(wsum, 1e-8);
  let m = textureLoad(motion, nearest_px, 0);
  let reactive = textureLoad(motion, clamp(centre, vec2i(0), render - 1), 0).b;
  let prev_uv = uv - m.xy;
  var alpha = max(taa.blend.x, reactive);
  if (any(prev_uv < vec2f(0.0)) || any(prev_uv > vec2f(1.0))) {
    alpha = 1.0;
  }
  var out = current;
  if (alpha < 1.0) {
    let mean = m1 / 9.0;
    let sigma = sqrt(max(m2 / 9.0 - mean * mean, vec3f(0.0)));
    let g = taa.blend.y;
    let h = from_ycocg(clip_box(to_ycocg(history_at(prev_uv) * e), mean - g * sigma,
                                mean + g * sigma));
    let wh = (1.0 - alpha) * tone_weight(h);
    let wc = alpha * tone_weight(current);
    out = (h * wh + current * wc) / (wh + wc);
  }
  textureStore(history_out, id.xy, vec4f(out / e, 1.0));
}
