# Yet-Another-Reuse-Distance-Analyzer

"Static Estimation of Reuse Profiles for Arrays in Nested Loops" 논문의 **Alternative Static Analyzer** C++ 재현 프로젝트입니다.

프로그램을 실행하거나 메모리 트레이스를 수집하지 않고, **LLVM 정적 분석만으로** 중첩 루프 내 배열 접근의 재사용 거리 히스토그램(RDH)을 정적으로 예측합니다.

---

## 아키텍처

```
C/C++ 소스
    │
    │  clang-14 -O1 -g -emit-llvm
    ▼
LLVM IR (.ll)
    │
    │  opt-14 -passes=loop-annotated-trace
    ▼
Loop Annotated Trace (JSON)
    │
    │  python3 -c "from backend.main import analyze; analyze(...)"
    ▼
RDH (Reuse Distance Histogram)
```

**C++ 프론트엔드** (`src/`, `include/`)가 LLVM Pass로 IR을 분석해 루프 구조와 배열 접근 패턴을 JSON으로 출력합니다. **Python 백엔드** (`backend/`)는 이 JSON을 받아 소규모 LRU 시뮬레이션으로 Dilation Equation 계수를 도출하고, 실제 바운드에서의 RDH를 정적으로 예측합니다.

---

## 출력 형식

`{ll파일명}_loop_annotated_trace.json`의 노드는 세 가지 타입으로 구성됩니다.

| 타입 | 필드 | 설명 |
|------|------|------|
| `Loop` | `var`, `bound`, `depth`, `body` | 루프 노드. `body`에 자식 노드 포함 |
| `Array` | `name`, `indices` | 배열 접근. 인덱스는 루프 유도 변수 또는 상수 |
| `Scalar` | `name` | 루프 인덱스와 무관한 스칼라 접근 |

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
[{"type":"Loop","var":"i","bound":32,"depth":1,"body":[
  {"type":"Loop","var":"j","bound":32,"depth":2,"body":[
    {"type":"Loop","var":"k","bound":64,"depth":3,"body":[
      {"type":"Array","name":"A","indices":["i","k"]},
      {"type":"Array","name":"B","indices":["k","j"]},
      {"type":"Array","name":"C","indices":["i","j"]},
      {"type":"Array","name":"C","indices":["i","j"]}
    ]}
  ]}
]}]
```

---

## 빌드

**요구 사항:** LLVM 14, CMake ≥ 3.20, GCC ≥ 11, GTest, Python ≥ 3.10, pytest

```bash
git clone <repo>
cd Yet-Another-Reuse-Distance-Analyzer

# C++ 프론트엔드
cmake -DLLVM_DIR=$(llvm-config-14 --cmakedir) -B build -S .
cmake --build build

# Python 백엔드 의존성
pip install pytest
```

빌드 산출물:
- `build/libLoopAnnotatedTrace.so` — opt에 로드할 Pass 플러그인
- `build/LoopAnnotatedTraceTests` — GTest 바이너리

---

## 사용법

### 1. C 소스 → LLVM IR 컴파일

```bash
clang-14 -O1 -g -emit-llvm -S -o <name>_g.ll <name>.c
```

`-g` 플래그가 없으면 변수명을 debug info에서 추출할 수 없어 이름이 IR 슬롯 번호로 대체됩니다.

### 2. Pass 실행

```bash
opt-14 -load-pass-plugin ./build/libLoopAnnotatedTrace.so \
       -passes=loop-annotated-trace <name>_g.ll -o /dev/null
```

현재 디렉토리에 `<name>_g_loop_annotated_trace.json`이 생성됩니다.

### 3. 결과 확인

```bash
python3 -m json.tool <name>_g_loop_annotated_trace.json
```

### 4. RDH 예측

```python
import sys; sys.path.insert(0, 'backend')
from main import analyze

profile = analyze('<name>_g_loop_annotated_trace.json')
print('histogram:', profile.histogram)
print('cold misses:', len(profile.cold_misses))
```

### 실행 예시 (tasks/ 디렉토리)

```bash
cd tasks

# 1D 루프
clang-14 -O1 -g -emit-llvm -S -o test_1d_g.ll test_1d.c
opt-14 -load-pass-plugin ../build/libLoopAnnotatedTrace.so \
       -passes=loop-annotated-trace test_1d_g.ll -o /dev/null

# 행렬 곱셈
clang-14 -O1 -g -emit-llvm -S -o test_matmul_g.ll test_matmul.c
opt-14 -load-pass-plugin ../build/libLoopAnnotatedTrace.so \
       -passes=loop-annotated-trace test_matmul_g.ll -o /dev/null
```

---

## 테스트

### C++ (GTest)

```bash
./build/LoopAnnotatedTraceTests
```

19개 테스트.

| 테스트 스위트 | 내용 |
|---|---|
| `ScalarAccess` | 스칼라 접근 생성 및 JSON 직렬화 |
| `ArrayAccess` | 배열 접근 생성, 상수 인덱스 구분 포함 |
| `LoopNest` | 루프 트리 구성 및 중첩 JSON 직렬화 |
| `GetBaseName` | `IrHelpers::getBaseName` — 무명 변수 IR 슬롯 번호 구분 |

### Python (pytest)

```bash
pytest -q
```

59개 테스트.

| 테스트 파일 | 내용 |
|---|---|
| `test_parser.py` | TraceNode AST, unroll(), parse_trace() |
| `test_lru_sim.py` | LRU 스택 시뮬레이션, 재사용 거리 계산 |
| `test_dilation.py` | Dilation 수식 (2D/3D), Strategy/Builder/Predictor |
| `test_merger.py` | BlockMerger cold miss 조정, cross-block 재사용 조정 |
| `test_e2e.py` | 전체 파이프라인 E2E (1D/2D/3D matmul/Scalar) |

### 예측 정확도 검증 (verify.py)

```bash
python backend/verify.py
```

ground-truth(완전 언롤 LRU)와 Dilation 예측을 케이스별로 비교합니다.

| 케이스 | 결과 |
|---|---|
| 2D loop j=8, k=8 | ✅ MATCH |
| matmul i=3, j=3, k=3 | ✅ MATCH |
| matmul i=4, j=4, k=4 | ✅ MATCH |
| matmul i=8, j=8, k=8 | ✅ MATCH |
| ATAX i=100, j=100, k=100 | ✅ MATCH |

---

## 프로젝트 구조

```
include/
├── Statement.hpp         # AST 노드: Statement / ScalarAccess / ArrayAccess / LoopNest
├── JsonExportVisitor.hpp # Visitor: AST → llvm::json::Value
└── IrHelpers.hpp         # IR 쿼리 헬퍼 선언 (NameMap, getBaseName 등)
src/
├── IrHelpers.cpp         # buildDebugNameMap, getInductionVarName, getBaseName,
│                         # resolveIndex, getIndexVars, irOperandName
└── LoopAnalysisPass.cpp  # LLVM Pass: makeAccessFromInstr → buildRootStatements
tests/
├── Statement_test.cpp    # AST / JSON 직렬화 단위 테스트
└── IrHelpers_test.cpp    # IR 헬퍼 단위 테스트 (LLVM IR 직접 생성)
backend/
├── parser.py             # TraceNode AST + parse_trace()
├── lru_sim.py            # ReuseProfile + LRUProfiler
├── dilation.py           # Dilation Equation (Strategy/Factory/Builder/Predictor)
├── merger.py             # BlockMerger (stateful, cross-block 재사용 조정)
├── main.py               # 파이프라인 오케스트레이터: analyze(json_path) → ReuseProfile
└── tests/
    ├── test_parser.py
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
└── test_constant_access.c# 상수 인덱스 접근 (A[0], array[1] 등)
pyproject.toml            # pytest 설정 (testpaths, pythonpath)
```

---

## 구현 상태

| 기능 | 상태 |
|------|------|
| LLVM Pass 플러그인 기반 정적 분석 | ✅ |
| 루프 트리 구성 (RPO 순회, 중첩 루프) | ✅ |
| 배열 접근 인덱스 추출 (루프 IV, 상수) | ✅ |
| 전역 배열 `ConstantExpr` GEP 처리 | ✅ |
| 무명 변수 IR 슬롯 번호 구분 | ✅ |
| JSON → TraceNode AST 역직렬화 (`parser.py`) | ✅ |
| LRU 스택 시뮬레이션 (`lru_sim.py`) | ✅ |
| Dilation Equation 솔버 (`dilation.py`) | ✅ |
| 블록 간 재사용 조정 및 병합 (`merger.py`) | ✅ |
| 파이프라인 오케스트레이터 (`main.py`) | ✅ |
| Volatile RD 예측 (논문 §3.7.2, n×n×n 대각선 케이스) | ✅ |
| Cold miss 정확 예측 (`_predict_cold_misses`) | ✅ |
| Ground-truth vs. 예측 비교 검증 (`verify.py`) | ✅ |
| Rectangular 루프 (i≠j≠k) volatile 예측 | 🔲 미구현 |
| 4중 루프 이상 지원 | 🔲 미구현 |