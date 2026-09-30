# YARDA — C++ Reuse-Distance and Cache-Hierarchy Analyzer

YARDA(Yet Another Reuse Distance Analyzer)는 C/C++의 메모리 접근을 LLVM IR에서
추출해 reuse-distance histogram과 L1/LLC 계층 지표를 계산하는 C++17 정적 분석기입니다.

- Element/cache-line 단위 exact RDH와 ELF-linked 주소 mapping
- Streaming Full Exact Cache-Set Reuse Distance(CSRD), First-Hit 및 All-Cache Miss 지표
- RESULT v2 JSON과 선택적 EVENTS·TELEMETRY 출력
- 선택적 C11 구간 분석 및 batch/streaming runtime·RSS 비교 도구

## 빌드

CMake 3.20+, C++17 compiler, LLVM/Clang 14, `llvm-objcopy-14`,
nlohmann/json 3.10.5+, yaml-cpp 0.7+가 필요합니다.
테스트 빌드에는 GoogleTest가 필요합니다.

```sh
git clone --recurse-submodules \
  https://github.com/OBC-SIM/Yet-Another-Reuse-Distance-Analyzer
cd Yet-Another-Reuse-Distance-Analyzer

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

구간 frontend는 `-DYARDA_BUILD_REGION_FRONTEND=ON`으로 켭니다.
평가 도구는 여기에 `-DYARDA_BUILD_HIERARCHY_EXPERIMENTS=ON`을 추가합니다.
추가 의존성과 실행 절차는 아래 상세 문서를 참고하세요.

## 빠른 시작: L1/LLC 계층 분석

저장소 루트에서 예제 `tasks/test_1d.c`의 MAP와 non-PIE ELF를 생성한 뒤 분석합니다.
`yarda_cpp`는 C source 대신 MAP JSON을 입력받습니다.

```sh
clang-14 -O0 -Xclang -disable-O0-optnone -g \
  -emit-llvm -S -o tasks/test_1d_g.ll tasks/test_1d.c

cd tasks
opt-14 -load-pass-plugin ../build/libMemoryAccessPatterns.so \
  '-passes=function(mem2reg),loop-simplify,loop-annotated-trace' \
  test_1d_g.ll -o /dev/null
cd ..

clang-14 -O0 -g -fno-pie -no-pie tasks/test_1d.c -o /tmp/test_1d.elf

./build/backend/yarda_cpp tasks/test_1d_g_ape.json \
  --analysis hierarchy-rd \
  --elf /tmp/test_1d.elf \
  --cache backend/config/cache.32b.yaml \
  --export /tmp/test_1d.result.json
```

결과는 `/tmp/test_1d.result.json`에 저장됩니다. Task별 CSRD histogram,
First-Hit Count/Ratio와 All-Cache Miss Count/Ratio를 RESULT v2로 출력합니다.
전체 옵션은 `./build/backend/yarda_cpp --help`에서 확인할 수 있습니다.

## 분석 범위

계층 분석은 `ET_EXEC`의 정적 global 주소와 core 0의 L1 → LLC → Memory 경로를
사용합니다. 두 cache는 같은 line size와 LRU를 사용하며, 각 task는 cold state로 시작합니다.
L1 miss만 LLC로 전달하고 load/store에 같은 residency 규칙을 적용하는 추상 모델입니다.
지원 입력과 모델 제약은 [모델 계약](docs/cache-hierarchy-rd-model-v1.md)을 따릅니다.

## 상세 문서

- [Backend CLI·API와 기존 RDH/mapping 사용법](backend/README.md)
- [LLVM frontend·annotation·MAP schema](frontend/README.md)
- [선택 구간 frontend 빌드와 사용법](frontend/docs/analysis-regions.md)
- [RESULT v2 지표·JSON 계약](docs/cache-hierarchy-artifacts-v2.md)
- [계층 평가 실행 가이드](backend/experiments/README.md)
- [평가 결과와 적용 범위](docs/cache-hierarchy-evaluation.md)
