---
type: Added
title: Sparks and debris bounce off whatever the frame drew
---

A particle pool can now collide with the last frame's depth (`ParticleSettings::collide`): sparks, grains and debris that fall bounce off the walls, floors and models the frame drew and come to rest on floors, in a world with no heightfield. Shooter's Edge's pool collides, and a blast's falling sparks and debris also come down on the floor under the blast when they are flung out of view.
