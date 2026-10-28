---
title: The eye's meter reads the frame at the sky's exposure, never at its own, or it chases itself
date: 2026-10-28
scope: engine/render/eye.hpp, engine/render/eye.wgsl, engine/render/eye_adapt.wgsl, engine/render/wgsl/frame.wgsl
concepts: [eye-adaptation, auto-exposure, histogram, feedback, sky-light, wgsl-embed, reserved-word]
---

An adapted exposure metered off the picture it brightens is a feedback loop: each frame's
reading moves with the last frame's answer, and the eye hunts. The meter here exposes every tap
at `sky_exposure()` (the eye at rest), so its average is a property of the scene alone and the
step only smooths toward a fixed goal; `exposure()` is the only reader of `SkyLight.eye`. The
adapted state lives in the sky's light buffer because every pass already binds it read-only;
the step writes it from a module of its own (eye_adapt.wgsl, its own copy of the struct),
because a module that includes frame.wgsl already declares that binding read-only. Two traps
that cost a build each: an unbalanced `[` in a WGSL comment (`[low, high)`) breaks the CMake
list the embedder splits lines into and leaves the shader EMPTY with no error until the C++
compiles it; and `target` is a reserved word in WGSL, which a pipeline only reports as
`finish()` returning false. Run the resolved file under `build/<preset>/gen/` through
`.cache/deps/Dawn-*/bin/tint --format spirv` to see the message.
