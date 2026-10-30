// THE VIEW UNDER WATER (render/underwater.hpp): the opaque frame, copied before the water's
// passes, redrawn as an eye under the surface sees it: the water's fog by its beam
// attenuation into its own lit veil (what stands under the surface lit by the day dimmed on its
// way down), the eye's adaptation to the dark, the blur by distance, Snell's window and the mirror past
// it where the view meets the surface first, the wobble and a stylised tint; the specks
// suspended in the water; and, at the waterline, the split where the lens plate crosses the
// surface, blended over the frame by the share under it.
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
  lit: vec4f,      // the room's share of the light, the ripple's slope, the mirror, the caustics
}

@group(1) @binding(0) var<uniform> under: Under;
@group(1) @binding(1) var scene_colour: texture_2d<f32>;
@group(1) @binding(2) var scene_depth: texture_2d<f32>;  // R32F: the opaque depth

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

// Where a direction from the eye lands on the screen (uv), and whether it is in front.
fn screen_of(dir: vec3f) -> vec3f {
  let c = frame.view_proj * vec4f(dir * 100.0, 1.0);
  if (c.w <= 1e-4) {
    return vec3f(0.0, 0.0, 0.0);
  }
  let ndc = c.xy / c.w;
  return vec3f(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5, 1.0);
}

// The surface's ripple at a point of the plan: its height's slope (dh/dx, dh/dz), three short
// crossing swells drifting at their own speeds, `amp` the steepest.
fn ripple_slope(p: vec2f, t: f32, amp: f32) -> vec2f {
  let d1 = vec2f(0.8, 0.6);
  let d2 = vec2f(-0.45, 0.9);
  let d3 = vec2f(0.95, -0.3);
  let s = d1 * cos(dot(p, d1) * 5.1 + t * 1.7) + d2 * cos(dot(p, d2) * 7.3 - t * 2.1) * 0.7 +
          d3 * cos(dot(p, d3) * 11.0 + t * 2.9) * 0.4;
  return s * amp / 2.1;
}

// The caustics' net at a point of the plan, 0..about 2: the rippled surface focuses the light
// into bright thin lines round dimmer cells, as on a pool's floor; the lines are the cells'
// edges (the gap between the nearest two of points that wander in a grid), two scales, drifting.
fn caustic_cells(p: vec2f, t: f32, k: i32) -> f32 {
  let base = floor(p);
  var f1 = 9.0;
  var f2 = 9.0;
  for (var j = -1; j <= 1; j++) {
    for (var i = -1; i <= 1; i++) {
      let c = base + vec2f(f32(i), f32(j));
      let h = hash4(vec3i(vec2i(c), 7), k);
      let q = c + 0.5 + 0.38 * vec2f(sin(t * (0.6 + h.z) + h.x * 6.28),
                                     cos(t * (0.5 + h.w) + h.y * 6.28));
      let d = length(p - q);
      if (d < f1) {
        f2 = f1;
        f1 = d;
      } else if (d < f2) {
        f2 = d;
      }
    }
  }
  return pow(1.0 - smoothstep(0.0, 0.16, f2 - f1), 2.0);
}

// The net laid on a face by its normal's share of each plane (a wall takes it upright).
fn caustic_on(p: vec3f, n: vec3f, t: f32) -> f32 {
  let w = n / max(n.x + n.y + n.z, 1e-4);
  return w.y * caustic_net(p.xz, t) + w.z * caustic_net(p.xy + 3.3, t) +
         w.x * caustic_net(p.zy + 7.1, t);
}

fn caustic_net(p: vec2f, t: f32) -> f32 {
  // The lines wobble with the swells, then the two scales cross.
  let w = p + 0.12 * vec2f(sin(p.y * 2.3 + t * 1.1), cos(p.x * 2.1 - t * 0.9));
  return caustic_cells(w * 1.3, t * 0.8, 1) + 0.6 * caustic_cells(w * 2.6 + 4.7, t, 2);
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
  // here in uniform control flow): the caustics are laid on it from its three planes.
  let p0 = frame.eye.xyz + ray0 * min(scene_distance(uv0), 1e3);
  let face_n = abs(normalize(cross(dpdx(p0), dpdy(p0)) + vec3f(0.0, 1e-6, 0.0)));

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
  // Half the water's hue: a pool's colour reads in what it does to the room, not as a dye.
  let hue = mix(vec3f(1.0), under.veil.rgb / max(dot(under.veil.rgb, luma_w), 1e-4), 0.5);
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
  // The surface first: the world above through Snell's window, the water mirrored past it.
  var through = false;  // the ray left the water through the surface
  if (ray.y > 1e-4) {
    let to_surface = eye_depth / ray.y;
    if (to_surface < dist) {
      path = to_surface;
      if (under.look.w > 0.5) {
        // The surface where the ray meets it, tilted by its ripple.
        let hit = frame.eye.xz + ray.xz * to_surface;
        let sl = ripple_slope(hit, t, under.lit.y);
        let n = normalize(vec3f(-sl.x, 1.0, -sl.y));
        let cos_i = dot(ray, n);
        let window = smoothstep(0.645, 0.675, cos_i);  // cos 48.6 degrees, softened
        let f = 1.0 - cos_i;
        let fresnel = 0.02 + 0.98 * f * f * f * f * f;
        // Through the window: the world above, bent by the refraction where the surface is
        // rippled (a flat surface keeps the frame's own picture).
        var above = seen;
        if (under.lit.y > 0.0) {
          // Only the ripple's part of the bend: the flat surface's own (the window's fisheye)
          // would reach past the frame's edges, where the copy has nothing.
          let flat_n = vec3f(0.0, 1.0, 0.0);
          let bent = screen_of(refract(ray, -n, 1.333));
          let level_bent = screen_of(refract(ray, -flat_n, 1.333));
          let ok = select(0.0, 1.0, bent.z > 0.5 && level_bent.z > 0.5);
          let at = clamp(uv + (bent.xy - level_bent.xy) * ok, vec2f(0.0), vec2f(1.0));
          above = blurred(at, radius * 0.5);
          // The ceiling's caustics: the light the rippled water throws up on what stands over
          // it, fading a few metres up.
          if (under.lit.w > 0.0) {
            let d_above = scene_distance(at);
            let p_above = frame.eye.xyz + normalize(view_ray(frame, at)) * min(d_above, 1e3);
            let rise = max(p_above.y - under.info.x, 0.0);
            above *= 1.0 + under.lit.w * 1.4 * caustic_net(p_above.xz * 0.9, t) * exp(-rise * 0.3);
          }
        }
        // Past the window, the water below mirrored by total internal reflection.
        var mirrored = veil;
        if (under.lit.z > 0.0) {
          let r = reflect(ray, n);
          let s_r = screen_of(r);
          if (s_r.z > 0.5 && all(s_r.xy > vec2f(0.0)) && all(s_r.xy < vec2f(1.0))) {
            let d_r = scene_distance(s_r.xy);
            let tr_r = exp(-under.c.rgb * under.fog.x * min(d_r, 1e3));
            let below = blurred(s_r.xy, radius);
            mirrored = mix(veil, below * tr_r + veil * (1.0 - tr_r), under.lit.z);
          }
        }
        seen = mix(mirrored, above * (1.0 - fresnel), window);
        through = window > 0.5;
      }
    }
  }
  // What stands under the surface was lit by the day dimmed on its way down.
  let point_depth = under.info.x - (frame.eye.y + ray.y * min(path, 1e4));
  seen *= exp(-under.kd.rgb * max(point_depth, 0.0));
  // The caustics on what stands under the surface, fading with its depth.
  if (under.lit.w > 0.0 && !through && point_depth > 0.0 && dist < 1e5) {
    let p = frame.eye.xyz + ray * path;
    seen *= 1.0 + under.lit.w * 1.4 * caustic_on(p * 0.8, face_n, t) *
                      exp(-point_depth * 0.25);
  }
  let tr = exp(-under.c.rgb * under.fog.x * path);
  var colour = seen * tr + veil * (1.0 - tr);
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
