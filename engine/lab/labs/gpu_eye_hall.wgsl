// THE EYE LAB'S HALL (gpu_eye_lab.cpp): a stone corridor drawn over the frame by tracing each
// pixel's view ray against its box from inside. The corridor's +z end wall has a door; a ray
// through the door keeps what the frame drew outside (the ground and the sky). The walls are lit
// by the day through the door (the outdoors' luminance over the door's solid angle from each
// point, the small-source form) and by one warm lamp, so the inside is the tens to hundreds of
// times darker than the outdoors a real hall is. Nothing is drawn once the eye is out of it.
#include "render/wgsl/frame.wgsl"

struct Hall {
  lo: vec4f,    // xyz: the inside's low corner (world, m)
  hi: vec4f,    // xyz: the high corner; the door is in the z = hi.z wall
  door: vec4f,  // x the door's half width, y its height, z the stone's albedo, w the outdoors' share
  lamp: vec4f,  // xyz: the lamp (world, m); w its intensity, the sun's units a steradian
}
@group(1) @binding(0) var<uniform> hall: Hall;

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

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
  let eye = frame.eye.xyz;
  if (any(eye < hall.lo.xyz) || any(eye > hall.hi.xyz)) {
    discard;
  }
  let dir = view_ray(frame, in.position.xy * frame.size.zw);
  // The face the ray leaves the box through.
  let far = select(hall.lo.xyz, hall.hi.xyz, dir > vec3f(0.0));
  let t3 = (far - eye) / select(vec3f(-1e-6), dir, abs(dir) > vec3f(1e-6));
  var t = 1e9;
  var n = vec3f(0.0);
  for (var k = 0; k < 3; k++) {
    if (t3[k] > 0.0 && t3[k] < t) {
      t = t3[k];
      n = vec3f(0.0);
      n[k] = -sign(dir[k]);
    }
  }
  let p = eye + dir * t;
  let mid = 0.5 * (hall.lo.x + hall.hi.x);
  if (n.z < -0.5 && abs(p.x - mid) < hall.door.x && p.y < hall.lo.y + hall.door.y) {
    discard;  // through the door: the outdoors as the frame drew it
  }
  // The day through the door: its luminance over the door's solid angle, cosine at both ends.
  let outside = hall.door.w * white_luminance();
  let door_c = vec3f(mid, hall.lo.y + 0.5 * hall.door.y, hall.hi.z);
  let to_door = door_c - p;
  let d2 = max(dot(to_door, to_door), 0.25);
  let l = to_door * inverseSqrt(d2);
  let area = 2.0 * hall.door.x * hall.door.y;
  let cos_wall = max(dot(n, l), 0.0);
  let cos_door = max(-l.z, 0.0);
  var e = outside * area * cos_wall * cos_door / d2;
  // The lamp, and the light bounced round (a quarter of the door's at the door, less deeper in).
  let to_lamp = hall.lamp.xyz - p;
  let ld2 = max(dot(to_lamp, to_lamp), 0.04);
  e += hall.lamp.w * max(dot(n, to_lamp * inverseSqrt(ld2)), 0.0) / ld2;
  e += 0.25 * outside * area / (d2 + area);
  // The lamp's own glow, a small bright disc where it is seen.
  let along = dot(to_lamp, dir);
  let off = length(to_lamp - dir * along);
  var c = vec3f(1.0, 0.92, 0.85) * hall.door.z / PI * e;
  if (along > 0.0 && along < t && off < 0.08) {
    c = vec3f(1.0, 0.7, 0.4) * hall.lamp.w * 15.0;
  }
  return vec4f(c, 1.0);
}
