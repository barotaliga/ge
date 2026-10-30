---
type: Added
---
Classic enhance upscales textures 4× with a trained dual aggregation transformer (`plugins/classicenhance/upscaler.hpp`). The tool fetches its CC0 weights once into the cache and checks their SHA-256; they are never committed. It runs in C++ on the CPU (`net.hpp`), within 1.2e-6 of the original network's output. It also runs through Dawn compute shaders on the GPU (`app/upscaler_gpu.hpp`), within one 8-bit step of the CPU. The tool picks the GPU unless its adapter is a software one (`--device`); `--upscaler=kernel` keeps the edge-following kernel and downloads nothing. The `gpu-upscaler` lab times the CPU against the GPU, and the `enhance` lab's `--weights` adds the network's column.
