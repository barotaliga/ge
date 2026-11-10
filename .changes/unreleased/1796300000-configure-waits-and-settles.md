---
type: Fixed
---
`make build`, `make test`, `make shaders-check` and precheck configure through `tools/configure.sh`, which waits for a cloud session's background start build to finish instead of regenerating build.ninja beside it, and dates build.ninja no older than CMake's glob stamp, so Ninja no longer loops on "build.ninja still dirty".
