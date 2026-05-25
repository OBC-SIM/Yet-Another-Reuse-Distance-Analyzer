# Yet-Another-Reuse-Distance-Analyzer

"Static Estimation of Reuse Profiles for Arrays in Nested Loops" 논문의 **Alternative Static Analyzer** C++ 재현 프로젝트입니다.

프로그램을 실행하거나 메모리 트레이스를 수집하지 않고, **LLVM 정적 분석만으로** 중첩 루프 내 배열 접근의 재사용 거리 히스토그램(RDH)을 정적으로 예측합니다.

---

## 아키텍처

```
C/C++ 소스
    │
    │  clang-14 -O0 -Xclang -disable-O0-optnone -g -emit-llvm
    ▼
LLVM IR (.ll)
    │
    │  opt-14 -passes=function(mem2reg),loop-simplify,loop-annotated-trace
    ▼
Loop Annotated Trace (JSON)
    │
    │  python3 backend/main.py <file.c>
    ▼
RDH (Reuse Distance Histogram)
```

**C++ 프론트엔드** (`src/`, `include/`)가 LLVM Pass로 IR을 분석해 루프 구조, 시작 값, 배열 접근 패턴을 JSON으로 출력합니다. **Python 백엔드** (`backend/`)는 이 JSON을 받아 소규모 LRU 시뮬레이션으로 Dilation Equation 계수를 도출하고, 실제 바운드에서의 RDH를 정적으로 예측합니다.

---

## 출력 형식

`{ll파일명}_lat.json`은 함수별 wrapper를 갖고, 각 함수 wrapper는 `function`, `params`, `annotations`, `body` 필드를 가집니다. `body`는 네 가지 노드 타입으로 구성됩니다.

| 타입 | 필드 | 설명 |
|------|------|------|
| Function wrapper | `function`, `params`, `annotations`, `body` | 함수 이름, 파라미터 이름, `yard.*` annotation, 함수 본문 |
| `Loop` | `var`, `start`, `bound`, `depth`, `body` | 루프 노드. `start` 이상 `bound` 미만 반복 |
| `Array` | `name`, `indices` | 배열 접근. 인덱스는 루프 유도 변수, 상수, `i-1` 같은 affine 식 |
| `Scalar` | `name` | 루프 인덱스와 무관한 스칼라 접근 |
| `Call` | `callee`, `args` | direct call node. Python 백엔드에서 `YARD_INLINE` callee를 call site에 확장 |

**예시 — 행렬 곱셈 (`test_matmul_g.ll`)**

```c
void matmul(float A[32][64], float B[64][32], float C[32][32]) {
    for (int i = 0; i < 32; i++)
        for (int j = 0; j < 32; j++)
            for (int k = 0; k < 64; k++)
                C[i][j] += A[i][k] * B[k][j];
}
```

```json
[{"function":"matmul","params":["A","B","C"],"annotations":["yard.analyze"],"body":[
  {"type":"Loop","var":"i","start":0,"bound":32,"depth":1,"body":[
    {"type":"Loop","var":"j","start":0,"bound":32,"depth":2,"body":[
      {"type":"Loop","var":"k","start":0,"bound":64,"depth":3,"body":[
        {"type":"Array","name":"A","indices":["i","k"]},
        {"type":"Array","name":"B","indices":["k","j"]},
        {"type":"Array","name":"C","indices":["i","j"]},
        {"type":"Array","name":"C","indices":["i","j"]}
      ]}
    ]}
  ]}
]}]}]
```

---

## 빌드

**요구 사항:** LLVM 14, CMake ≥ 3.20, GCC ≥ 11, GTest, Python ≥ 3.10, pytest, matplotlib/seaborn(플롯 옵션)

```bash
git clone <repo>
cd Yet-Another-Reuse-Distance-Analyzer

# C++ 프론트엔드
cmake -DLLVM_DIR=$(llvm-config-14 --cmakedir) -B build -S .
cmake --build build

# Python 백엔드 의존성
pip install pytest matplotlib seaborn
```

빌드 산출물:
- `build/libLoopAnnotatedTrace.so` — opt에 로드할 Pass 플러그인
- `build/LoopAnnotatedTraceTests` — GTest 바이너리

---

## 사용법

### 1. C 소스 → LLVM IR 컴파일

현재 파이프라인은 source-level 메모리 접근을 보존하기 위해 아래 옵션을 사용합니다.

```bash
clang-14 -O0 -Xclang -disable-O0-optnone -g \
         -emit-llvm -S -o <name>_g.ll <name>.c
```

`-g` 플래그가 없으면 변수명을 debug info에서 추출할 수 없어 이름이 IR 슬롯 번호로 대체됩니다.

### 2. Pass 실행

```bash
opt-14 -load-pass-plugin ./build/libLoopAnnotatedTrace.so \
       -passes=function\(mem2reg\),loop-simplify,loop-annotated-trace \
       <name>_g.ll -o /dev/null
```

현재 디렉토리에 `<name>_g_lat.json`이 생성됩니다.

### RTEMS / 사용자 함수만 분석하기

RTEMS 커널이나 라이브러리 함수까지 같은 LLVM 모듈에 들어오는 경우,
분석 대상 함수에 Clang annotation을 붙여 사용자 정의 함수만 필터링할 수 있습니다.

```c
#if defined(__clang__)
#define YARD_ANALYZE __attribute__((annotate("yard.analyze")))
#define YARD_INLINE  __attribute__((annotate("yard.inline")))
#else
#define YARD_ANALYZE
#define YARD_INLINE
#endif

YARD_ANALYZE
void user_kernel(float *a, float *b) {
    for (int i = 0; i < 128; ++i)
        a[i] = b[i] + 1.0f;
}
```

`YARD_ANALYZE`는 최종 report 대상 root 함수에 붙입니다. RTEMS task
entry나 application kernel처럼 독립적으로 보고 싶은 함수가 여기에 해당합니다.

`YARD_INLINE`은 다른 analyzed 함수 안에서 펼칠 helper 함수에 붙입니다.
Python 백엔드는 `YARD_INLINE` 함수 본문을 call site 위치에 확장하지만,
helper 자체는 standalone report 대상에서 제외합니다.

annotation이 전혀 없으면 기존처럼 모든 정의된 함수를 분석합니다.

`YARD_INLINE` 함수로의 직접 호출은 LAT에 `Call` node로 보존됩니다.

```c
YARD_INLINE
void touch(float *x, int idx) {
    x[idx] = x[idx] + 1.0f;
}

YARD_ANALYZE
void kernel(float *a) {
    for (int i = 0; i < 128; ++i)
        touch(a, i);
}
```

지원 범위는 direct call + non-recursive `YARD_INLINE` callee입니다.
function pointer, indirect call, recursion은 지원하지 않습니다.
`rtems_task_start()`에 넘기는 task entry는 inline 대상이 아니므로,
task entry 함수 자체에 `YARD_ANALYZE`를 붙여 별도 root로 분석하세요.

### 3. 결과 확인

```bash
python3 -m json.tool <name>_g_lat.json
```

### 4. RDH 예측

CLI로는 C 소스 또는 `.ll` 파일을 바로 넣을 수 있습니다.

```bash
# Dilation 예측 (기본, 빠름)
python backend/main.py tasks/test_stencil.c

# 실제 loop unroll + LRU 시뮬레이션 (정확, 느림)
python backend/main.py --mode unroll tasks/test_stencil.c

# 플롯 저장 (figs/<stem>_blocks.png, figs/<stem>_program.png)
python backend/main.py --plot tasks/test_stencil.c

# 저장 경로 직접 지정 (_blocks / _program suffix 자동 추가)
python backend/main.py --save figs/out.png tasks/test_matmul.c
```

| 옵션 | 설명 |
|------|------|
| `--mode predict` | Dilation Equation 정적 예측 (기본값) |
| `--mode unroll`  | 실제 loop unroll + LRU 시뮬레이션 (ground truth) |

`--plot` / `--save`를 주면 두 파일이 생성됩니다.

| 파일 | 내용 |
|------|------|
| `<stem>_blocks.png`  | 루프 블록별 RDH (subplot 1개 = 블록 1개) |
| `<stem>_program.png` | 프로그램 전체 합산 RDH |

cold miss는 RD = −1 bin으로 맨 앞에 표시됩니다. scale gap이 4배 이상이면 broken axis로 자동 분리됩니다.

라이브러리처럼 사용할 때는 `backend/predictor.py`의 `analyze()` 또는 `analyze_blocks()`를 호출합니다.

```python
import sys; sys.path.insert(0, 'backend')
from predictor import analyze, analyze_blocks

# 함수 전체 합산 프로파일
profile = analyze('<name>_g_lat.json')
print('histogram:', profile.histogram)
print('cold misses:', len(profile.cold_misses))

# 블록별 프로파일 리스트
for name, block_profile in analyze_blocks('<name>_g_lat.json'):
    print(name, block_profile.histogram)
```

### 실행 예시 (tasks/ 디렉토리)

```bash
cd tasks

# 1D 루프
clang-14 -O0 -Xclang -disable-O0-optnone -g -emit-llvm -S -o test_1d_g.ll test_1d.c
opt-14 -load-pass-plugin ../build/libLoopAnnotatedTrace.so \
       -passes=function\(mem2reg\),loop-simplify,loop-annotated-trace \
       test_1d_g.ll -o /dev/null

# 행렬 곱셈
clang-14 -O0 -Xclang -disable-O0-optnone -g -emit-llvm -S -o test_matmul_g.ll test_matmul.c
opt-14 -load-pass-plugin ../build/libLoopAnnotatedTrace.so \
       -passes=function\(mem2reg\),loop-simplify,loop-annotated-trace \
       test_matmul_g.ll -o /dev/null
```

---

## 테스트

### C++ (GTest)

```bash
./build/LoopAnnotatedTraceTests
```

26개 테스트.

| 테스트 스위트 | 내용 |
|---|---|
| `ScalarAccess` | 스칼라 접근 생성 및 JSON 직렬화 |
| `ArrayAccess` | 배열 접근 생성, 상수 인덱스 구분 포함 |
| `LoopNest` | 루프 트리 구성 및 중첩 JSON 직렬화 |
| `CallStmt` | 함수 호출 노드 생성 및 JSON 직렬화 |
| `GetBaseName` | `IrHelpers::getBaseName` — 무명 변수 IR 슬롯 번호 구분 |
| `GetValueName` | 함수 파라미터와 call argument 이름 추출 |
| `ResolveIndex` | 스칼라 argument index 이름 보존 |
| `FunctionAnnotation` | `yard.analyze` / `yard.inline` annotation 감지 |

### Python (pytest)

```bash
pytest -q
```

76개 테스트.

| 테스트 파일 | 내용 |
|---|---|
| `test_parser.py` | TraceNode AST, unroll(), parse_trace(), loop start/affine index |
| `test_calls.py` | `CallNode` expansion, analyze root filtering, recursion guard |
| `test_lru_sim.py` | LRU 스택 시뮬레이션, 재사용 거리 계산 |
| `test_dilation.py` | Dilation 수식 (2D/3D), Strategy/Builder/Predictor |
| `test_merger.py` | BlockMerger cold miss 조정, cross-block 재사용 조정, intra-block 중복 방지 |
| `test_e2e.py` | 전체 파이프라인 E2E, LAT body 순서 보존, function-level cross-block GT |

### 예측 정확도 검증 (verify.py)

```bash
# 내장 케이스 5개 검증
python backend/verify.py

# C/LLVM 파일 지정
python backend/verify.py tasks/test_stencil.c tasks/polybench_2mm.c

# GT vs. 예측 비교 플롯 생성
python backend/verify.py --plot tasks/test_matmul.c

# 저장 경로 직접 지정
python backend/verify.py --save figs/compare.png tasks/polybench_atax.c
```

ground-truth(완전 언롤 LRU)와 Dilation 예측을 케이스별로 비교합니다. C/LLVM 입력을 주면 컴파일과 pass 실행까지 포함해 검증합니다.

`verify.py`는 디버깅용 block-level 비교를 먼저 출력하고, 이어서 함수 전체 LAT body 순서를 실제로 unroll한 function-level GT와 `BlockMerger` 기반 예측을 비교합니다. function-level 비교는 loop 사이의 cross-block RD를 포함하므로 block-level MATCH와 별개로 MISMATCH가 드러날 수 있습니다.

`--plot` / `--save` 옵션을 주면 함수 단위 비교 차트가 생성됩니다.

| 파일 | 내용 |
|------|------|
| `verify_<stem>_functions.png`        | 함수별 GT(파랑) vs. 예측(주황) RDH |
| `verify_<stem>_timing_functions.png` | 함수별 unroll 시간 vs. 예측 시간 (ms) |

cold miss는 RD = −1 bin으로 맨 앞에 표시됩니다.

| 케이스 | 결과 |
|---|---|
| 2D loop j=8, k=8 | ✅ MATCH |
| matmul i=3, j=3, k=3 | ✅ MATCH |
| matmul i=4, j=4, k=4 | ✅ MATCH |
| matmul i=8, j=8, k=8 | ✅ MATCH |
| ATAX i=100, j=100, k=100 | ✅ MATCH |
| test_stencil.c | ✅ MATCH |
| test_regular_block.c | ✅ MATCH |
| test_call.c | ✅ MATCH |
| polybench_gemm.c | ⚠️ loop block은 MATCH, function-level cross-block MISMATCH |
| polybench_atax.c | ✅ MATCH |
| polybench_2mm.c | ⚠️ loop block은 MATCH, function-level cross-block MISMATCH |
| polybench_correlation.c | ⚠️ 마지막 3D `i-loop (bound=25)` MISMATCH |
| polybench_jacobi.c | ⚠️ `jacobi_2d_kernel`의 3D `t-loop (bound=5)` 및 function-level MISMATCH |

현재 `polybench_correlation.c`의 미해결 케이스는 sample마다 volatile RD group count가 달라지는 3D sparse/tail family를 기존 rectangular predictor가 표현하지 못해서 발생합니다. `polybench_jacobi.c`는 `t` 루프 안에 두 개의 2D stencil nest가 순차 배치된 구조라, 현재 3D predictor가 하나의 rectangular nest처럼 처리하면서 histogram과 cold miss가 함께 어긋납니다.

`polybench_gemm.c`와 `polybench_2mm.c`는 개별 loop block RDH는 맞지만, 함수 전체에서 같은 배열 reference가 여러 loop block을 건너 재사용되는 경우 현재 `BlockMerger`가 sample trace 기반 LRU stack으로 cross-block RD를 보정하기 때문에 function-level GT와 차이가 납니다. 논문식 sequence-based cross-block prediction은 아직 별도 과제로 남아 있습니다.

### Legacy analyzer 비교

기존 MEM_RD_IR 결과 JSON과 현재 analyzer 결과를 같은 입력에서 비교할 수 있습니다.

```bash
python backend/compare_legacy.py tasks/polybench_atax.c
python backend/compare_legacy.py tasks/polybench_*.c --save figs/legacy_compare
```

기본 legacy 결과 위치는 `/workspace/caas/MEM_RD_IR/Static-Memory-Reuse-Distance-on-LLVM/results`입니다. `--legacy-dir` 또는 `--legacy-json`으로 경로를 바꿀 수 있습니다.

---

## 프로젝트 구조

```
include/
├── Statement.hpp         # AST 노드: Statement / ScalarAccess / ArrayAccess / LoopNest / CallStmt
├── JsonExportVisitor.hpp # Visitor: AST → llvm::json::Value
└── IrHelpers.hpp         # IR 쿼리 헬퍼 선언 (NameMap, getBaseName 등)
src/
├── IrHelpers.cpp         # buildDebugNameMap, getInductionVarName, getLoopStart, getBaseName,
│                         # resolveIndex, getIndexVars, irOperandName
└── LoopAnalysisPass.cpp  # LLVM Pass: makeAccessFromInstr → buildRootStatements
tests/
├── Statement_test.cpp    # AST / JSON 직렬화 단위 테스트
└── IrHelpers_test.cpp    # IR 헬퍼 단위 테스트 (LLVM IR 직접 생성)
backend/
├── block_trace.py        # LAT body 순서를 보존하는 actual trace / block trace helper
├── calls.py              # annotated direct call의 AST-level inline expansion
├── parser.py             # TraceNode AST + parse_trace() + loop start/affine index 해석
├── lru_sim.py            # ReuseProfile + LRUProfiler
├── dilation.py           # Dilation Equation (Strategy/Factory/Builder/Predictor)
├── merger.py             # BlockMerger (stateful, cross-block 재사용 조정)
├── predictor.py          # LAT JSON → ReuseProfile 예측 엔진 (analyze, analyze_blocks)
├── main.py               # CLI 파이프라인: C/LLVM IR → LAT → RDH 출력/플롯 (--mode predict|unroll)
├── _plot_utils.py        # 공유 시각화 인프라 (binning, broken axis, bar helpers)
├── plot.py               # RDH 시각화 (plot_histograms, plot_verify_comparison, aggregate_as_program)
├── plot_timing.py        # 실행시간 비교 차트 (plot_timing_comparison, aggregate_timing_as_program)
├── stability.py          # stable RD 후보 검증 (2D holdout validation)
├── volatile.py           # 공용 volatile helper + 3D diagonal/rectangular RD 예측
├── volatile2d.py         # 2D Volatile RD 예측
├── gt_cache.py           # Ground-truth 계산 + SHA-256 캐시
├── report.py             # 비교 출력 헬퍼 (timed / print_comparison)
├── verify.py             # block/function-level GT vs. 예측 비교 스크립트
├── compare_legacy.py     # legacy analyzer JSON vs. current analyzer 비교 플롯
└── tests/
    ├── test_parser.py
    ├── test_calls.py
    ├── test_lru_sim.py
    ├── test_dilation.py
    ├── test_merger.py
    └── test_e2e.py       # E2E 테스트
tasks/
├── test_1d.c             # 단순 1D 루프
├── test_2d.c             # 2D 루프
├── test_2steps.c         # stride-2 1D 루프
├── test_matmul.c         # 행렬 곱셈 (3중 루프)
├── test_stencil.c        # 스텐실 패턴
├── test_multi_array.c    # 다중 배열 접근
├── test_global.c         # 전역 배열
├── test_local.c          # 로컬 배열
├── test_regular_block.c  # 루프 경계 바깥 블록 순서 검증
├── test_call.c           # annotated direct call expansion 검증
├── test_constant_access.c# 상수 인덱스 접근 (A[0], array[1] 등)
├── polybench_gemm.c      # PolyBench: C = α·A·B + β·C
├── polybench_atax.c      # PolyBench: y = Aᵀ·(A·x)
├── polybench_2mm.c       # PolyBench: D = A·B·C (2단계 행렬 곱)
├── polybench_correlation.c # PolyBench: 상관관계 행렬
└── polybench_jacobi.c    # PolyBench: Jacobi 2D 스텐실
pyproject.toml            # pytest 설정 (testpaths, pythonpath)
```

---

## 구현 상태

| 기능 | 상태 |
|------|------|
| LLVM Pass 플러그인 기반 정적 분석 | ✅ |
| 루프 트리 구성 (RPO 순회, 중첩 루프) | ✅ |
| 배열 접근 인덱스 추출 (루프 IV, 상수) | ✅ |
| 루프 시작 값 `start` 보존 및 JSON 출력 | ✅ |
| affine 인덱스 (`i-1`, `i+1`) 보존/해석 | ✅ |
| `__attribute__((annotate("yard.analyze")))` 기반 함수 필터링 | ✅ |
| `YARD_INLINE` direct call의 AST-level expansion | ✅ |
| 전역 배열 `ConstantExpr` GEP 처리 | ✅ |
| 무명 변수 IR 슬롯 번호 구분 | ✅ |
| JSON → TraceNode AST 역직렬화 (`parser.py`) | ✅ |
| LRU 스택 시뮬레이션 (`lru_sim.py`) | ✅ |
| Dilation Equation 솔버 (`dilation.py`) | ✅ |
| 블록 간 재사용 조정 및 병합 (`merger.py`) | ✅ |
| 1D loop actual-bound frequency 확장 | ✅ |
| function-level top-level access 병합 | ✅ |
| LAT body 순서 보존 actual trace helper (`block_trace.py`) | ✅ |
| 파이프라인 오케스트레이터 (`main.py`) | ✅ |
| Volatile RD 예측 (2D rectangular, 3D diagonal/일부 rectangular) | ✅ |
| Cold miss 정확 예측 (`_predict_cold_misses`) | ✅ |
| block/function-level Ground-truth vs. 예측 비교 검증 (`verify.py`) | ✅ |
| legacy analyzer 결과 비교 (`compare_legacy.py`) | ✅ |
| 블록별 / 프로그램 전체 RDH 시각화 (`plot.py`, `--plot`/`--save`) | ✅ |
| GT vs. 예측 비교 시각화 (grouped bar, cold miss RD=−1, broken axis 자동) | ✅ |
| 실행시간 비교 시각화 (`plot_timing.py`, verify `--plot`) | ✅ |
| `--mode unroll`로 실제 LRU 시뮬 결과 출력 (`main.py`) | ✅ |
| sequence-based cross-block RD prediction | 🔲 미구현 |
| `polybench_correlation.c` 마지막 3D sparse/tail family | 🔲 미해결 |
| `polybench_jacobi.c` sequential 2D stencil nests | 🔲 미해결 |
| 4중 루프 이상 지원 | 🔲 미구현 |
