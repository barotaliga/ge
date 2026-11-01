---
type: Fixed
title: The GPU works on Windows
---

On Windows the device was never created: Dawn opens its DLLs at each search path plus the name, and Windows refuses its last try, the bare name, so `d3dcompiler_47.dll`, `vulkan-1.dll` and DXC never loaded and every GPU test and lab skipped. The engine now names System32 to Dawn in full, and the build copies the Windows SDK's `dxcompiler.dll` and `dxil.dll` beside every executable. The whole suite passes with `GE_GPU_REQUIRE=1` on an NVIDIA card over D3D12. `GE_GPU_NO_SOFTWARE=1` refuses a software adapter; CI's Windows row sets it, since its runner has only WARP.
