// THE VIEW UNDER WATER (render/underwater.hpp): the opaque frame, copied before the water's
// passes, redrawn as an eye under the surface sees it: the water's fog by its beam
// attenuation into its own lit veil (what stands under the surface lit by the day dimmed on its
// way down), the eye's adaptation to the dark, the blur by distance, Snell's window and the mirror past
// it where the view meets the surface first (the water pass's own surface from below, "water.below",
// fogged over the way to it), the wobble and a stylised tint; the specks suspended in the water;
// and, at the waterline, the split where the lens plate crosses the surface, blended over the frame
// by the share under it.
#include "render/wgsl/frame.wgsl"

struct Under {
  info: vec4f,  // the level (world m), the eye's depth (m), the blur (px), its range (m)
  c: vec4f,     // beam attenuation, rgb (1/m)
  kd: vec4f,    // daylight's diffuse attenuation, rgb (1/m)
  veil: vec4f,  // the veil's colour at the surface (white = a white face in daylight)
  look: vec4f,  // magnify, warp (of the height), warp rate (Hz), Snell's window on (1)
  tint: vec4f,  // rgb, mix
  fog: vec4f,   // the fog's scale, the eye's adaptation (the veil's luma it lifts to; 0 off)
  surface: vec4f,  // the surface at the eye (m), its slope dx, dz, 1 when the eye is under it
  motes: vec4f,    // the specks' share of cells, their radius (m), the lens plate's height (m)
  lit: vec4f,      // the room's share of the light, the caustics, the shafts, the tops
  tops: array<vec4f, 4>,  // the tops' boxes (x0, z0, x1, z1), the eye's body's first
  top_level: vec4f,       // their levels
}

@group(1) @binding(0) var<uniform> under: Under;
@group(1) @binding(1) var scene_colour: texture_2d<f32>;
@group(1) @binding(2) var scene_depth: texture_2d<f32>;  // R32F: the opaque depth
// The surface seen from below (render/water.hpp's "water.below"): its light and distance (m; 0
// none), and the share of it the veil fills.
@group(1) @binding(3) var below: texture_2d<f32>;
@group(1) @binding(4) var below_veil: texture_2d<f32>;
// The body the eye is in, its waves: the caustics and the shafts read the real surface.
@group(1) @binding(5) var<uniform> body: WaterBody;
@group(1) @binding(6) var<storage, read> waves: array<Wave>;

#include "render/wgsl/water_body.wgsl"

struct VertexOut {
  @builtin(position) position: vec4f,
}

@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> VertexOut {
  let p = vec2f(f32((i << 1u) & 2u), f32(i & 2u)) * 2.0 - 1.0;
  var out: VertexOut;
  out.position = vec4f(p, 0.0, 1.0);
  return out;
}

// The distance to the opaque scene through a uv, m (far where only the sky was drawn).
fn scene_distance(uv: vec2f) -> f32 {
  let size = vec2i(frame.size.xy);
  let pixel = clamp(vec2i(uv * frame.size.xy), vec2i(0), size - vec2i(1));
  let z = textureLoad(scene_depth, pixel, 0).r;
  if (z <= 0.0) {
    return 1e6;
  }
  let ndc = vec2f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
  let p = frame.inv_view_proj * vec4f(ndc, z, 1.0);
  return length(p.xyz / p.w);
}

// The surface over a point of the plan (render/underwater.hpp's `level_over`): the level of the
// top whose box holds it, else the tops' levels weighed by the inverse square of the distance to
// each box; with one top, the surface at the eye.
fn level_over(xz: vec2f) -> f32 {
  let n = u32(under.lit.w);
  if (n <= 1u) {
    return under.info.x;
  }
  var sum = 0.0;
  var weights = 0.0;
  for (var i = 0u; i < n; i++) {
    let b = under.tops[i];
    let d = max(max(b.xy - xz, xz - b.zw), vec2f(0.0));
    let w = 1.0 / (dot(d, d) + 1e-4);
    sum += w * under.top_level[i];
    weights += w;
  }
  return sum / weights;
}

// Eight taps round a disc of `radius` px and the centre.
fn blurred(uv: vec2f, radius: f32) -> vec3f {
  var sum = textureSampleLevel(scene_colour, linear_clamp, uv, 0.0).rgb;
  if (radius < 0.5) {
    return sum;
  }
  let px = frame.size.zw * radius;
  for (var i = 0; i < 8; i++) {
    let a = f32(i) * 0.785398 + 0.3;
    let r = select(1.0, 0.55, (i & 1) == 1);
    sum += textureSampleLevel(scene_colour, linear_clamp, uv + vec2f(cos(a), sin(a)) * r * px,
                              0.0).rgb;
  }
  return sum / 9.0;
}

// The surface from below round `uv` (its light, the veil filling its share `veil_down`), blurred as
// `blurred` blurs the scene, `radius` px, from the taps that hold the surface: the eye's own blur
// softens the window's rim and the mirror, never bleeding them into what stands in front.
fn surface_blurred(uv: vec2f, radius: f32, veil_down: vec3f) -> vec3f {
  let size = vec2i(frame.size.xy);
  let px = frame.size.zw * radius;
  var sum = vec3f(0.0);
  var n = 0.0;
  for (var i = -1; i < 8; i++) {
    var at_uv = uv;
    if (i >= 0) {
      let a = f32(i) * 0.785398 + 0.3;
      at_uv += vec2f(cos(a), sin(a)) * select(1.0, 0.55, (i & 1) == 1) * px;
    }
    let at = clamp(vec2i(at_uv * frame.size.xy), vec2i(0), size - vec2i(1));
    let b = textureLoad(below, at, 0);
    if (b.a > 0.0) {
      sum += b.rgb + veil_down * textureLoad(below_veil, at, 0).r;
      n += 1.0;
    }
    if (radius < 0.5) {
      break;
    }
  }
  return sum / max(n, 1.0);
}

// A cell's hash, 0..1, four of them (a PCG-style mix of the cell's integer coordinates).
fn hash4(c: vec3i, k: i32) -> vec4f {
  var v = vec4u(bitcast<vec3u>(c + vec3i(k * 7919, k * 104729, k * 15485863)), 0x9e3779b9u);
  v = v * 1664525u + 1013904223u;
  v.x += v.y * v.w;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v.w += v.y * v.z;
  v ^= v >> vec4u(16u);
  v.x += v.y * v.w;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v.w += v.y * v.z;
  return vec4f(v >> vec4u(8u)) / 16777216.0;
}

// The specks a ray passes near, found on six shells round the eye (0.1 to 1.6 m, never past the
// scene): on each, the cell the ray meets holds a speck with the share's chance, jittered in
// it and drifting; it shows as a soft dot, faded by how far its nearest approach lies off the
// shell so it comes and goes smoothly as the eye moves. Returns the light they scatter, each
// fogged over its own distance.
fn specks(origin: vec3f, ray: vec3f, dist: f32, focal: f32, t: f32) -> vec3f {
  var light = vec3f(0.0);
  for (var k = 0; k < 6; k++) {
    let d = 0.1 * exp2(f32(k) * 0.8);
    if (d > dist) {
      break;
    }
    let cell = 0.03 * exp2(f32(k) * 0.5);
    let q = origin + ray * d;
    let c = vec3i(floor(q / cell));
    let h = hash4(c, k);
    if (h.w >= under.motes.x) {
      continue;
    }
    // Drifting a little round its place, never out of its cell (whose neighbours a pixel
    // does not look in, so a speck leaving its cell would be cut).
    let drift = vec3f(sin(t * 0.31 + h.x * 6.28), sin(t * 0.23 + h.y * 6.28),
                      cos(t * 0.27 + h.z * 6.28)) * 0.08 * cell;
    let centre = (vec3f(c) + 0.25 + 0.5 * h.xyz) * cell + drift;
    let along = dot(centre - origin, ray);
    let off = length(centre - origin - ray * along);
    // Never under a pixel and a half wide, so a far speck stays a speck rather than flickering.
    let r = max(under.motes.y, 1.5 * along / focal);
    let near = 1.0 - smoothstep(0.0, cell, abs(along - d));
    let dot_ = exp(-off * off / (r * r));
    // A small speck catches as much light as its area; one smeared out by the pixel no more.
    light += near * dot_ * min(1.0, under.motes.y / r) * (0.6 + 0.8 * h.x) *
             exp(-under.c.rgb * under.fog.x * along);
  }
  return light;
}

// Light leaving water into air at `cos_i` to the normal (render/water.wgsl's `fresnel_exit`).
fn fresnel_exit(cos_i: f32) -> f32 {
  let eta = 1.333;
  let s2 = eta * eta * max(1.0 - cos_i * cos_i, 0.0);
  if (s2 >= 1.0) {
    return 1.0;
  }
  let ct = sqrt(1.0 - s2);
  let rs = (eta * cos_i - ct) / (eta * cos_i + ct);
  let rp = (eta * ct - cos_i) / (eta * ct + cos_i);
  return 0.5 * (rs * rs + rp * rp);
}

// THE LIGHT SHAFTS along the eye's ray out to `path` m (at most 24): sixteen steps (offset per
// pixel and frame, so the resolve smooths the steps away), each the sun's light the waves focus
// there over its mean, dimmed per band over the way down from the surface and the way back to the
// eye, off a net four times softer than the bed's (the light is scattered through the volume); times
// the forward scattering toward the eye. What they add over the veil (bright shafts and
// the darker water between them).
fn light_shafts(ray: vec3f, path: f32, pixel: vec2f, t: f32, px_angle: f32) -> vec3f {
  let down = sun_down();
  let reach = min(path, 24.0);
  let jitter = fract(52.9829189 * fract(dot(pixel, vec2f(0.06711056, 0.00583715))) +
                     0.618034 * frame.time.y);
  let step = reach / 16.0;
  var sum = vec3f(0.0);
  for (var k = 0; k < 16; k++) {
    let along = (f32(k) + jitter) * step;
    let p = frame.eye.xyz + ray * along;
    let depth = level_over(p.xz) - p.y;
    if (depth <= 0.0) {
      continue;
    }
    let focus = caustic_at(p.xz, depth, down, t, 4.0, along * px_angle) - 1.0;
    sum += focus * exp(-under.c.rgb * under.fog.x * (along + depth / max(-down.y, 0.2))) * step;
  }
  // Henyey-Greenstein, g 0.8: a water's particles scatter mostly forward.
  let g = 0.8;
  let c = -dot(down, ray);
  // Capped a little off the peak, which the sun's own disc in the window already stands for.
  let phase = min((1.0 - g * g) / (4.0 * PI * pow(1.0 + g * g - 2.0 * g * c, 1.5)), 1.2);
  let sun = sun_light(under.info.x, frame.eye.xz) * max(frame.sun.y, 0.0) * body.sun.x;
  // About a quarter of a clear water's attenuation is scattering, and of what it scatters into the
  // shafts' pattern (over the mean the veil carries) the eye takes about two thirds.
  return sun * 0.15 * under.c.rgb * phase * sum;
}

// The room's light round the eye: the mean of the scene seen at a grid over the screen.
fn room_light() -> vec3f {
  var sum = vec3f(0.0);
  for (var i = 0; i < 5; i++) {
    for (var j = 0; j < 4; j++) {
      let uv = vec2f((f32(i) + 0.5) / 5.0, (f32(j) + 0.5) / 4.0);
      sum += textureSampleLevel(scene_colour, linear_clamp, uv, 0.0).rgb;
    }
  }
  return sum / 20.0;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
  let t = frame.time.x;
  let uv0 = in.position.xy * frame.size.zw;
  let fwd = normalize(view_ray(frame, vec2f(0.5, 0.5)));
  let ray0 = normalize(view_ray(frame, uv0));
  let edge = view_ray(frame, vec2f(0.5, 0.0));
  let tan_half = length(edge / dot(edge, fwd) - fwd);
  let focal = 0.5 * frame.size.y / tan_half;
  // The face under this pixel's normal (from the scene's points' screen derivatives, taken
  // here in uniform control flow): a wall takes the sun's caustics at a slant.
  let p0 = frame.eye.xyz + ray0 * min(scene_distance(uv0), 1e3);
  let face_n = normalize(cross(dpdx(p0), dpdy(p0)) + vec3f(0.0, 1e-6, 0.0));

  // THE WATERLINE: this pixel's point on the lens plate against the surface there (the plane
  // through the surface at the eye, tilted by its slope, lapping a centimetre).
  let plate = under.motes.z;
  let lens = frame.eye.xyz + (ray0 / dot(ray0, fwd) - fwd) * (0.5 * plate / tan_half);
  let lap = 0.006 * sin(lens.x * 9.0 + t * 5.3) + 0.004 * sin(lens.z * 13.0 - t * 7.1);
  let level = under.surface.x + under.surface.y * (lens.x - frame.eye.x) +
              under.surface.z * (lens.z - frame.eye.z) + lap;
  var over = lens.y - level;  // m over the water (negative: the plate under it here)
  if (plate <= 0.0) {
    over = select(1.0, -1.0, under.surface.w > 0.5);
  }
  let px = max(fwidth(over), 1e-5);
  let wet = clamp(0.5 - over / px, 0.0, 1.0);
  let meniscus = 0.55 * exp(-over * over / (2.25 * px * px)) * select(0.0, 1.0, plate > 0.0);
  if (wet <= 0.0 && meniscus < 0.01) {
    return vec4f(0.0);
  }

  // The mask's magnification about the centre, then the slow wobble.
  var uv = (uv0 - 0.5) / under.look.x + 0.5;
  let w = 6.2831853 * under.look.z * t;
  uv += under.look.y * vec2f(sin(uv.y * 23.0 + w), cos(uv.x * 17.0 + 1.3 * w));
  uv = clamp(uv, vec2f(0.0), vec2f(1.0));
  let ray = normalize(view_ray(frame, uv));
  let dist = scene_distance(uv);

  // The light down here: the day's, dimmed over the water above the eye.
  let eye_depth = max(level - lens.y, 0.0);
  let sun = sun_light(frame.eye.y, frame.eye.xz);
  let day = sun * max(frame.sun.y, 0.0) + sky_irradiance(vec3f(0.0, 1.0, 0.0));
  // Under a lit room the light in the water is the room's, the window's world among it.
  // The water there glows about as bright as the room round it, in the water's own hue, so the
  // window is a few times brighter than the water, never a lamp in a black sea.
  let luma_w = vec3f(0.2126, 0.7152, 0.0722);
  // A third of the water's hue: a pool's colour reads in what it does to the room, not as a dye.
  let hue = mix(vec3f(1.0), under.veil.rgb / max(dot(under.veil.rgb, luma_w), 1e-4), 0.35);
  var room_veil = vec3f(0.0);
  if (under.lit.x > 0.0) {
    room_veil = hue * dot(room_light(), luma_w) * 0.7;
  }
  let veil0 = mix(under.veil.rgb * day / PI, room_veil, under.lit.x) *
              exp(-under.kd.rgb * eye_depth);
  // The light in the water comes from above: the veil is brighter looking up than down (the
  // radiance under a surface falls from the zenith to the nadir by about ten times; this is a
  // gentler three, round the veil the optics give for a level view).
  let veil = veil0 * mix(0.45, 1.5, smoothstep(-0.9, 0.9, ray.y));

  let radius = under.info.z * (1.0 - exp(-min(dist, 1e3) * 0.693 / max(under.info.w, 0.01)));
  var seen = blurred(uv, radius);
  var path = dist;
  // The surface first, where the eye's ray meets it before the scene: as the water pass drew it
  // from below, its share that mirrors nothing filled by the veil looking down.
  var through = false;
  if (under.look.w > 0.5) {
    // Its nearest pixel: a filtered read would blend a distance with the empty pixels round it.
    let at = clamp(vec2i(uv * frame.size.xy), vec2i(0), vec2i(frame.size.xy) - vec2i(1));
    let b = textureLoad(below, at, 0);
    let veil_down = veil0 * mix(0.45, 1.5, smoothstep(-0.9, 0.9, -ray.y));
    if (b.a > 0.0 && b.a < dist) {
      path = b.a;
      let r_s = under.info.z * (1.0 - exp(-min(path, 1e3) * 0.693 / max(under.info.w, 0.01)));
      seen = surface_blurred(uv, r_s, veil_down);
      through = true;
    } else if (b.a <= 0.0 && under.surface.w < 0.5 && ray.y > 1e-4 && eye_depth / ray.y < dist) {
      // No surface drawn (the eye over the level, the lens plate dipping under it): the level
      // surface, the world seen straight through it and the water mirrored past the window. Never
      // with the eye under: there every surface it may see is drawn, and a pixel none covers
      // looks at what stands under the water (a flooded passage's roof, not a level over it).
      path = eye_depth / ray.y;
      let refl = fresnel_exit(ray.y);
      seen = seen * (1.0 - refl) + veil_down * refl;
      through = true;
    }
  }
  // What stands under the surface was lit by the day dimmed on its way down.
  let point = frame.eye.xyz + ray * min(path, 1e4);
  let point_depth = level_over(point.xz) - point.y;
  seen *= exp(-under.kd.rgb * max(point_depth, 0.0));
  // THE CAUSTICS on what stands under the surface: the sun's share of its light gathered and
  // spread by the waves over it (water_body.wgsl's `caustic_at`), a wall's at its slant to the sun.
  if (under.lit.y > 0.0 && !through && point_depth > 0.0 && dist < 1e5 && body.sun.x > 0.0) {
    let p = frame.eye.xyz + ray * path;
    let down = sun_down();
    let slant = clamp(abs(dot(face_n, down)) / max(-down.y, 0.2), 0.0, 1.0);
    let share = sun_share(under.info.x, p.xz) * body.sun.x * slant * under.lit.y;
    let foot = path * 2.0 * tan_half / frame.size.y / max(abs(dot(face_n, ray)), 0.2);
    seen *= 1.0 + (caustic_at(p.xz, point_depth, down, t, 1.0, foot) - 1.0) * share;
  }
  let tr = exp(-under.c.rgb * under.fog.x * path);
  var colour = seen * tr + veil * (1.0 - tr);
  if (under.lit.z > 0.0 && body.sun.x > 0.0 && frame.sun.y > 0.0) {
    // The water between the shafts darker, never by more than a third.
    colour = max(colour + under.lit.z * light_shafts(ray, path, in.position.xy, t, 2.0 * tan_half / frame.size.y), 0.67 * colour);
  }
  // The specks, lit by the light in the water, each fogged over its own distance.
  if (under.motes.x > 0.0) {
    let s = specks(frame.eye.xyz, ray, path, focal, t);
    colour += 5.0 * veil0 * s;
  }
  // The eye adapts to the dark: the veil's luma lifted toward `fog.y`, at most six times.
  if (under.fog.y > 0.0) {
    // Under a lit room the eye adapts to the room's own light (a dim hall lifted, at most
    // three times), so the window never blows out over a black sea.
    let v = dot(under.veil.rgb * exp(-under.kd.rgb * eye_depth), luma_w);
    let open_lift = clamp(under.fog.y / max(v, 1e-4), 1.0, 6.0);
    let room_lift = clamp(1.5 * under.fog.y / max(dot(room_veil, luma_w), 1e-4), 1.0, 3.0);
    colour *= mix(open_lift, room_lift, under.lit.x);
  }
  let luma = dot(colour, vec3f(0.2126, 0.7152, 0.0722));
  colour = mix(colour, under.tint.rgb * luma, under.tint.w);
  // Premultiplied by the share under, with the meniscus a dark line over both sides.
  let a = wet * (1.0 - meniscus) + meniscus;
  return vec4f(colour * wet * (1.0 - meniscus) + veil0 * 0.15 * meniscus, a);
}
