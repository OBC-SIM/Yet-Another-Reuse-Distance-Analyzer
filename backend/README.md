# YARDA C++ Backend

This backend consumes legacy or APE v2 Loop Annotated Trace JSON and computes
reuse-distance histograms without Python. It supports exact element/cache-line
unrolling.

## Build and test

Requirements: LLVM 14, CMake 3.20+, a C++17 compiler, nlohmann/json, and
GTest. The root build configures both the frontend submodule and this backend.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

CTest runs the frontend and backend test suites together.

## Run

```bash
./build/backend/yarda_cpp tasks/polybench_atax_g_ape.json \
  --mode unroll \
  --granularity cache-line \
  --cache-line-size 32 \
  --export atax_rdh.json
```

The unroll path implements LAT v1/v2 normalization, annotated direct-call
expansion, block profiles, and Python-compatible JSON export. Exact unrolling
uses a Fenwick tree for O(N log N) reuse-distance profiling. `--mode unroll`
remains accepted for command-line compatibility; `--mode predict` is not
supported.
