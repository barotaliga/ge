// THE LIGHTNING'S LIGHT (render/view.hpp's `Flashes`, the frame's `flash_info` and `flashes`;
// docs/storms.md § The storm drawn). A flash is a lamp spread along its channel: its light at a
// distance r is its flux over 4 pi r^2, the channel's extent softening the near field (r^2 plus a
// quarter of its reach squared), and dimmed by the cloud the light diffuses through. Light
// scattered on and on through a cloud of optical depth tau leaves it as a diffusion does, falling
// as one over (1 + 0.15 tau) rather than exponentially, and spreads through the cloud only so far
// before it leaks out of the cloud's faces (`flash_spread`): why a flash in a storm lights the
// tower round itself and a patch of its base (sheet lightning), not the whole sky.
//
// - `flash_light(p, sigma)`: the light (lux, the frame's units) at a point, from every side, in a
//   medium of extinction `sigma` (1/m): inside the cloud the depth is sigma times the way to the
//   lamp (no more than its reach, where the light is spread from, nor than the lamp's depth out
//   of its cloud, the most its light crosses); outside it (sigma 0) that depth out. The cloud's
//   march adds it to its per-step light.
// - `flash_on(p, n)`: the light on a face of normal `n` (the ground, a wall): the lamp's from
//   its direction, a flash in the cloud's from the base over the face as much as from the lamp.
// - `flash_air(d, far)`: the clear air's glow along the eye's ray `d` out to `far` m, the line
//   integral of the light the air scatters (its extinction and the fog's) toward the eye, in
//   closed form: beta P / (16 pi^2 b) times the angle the ray's span takes at the lamp.
//
// ge:summary The lightning's light at a point, on a face and in the air along a ray
// ge:keywords lightning flash flash-light flash_light flash_on flash_air sheet-lightning diffusion flash-spread
// ge:skill render water

fn flash_count() -> i32 {
  return i32(frame.flash_info.x);
}

// The share of a lamp's light left after `tau` of cloud.
fn flash_through(tau: f32) -> f32 {
  return 1.0 / (1.0 + 0.15 * tau);
}

// How far a flash's light spreads through its cloud before it leaks out of the cloud's faces, m.
// Light diffusing through a deck or a tower some kilometres deep escapes through its top, its base
// and its sides, so it falls off across the cloud as exp(-r / spread) with a spread near the
// cloud's depth over pi: in night photographs of a strike the cloud glows round where the channel
// enters it, a kilometre or two across, and stays dark beyond.
const kFlashSpread = 1200.0;

// The share of a lamp's light still in the cloud `r` m from its channel.
fn flash_spread(r: f32, reach: f32) -> f32 {
  return exp(-max(r - 0.5 * reach, 0.0) / kFlashSpread);
}

fn flash_light(p: vec3f, sigma: f32) -> vec3f {
  var sum = vec3f(0.0);
  for (var k = 0; k < flash_count(); k++) {
    let at = frame.flashes[2 * k];
    let lum = frame.flashes[2 * k + 1];
    let r = length(p - at.xyz);
    let tau = select(lum.w, min(sigma * min(r, at.w), lum.w), sigma > 0.0);
    sum += lum.rgb * flash_through(tau) * flash_spread(r, at.w) /
           (4.0 * PI * (r * r + 0.25 * at.w * at.w));
  }
  return sum;
}

fn flash_on(p: vec3f, n: vec3f) -> vec3f {
  var sum = vec3f(0.0);
  for (var k = 0; k < flash_count(); k++) {
    let at = frame.flashes[2 * k];
    let lum = frame.flashes[2 * k + 1];
    let to = at.xyz - p;
    let r = length(to);
    let facing = mix(max(dot(n, to / max(r, 1.0)), 0.0), 0.5 + 0.5 * n.y, min(lum.w / 10.0, 1.0));
    sum += lum.rgb * facing * flash_through(lum.w) / (4.0 * PI * (r * r + 0.25 * at.w * at.w));
  }
  return sum;
}

// The clear air's extinction, 1/m (its Rayleigh scattering at sea level in the green).
const kFlashAir = 1.2e-5;

fn flash_air(d: vec3f, far: f32) -> vec3f {
  var sum = vec3f(0.0);
  let beta = kFlashAir + frame.weather.z;
  for (var k = 0; k < flash_count(); k++) {
    let at = frame.flashes[2 * k];
    let lum = frame.flashes[2 * k + 1];
    let o = at.xyz - frame.eye.xyz;
    let tc = dot(o, d);
    let b = sqrt(max(dot(o, o) - tc * tc, 0.0) + 0.25 * at.w * at.w);
    let angle = atan((far - tc) / b) + atan(tc / b);
    sum += lum.rgb * flash_through(lum.w) * beta * angle / (16.0 * PI * PI * b);
  }
  return sum;
}
