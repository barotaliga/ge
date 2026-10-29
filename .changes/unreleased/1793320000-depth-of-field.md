---
type: Added
title: Depth of field
---

Depth of field for any game: `render/dof.hpp`'s half-size `dof` gather round a focus the game names (a tap behind a texel held to the texel's own circle, a texel in focus skipping it) and its mix in the post pass behind `kDof`, by each pixel's own blur circle off the full-size depth (`wgsl/dof_coc.wgsl`); `post_group` takes `PostReads` (formerly `PostMotion`) and `set_post_focus` sets the focus live. Fall Line's `r_dof` (off by default) and `r_dof_focus` (0: the camera's subject); the `world` lab's run moments name the chase camera's subject.
