# YARDA C++ Backend

This backend consumes legacy or APE v2 Loop Annotated Trace JSON and computes
reuse-distance histograms without Python. It supports exact element/cache-line
unrolling and the element-granularity Dilation predictor.

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

./build/backend/yarda_cpp tasks/polybench_atax_g_ape.json \
  --mode predict \
  --granularity element \
  --export atax_predicted_rdh.json
```

Both modes implement LAT v1/v2 normalization, annotated direct-call expansion,
block profiles, and Python-compatible JSON export. Exact unrolling uses a
Fenwick tree for O(N log N) reuse-distance profiling. Predict mode implements
the 1D/2D/3D Dilation Equation path at element granularity; when a sampled reuse
family is unstable it preserves correctness by falling back to the exact C++
profile for that block.
