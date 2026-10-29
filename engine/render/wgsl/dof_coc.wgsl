// DEPTH OF FIELD'S BLUR CIRCLE (render/dof.hpp), shared by the dof pass and the post pass's mix:
// how blurred a surface `dist` m from the eye is, 0 (sharp) .. 1 (the widest circle), for a lens
// focused at `focus` m that keeps `sharp` of the way either side of it sharp. A real lens's
// circle grows with |1 - focus / dist|: nothing behind the focus is ever blurred past the far
// field's limit (1), and the near field passes it a little in front of the focus.

fn dof_circle(dist: f32, focus: f32, sharp: f32) -> f32 {
  let k = abs(1.0 - focus / max(dist, 1e-3));
  return clamp((k - sharp) / max(1.0 - sharp, 1e-3), 0.0, 1.0);
}

// The distance from the eye, m, of what is drawn at uv (0,0 top left) at device depth `z`
// (reversed-z; 0 where nothing was drawn: the sky, as far as the far field reaches).
fn dof_distance(inv_view_proj: mat4x4f, uv: vec2f, z: f32) -> f32 {
  if (z <= 0.0) {
    return 1e9;
  }
  let c = inv_view_proj * vec4f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, z, 1.0);
  return length(c.xyz / c.w);
}
