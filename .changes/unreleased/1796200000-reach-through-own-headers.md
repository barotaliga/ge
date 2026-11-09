---
type: Changed
---
`ge digest --affected`, `ge test --affected` and precheck reach less: a reached library object passes on only its own header (or, without one, its directory's headers it includes), not every header of its library's directories. One object of a shared render library including a changed header no longer reaches every lab that draws through that library. CI's labs smoke still runs every lab.
