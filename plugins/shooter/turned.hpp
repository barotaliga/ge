// A BRUSH MODEL TURNED BY ITS ENTITY'S ANGLES: the one frame every game's turned brush is traced
// and drawn in. The model's own frame is the angles' forward, left (minus the view's right) and up
// (`angle_vectors`: pitch down positive, yaw counter-clockwise from +x, roll lowering the right
// side); a point (x, y, z) in it stands at origin + forward x + left y + up z in the world, the
// model's (0, 0, 0) at the entity's origin. A trace takes its line into that frame (`to_model`),
// sweeps the box there unchanged and turns the hit's normal back by the exact inverse
// (`to_world`). The picture turns a brush model in the same frame (Shooter's Edge's
// `models::rotation_of` with the pitch not reversed, held to it by tests/shooter_turned_test.cpp),
// so what is drawn is what is hit. All in 32-bit floats in the order written. An entity with every
// angle exactly 0 is not turned (`turned`): its model is traced as built, offset only.
//
// ge:summary A brush model turned by its entity's angles: the model's frame, the trace's turn
// ge:summary in and out, its reach
// ge:keywords turned-brush rotating-brush to_model to_world turned_reach rotate_object
// ge:keywords func_rotating func_door_rotating func_rotate_entity spinning-brush angles brush-model
// ge:skill shooter determinism
// ge:test tests/shooter_turned_test.cpp
#pragma once

#include "shooter/vec.hpp"

namespace ge::shooter {

// Whether any of the angles is non-zero (exactly: a model at 0 0 0 is never turned).
inline bool turned(Vec3f angles) { return angles.x != 0 || angles.y != 0 || angles.z != 0; }

// A point (or a direction) `d` from the model's origin, in the model's own frame at `angles`.
Vec3f to_model(Vec3f d, Vec3f angles);
// A point (or a direction) `l` of the model's frame at `angles` back in the world's, from its
// origin: the exact inverse of `to_model`.
Vec3f to_world(Vec3f l, Vec3f angles);

// How far from its origin a model with this box reaches turned any way round: the length to its
// farthest corner, so the cube of that half-width round the origin holds it at every angle.
float turned_reach(Vec3f mins, Vec3f maxs);

}  // namespace ge::shooter
