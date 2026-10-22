---
type: Changed
title: Cheaper remainder, 32-bit wrap and hypot, and a ski area a sixth faster
---

`num::mod` returns a dividend already inside the divisor without calling `fmod`, `num::to_int32` wraps a value within int64's range by an integer cast, `hypot` divides once instead of twice, and a heightfield with a power-of-two pitch multiplies by its exact inverse instead of dividing; every result is the same bits as before. With more of the generator's pure passes on threads, a ski area builds about 17% faster, with every map digest unchanged.
