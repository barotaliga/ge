// THE SKY FIX-UP (plugins/brush/app/bsp_draw.hpp): every pixel a sky face covered (bsp.wgsl's
// mask) set back to the cleared depth, so the sky pass and the clouds draw there; and THE VEIL
// after them: those pixels' colour times the blend constant (the map's sky light and tint), so a
// map's sky can be as dim as its own was against its lit rooms.
//
// ge:summary The BSP renderer's sky fix-up: sky faces' pixels back to the cleared depth
// ge:keywords bsp brush render sky-mask fix-up frag-depth veil sky-light
// ge:skill brush render

@group(0) @binding(0) var sky_mask: texture_2d<f32>;

struct FixOut {
  @builtin(position) clip: vec4f,
};

@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> FixOut {
  var o: FixOut;
  let xy = vec2f(f32((i << 1u) & 2u), f32(i & 2u));
  o.clip = vec4f(xy * 2.0 - 1.0, 0.0, 1.0);
  return o;
}

@fragment
fn fs_main(in: FixOut) -> @builtin(frag_depth) f32 {
  if (textureLoad(sky_mask, vec2i(in.clip.xy), 0).r < 0.5) {
    discard;
  }
  return 0.0;
}

@fragment
fn fs_veil(in: FixOut) -> @location(0) vec4f {
  if (textureLoad(sky_mask, vec2i(in.clip.xy), 0).r < 0.5) {
    discard;
  }
  return vec4f(1.0);
}
