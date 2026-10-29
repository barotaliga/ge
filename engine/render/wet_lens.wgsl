// THE WET LENS (render/wet_lens.hpp): the finished frame at the output's size, seen through the
// water on the glass: under the sheet's edge the view rippled and softened, the edge itself a
// bright wavy line; each drop a small lens of water showing the scene behind it turned over and
// out of focus, with a dark rim and the sky's highlight at its top.
#include "render/wgsl/frame.wgsl"

const kDrops = 96;

struct Lens {
  info : vec4f,  // the sheet's edge (0..1, 1: none), the time (s), drops, the drops' turn-over
         drops : array<vec4f, kDrops>,  // x, y (0..1), radius (screen heights), stretch
}

@group(1) @binding(0) var<uniform> lens : Lens;
@group(1) @binding(1) var scene : texture_2d<f32>;

struct VertexOut {
  @builtin(position) position : vec4f,
}

@vertex fn
vs_main(@builtin(vertex_index) i
        : u32) -> VertexOut {
  let p = vec2f(f32((i << 1u) & 2u), f32(i & 2u)) * 2.0 - 1.0;
  var out : VertexOut;
  out.position = vec4f(p, 0.0, 1.0);
  return out;
}

// Five taps round a disc of `r` (uv) and the centre.
fn soft(uv : vec2f, r : vec2f) -> vec3f {
  var sum = textureSampleLevel(scene, linear_clamp, uv, 0.0).rgb;
  for (var i = 0; i < 5; i++) {
    let a = f32(i) * 1.2566371 + 0.4;
    sum += textureSampleLevel(scene, linear_clamp,
                              clamp(uv + vec2f(cos(a), sin(a)) * r, vec2f(0.0), vec2f(1.0)), 0.0)
               .rgb;
  }
  return sum / 6.0;
}

@fragment fn fs_main(in : VertexOut) -> @location(0) vec4f {
  let size = vec2f(textureDimensions(scene));
  let uv = in.position.xy / size;
  let aspect = size.x / size.y;
  let t = lens.info.y;
  var colour = textureSampleLevel(scene, linear_clamp, uv, 0.0).rgb;

  // THE SHEET: below its edge (wavy itself) the view is rippled and soft; the edge is bright.
  let edge = lens.info.x + 0.012 * sin(uv.x * 37.0 + t * 3.0) + 0.008 * sin(uv.x * 91.0 - t * 5.0);
  if (lens.info.x < 1.0 && uv.y > edge) {
    let wobble = 0.006 * vec2f(sin(uv.y * 41.0 + uv.x * 13.0 - t * 9.0),
                               cos(uv.x * 29.0 + uv.y * 17.0 - t * 7.0));
    colour = soft(uv + wobble, vec2f(0.004 / aspect, 0.004)) * 0.92;
    let line = exp(-pow((uv.y - edge) / 0.006, 2.0));
    colour += line * 0.35 * (colour + vec3f(0.05));
  }

  // THE DROPS: the nearest one under this pixel wins.
  let n = i32(lens.info.z);
  for (var i = 0; i < n; i++) {
    let d = lens.drops[i];
    // In screen heights from the drop's centre; a sliding drop is drawn longer, its tail up.
    var p = vec2f((uv.x - d.x) * aspect, uv.y - d.y);
    let stretch = 1.0 + d.w * select(0.3, 1.0, p.y < 0.0);
    p.y /= stretch;
    let q = length(p) / d.z;
    if (q >= 1.0) {
      continue;
    }
    // A cap of water: its normal tips out toward the rim.
    let h = sqrt(1.0 - q * q);
    let normal = normalize(vec3f(p / d.z, h));
    // The scene behind turned over through the drop and out of focus, a little darker.
    let turned = vec2f(d.x, d.y) - vec2f(p.x / aspect, p.y) * lens.info.w;
    var seen = soft(clamp(turned, vec2f(0.0), vec2f(1.0)), vec2f(0.3 * d.z / aspect, 0.3 * d.z));
    let rim = smoothstep(0.55, 1.0, q);
    seen *= mix(0.95, 0.35, rim);
    let light = pow(max(dot(normal, normalize(vec3f(-0.35, -0.6, 0.72))), 0.0), 60.0);
    seen += vec3f(light) * 1.5 * (dot(colour, vec3f(0.2126, 0.7152, 0.0722)) + 0.2);
    let a = 1.0 - smoothstep(0.85, 1.0, q);
    colour = mix(colour, seen, a);
    break;
  }
  return vec4f(colour, 1.0);
}
