---
title: A night needs a pre-exposure, and the moon's sky beside the sun's, not instead of it
date: 2026-10-23
scope: engine/render/night.hpp
concepts: [night, moon, pre-exposure, half-float, key-light, sky-view, handover, twilight]
---

In units of the sun's light a night is a millionth of a day, below a half-float's smallest
normal step: the night drew black and banded until the frame stored its light through a
pre-exposure (the estimated light on white brought to about a quarter of a unit, 1 by day so no
day bit moves) and every absolute reading (the eye's adaptation, a star) divided it out. And
swapping the sky's one table from the sun to the moon at a handover moment made the picture jump
(35 %, the dome losing most of its levels): keep the sun's sky table always and add the moon's
as a second, summed, and hand the KEY light over only once the sun is gone from every eye (three
degrees under); the jump fell to 0.1 %. When a handover jumps, check the lab aims both shots the
same way first: one bad jump here was the lab's lens, not the sky.
