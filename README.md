# YARDA — C++ Static Reuse-Distance Analyzer

YARDA(Yet Another Reuse Distance Analyzer)는 C/C++ 프로그램의 반복적
메모리 접근을 LLVM IR에서 추출하고, 실제 loop bound로 접근 순서를 펼쳐
reuse-distance histogram(RDH)을 계산하는 C++17 정적 분석기입니다.

현재 지원 경로는 C++ LLVM frontend와 C++ backend만 사용합니다. 저장소에
남아 있는 Python predictor와 plotting 코드는 과거 연구 경로이며, 현재 기능
개발이나 결과 검증의 기준이 아닙니다.

## 현재 제공하는 기능

- LLVM 14 pass가 C/C++의 load/store, loop, direct call을 APE/LAT v2 JSON으로
  직렬화합니다.
- `ape.analyze` root만 선택하고 `ape.inline` direct callee를 call site
  순서로 확장합니다. C++ backend는 기존 LAT의 `yard.*` annotation도 읽습니다.
- 실제 loop start, bound, step과 affine index를 이용해 deterministic trace를
  생성합니다.
- element 또는 relative cache-line reference 단위로 exact RDH를 계산합니다.
- Fenwick tree 기반 `O(N log N)` reuse-distance 계산을 사용합니다.
- ABI padding을 포함한 배열·구조체 field의 정확한 byte offset과 leaf access
  size를 LAT v2 metadata에서 복원합니다.
- 한 접근이 여러 cache line에 걸치면 모든 line을 순서대로 생성합니다.
- versioned YAML cache 설정을 파싱하고 core 0 L1 line size를 CLI의
  cache-line granularity에 적용합니다.
- `--elf`로 `ET_EXEC` symbol과 canonical global object를 연결해 task별
  linked address/set/tag/offset과 coverage·exclusion을 JSON으로 출력합니다.
- 별도 ELF 도구와 C++ API로 `ET_EXEC`/`ET_DYN`의 data region과 object symbol,
  linked address basis를 읽을 수 있습니다.
- C++ 라이브러리에는 linked global address mapping과 set-local exact LRU RD
  분석기가 포함되어 있습니다.

## 파이프라인

```text
C/C++ source
    │ clang-14 -g -emit-llvm
    ▼
LLVM IR
    │ opt-14 + libLoopAnnotatedTrace.so
    ▼
APE/LAT v2 JSON
    ├── yarda_cpp --mode unroll
    │       └── element 또는 relative cache-line RDH
    └── yarda_cpp --elf PROGRAM.elf --cache CACHE.yaml
            └── ET_EXEC linked-address task mapping JSON
```

`yarda_cpp`는 현재 LAT JSON을 직접 입력받습니다. C source나 LLVM IR을
CLI에 바로 넘기는 wrapper는 현재 C++ 지원 경로에 포함되지 않습니다.

## 빌드

### 요구 사항

- CMake 3.20 이상
- C++17 compiler(GCC 11 이상 권장)
- LLVM/Clang 14 및 `llvm-objcopy-14`
- nlohmann/json 3.2 이상
- yaml-cpp 0.7 이상
- GoogleTest(테스트 빌드 시)

```bash
git clone --recurse-submodules \
  https://github.com/OBC-SIM/Yet-Another-Reuse-Distance-Analyzer
cd Yet-Another-Reuse-Distance-Analyzer

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

이미 clone한 저장소에서 frontend가 비어 있다면 다음을 먼저 실행합니다.

```bash
git submodule update --init --recursive
```

주요 산출물은 다음과 같습니다.

| 경로 | 역할 |
|---|---|
| `build/libLoopAnnotatedTrace.so` | LLVM LAT pass plugin |
| `build/LoopAnnotatedTraceTests` | frontend GTest executable |
| `build/backend/yarda_cpp` | exact unroll/RDH CLI |
| `build/backend/yarda_elf_regions` | ELF data-region inspection CLI |
| `build/backend/yarda_backend_tests` | trace/reuse/cache mapping tests |
| `build/backend/yarda_cache_config_tests` | YAML/cache configuration tests |
| `build/backend/yarda_elf_regions_tests` | ELF parser tests |

## 빠른 시작

### 1. C source를 LLVM IR로 변환

source-level 이름과 type metadata를 보존하기 위해 `-g`와 아래 O0 설정을
사용합니다.

```bash
clang-14 -O0 -Xclang -disable-O0-optnone -g \
  -emit-llvm -S -o tasks/test_1d_g.ll tasks/test_1d.c
```

### 2. APE/LAT v2 JSON 생성

pass는 현재 작업 디렉터리에 `<IR stem>_ape.json`을 생성합니다.

```bash
cd tasks
opt-14 -load-pass-plugin ../build/libLoopAnnotatedTrace.so \
  -passes=function\(mem2reg\),loop-simplify,loop-annotated-trace \
  test_1d_g.ll -o /dev/null
cd ..
```

결과 파일은 `tasks/test_1d_g_ape.json`입니다.

### 3. Element-level exact RDH 계산

```bash
./build/backend/yarda_cpp tasks/test_1d_g_ape.json \
  --mode unroll \
  --granularity element \
  --export /tmp/test_1d.element.json
```

### 4. Cache-line-level exact RDH 계산

```bash
./build/backend/yarda_cpp tasks/test_1d_g_ape.json \
  --mode unroll \
  --granularity cache-line \
  --cache backend/config/cache.32b.yaml \
  --export /tmp/test_1d.cache-line.json
```

현재 main CLI의 non-ELF cache-line 경로는 APE/LAT v2 access에 canonical
object ID를 사용하고, object가 없는 legacy access만 LAT reference name으로
fallback한 뒤 resolved relative byte offset을 cache-line index로 변환합니다.
과거 name 기반 동작으로 생성한 cache-line RDH는 동일 입력이라도 값이 달라질
수 있으므로 새 구현으로 다시 생성해야 합니다.
`--elf`를 지정하면 canonical global object를 `ET_EXEC` symbol에 연결하고
core 0 L1 geometry의 linked address, set, tag, offset을 task별 JSON으로
출력합니다. 다단계 cache hit/miss는 아직 계산하지 않습니다.

### 5. ELF-linked task mapping

```bash
./build/backend/yarda_cpp task_ape.json \
  --elf task.elf \
  --cache backend/config/cache.32b.yaml \
  --export /tmp/task.mapping.json
```

ELF 경로는 `APE_ANALYZE`/`YARD_ANALYZE` root를 독립 task로 유지하고 모든
지원 access를 먼저 linked byte address로 해석한 뒤 cache line으로 매핑합니다.
`ET_DYN`/PIE는 load bias가 없으므로 거부합니다.

## `yarda_cpp` CLI

```text
Usage: yarda_cpp LAT.json [--mode unroll]
                         [--granularity element|cache-line]
                         [--cache FILE]
                         [--elf FILE]
                         [--export PATH]
```

| 옵션 | 설명 |
|---|---|
| `LAT.json` | legacy 또는 APE/LAT v2 입력 파일 |
| `--mode unroll` | 유일하게 지원되는 mode. 생략해도 동일하게 동작 |
| `--granularity element` | non-ELF element/reference key 단위 RDH. 기본값 |
| `--granularity cache-line` | relative cache-line reference key 단위 RDH |
| `--cache FILE` | version 1 YAML cache hierarchy. cache-line 및 ELF 경로에서 필수 |
| `--elf FILE` | `ET_EXEC` symbol을 이용한 task별 cache-line linked-address mapping 선택 |
| `--export PATH` | deterministic JSON 결과 저장 경로 |

`--elf` 경로는 본질적으로 cache-line mapping을 수행합니다. granularity를
생략하거나 `--granularity cache-line`을 사용할 수 있으며, 명시적인
`--granularity element`는 거부합니다.

`predict`, `--plot`, `--save`, `--cache-line-size`, C/LLVM 입력 자동 변환은
현재 C++ CLI에서 지원하지 않습니다.

### Export 형식

```json
{
  "blocks": [
    {
      "name": "loop_1d  i-loop (bound=100)",
      "profile": {
        "cold_misses": 100,
        "histogram": {},
        "total_reuses": 0
      }
    }
  ],
  "cache_line_size": null,
  "file": "tasks/test_1d_g_ape.json",
  "granularity": "element",
  "mode": "unroll",
  "program": {
    "cold_misses": 100,
    "histogram": {},
    "total_reuses": 0
  }
}
```

`program`은 보고된 block trace를 순서대로 이어 계산한 exact profile이고,
`blocks`는 각 loop/flat block의 profile입니다. Histogram key는 JSON object
key이므로 문자열로 직렬화됩니다.

ELF mapping export는 `mode: "elf-task-mapping"`과
`address_basis: "linked_absolute"`를 사용합니다. 각 task에는 resolution
coverage, known non-inline static call-site 제외 수, ordered resolved accesses,
그리고 source/line ordinal과 load/store를 보존한 mapped line references가
포함됩니다. `known_non_inline_static_call_sites`는 inline expansion 후의 정적
호출 지점 수이며 루프 반복 횟수를 곱하지 않습니다. 다른 analyze root 호출도
caller의 opaque site로 세고 callee는 별도 task로 출력합니다. `--export`를
생략하면 같은 JSON을 stdout으로 출력합니다.

## 분석 대상 annotation

현재 frontend source pass는 `ape.*` annotation을 사용합니다. C++ backend는
이미 생성된 legacy LAT를 읽을 때 `yard.*` 이름도 호환합니다.

```c
#if defined(__clang__)
#define APE_ANALYZE __attribute__((annotate("ape.analyze")))
#define APE_INLINE  __attribute__((annotate("ape.inline")))
#else
#define APE_ANALYZE
#define APE_INLINE
#endif

APE_INLINE
void touch(float *array, int index)
{
  array[index] += 1.0f;
}

APE_ANALYZE
void kernel(float *array)
{
  for (int index = 0; index < 128; ++index)
    touch(array, index);
}
```

- `APE_ANALYZE`: 최종 report를 생성할 root function
- `APE_INLINE`: analyzed root의 direct call site에 펼칠 helper function
- annotation이 하나도 없으면 legacy `--mode unroll` 경로만 모든 정의된
  function을 분석합니다. `--elf` 경로는 `APE_ANALYZE`/`YARD_ANALYZE` root를
  반드시 요구합니다.
- indirect call, function pointer call, recursion은 지원하지 않음

`APE_INLINE` parameter의 storage identity는 `Call.arg_objects`를 통해 actual
object로 치환됩니다. 문자열 이름으로 ELF symbol을 추측하지 않습니다.

## APE/LAT v2 핵심 계약

LAT v2 root는 다음 필드를 갖습니다.

| 필드 | 설명 |
|---|---|
| `schema_version` | 현재 version `2` |
| `metadata.objects` | canonical storage object와 shape/type/size 정보 |
| `metadata.structs` | ABI struct size/alignment/field offset 정보 |
| `functions` | function wrapper 목록 |

주요 node는 다음과 같습니다.

| Node | 주요 필드 | 설명 |
|---|---|---|
| `Loop` | `var`, `start`, `bound`, `step`, `depth`, `body` | 반복 순서와 범위 |
| `Array` | `name`, `object`, `indices`, `access_path`, `op` | 배열 및 structured storage access |
| `Scalar` | `name`, `object`, `op` | 루프 인덱스와 무관한 memory access. legacy에서는 `object` 생략 가능 |
| `Call` | `callee`, `args`, `arg_objects` | annotated direct helper call |

`object`가 주소 계산의 canonical identity입니다. Scalar object identity를
보존하는 것과 Scalar를 linked cache 분석에 포함하는 것은 별도 정책입니다.
`name`은 표시용이며 ELF symbol lookup의 근거로 사용해서는 안 됩니다.
Structured access는 `access_path`의 field/index 순서와 `metadata.structs`를
함께 사용해 ABI padding이 반영된 byte span으로 해석합니다.

frontend schema의 전체 설명은 [`frontend/README.md`](frontend/README.md),
backend API와 CLI 설명은 [`backend/README.md`](backend/README.md)를
참고하세요.

## ELF inspection

```bash
./build/backend/yarda_elf_regions path/to/program.elf > /tmp/regions.json
```

이 도구는 allocated non-executable data region과 object symbol을 JSON으로
출력합니다.

- `ET_EXEC`: linked absolute address basis
- `ET_DYN`: load bias가 적용되지 않은 image-relative address basis
- TLS: ELF만으로 runtime address를 결정할 수 없어 제외

이 주소는 linked virtual/image address이며 자동으로 물리 주소가 되지
않습니다. `yarda_cpp --elf` 분석 경로는 load bias가 필요 없는 `ET_EXEC`의
absolute linked address만 허용합니다.

## 현재 범위와 제한

### 현재 main CLI가 보장하는 범위

- legacy 및 APE/LAT v2 module normalization
- annotated non-recursive direct call expansion
- 실제 loop bound와 순서를 사용하는 exact unroll
- element/relative cache-line RDH
- `ET_EXEC` global symbol 기반 task별 linked-address mapping
- source access와 cache-line provenance 및 mapping coverage 보고
- structured global access의 padding-aware byte offset 및 multi-line span
- deterministic console/JSON 결과

### 현재 main CLI에 아직 없는 기능

- L1 miss filtering과 L2 request stream
- hierarchy first-hit/EHC/AMC 및 traffic metric
- path-sensitive CFG와 indirect call
- stack/TLS/heap, PIE load bias, virtual-to-physical translation
- cache timing, coherence, prefetch, DMA, instruction-cache 모델

현재 논문 임계 경로의 linked-address 분석 대상은 canonical object ID와 정적
layout을 가진 scalar·array·structure global access입니다. Non-global,
pointer-backed, runtime-dependent access는 조용히 생략하지 않고 전체 CLI
호출을 categorized resolution error로 실패시킵니다. 알려진 non-inline static
call site는 access coverage와 분리된 exclusion count로 보고합니다.

## 테스트

전체 frontend/backend 테스트:

```bash
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

개별 executable을 직접 실행할 수도 있습니다.

```bash
./build/LoopAnnotatedTraceTests
./build/backend/yarda_backend_tests
./build/backend/yarda_cache_config_tests
./build/backend/yarda_elf_regions_tests
```

테스트 범위에는 frontend AST/metadata, annotated call expansion, exact reuse
profile, structured access layout, cross-line mapping, YAML validation, ELF
parsing, cache address decoding, set-local LRU와 CLI smoke test가 포함됩니다.

## 프로젝트 구조

```text
frontend/                       C++ LLVM 14 pass submodule
backend/
├── include/yarda/              public C++ API
│   ├── cache/                  geometry, mapping, set-local LRU, YAML config
│   ├── elf/                    ELF region and linked object address model
│   ├── reuse/                  exact reuse profile
│   └── trace/                  LAT schema, call expansion, trace APIs
├── src/
│   ├── cache/                  cache configuration/mapping/RD implementation
│   ├── cli/                    yarda_cpp, yarda_elf_regions
│   ├── elf/                    ELF parser and object mapping
│   ├── reuse/                  Fenwick-based reuse-distance implementation
│   └── trace/                  schema/layout/unroll implementation
├── tests/                      C++ GTest and CLI fixtures
└── config/                     versioned YAML cache examples
tasks/                          C benchmark and LAT fixtures
```

## 다음 구현 순서

1. operation-aware L1/L2 hierarchy 분석 및 독립 C++ reference oracle
2. deterministic hierarchy export와 paper experiment runner
3. path-aware LAT와 stack-frame address reconstruction은 이후 별도 작업

분석 로직과 reference model은 계속 C++로 구현합니다. Python port나 parity
작업은 현재 계획에 포함하지 않습니다.
