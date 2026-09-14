# RTEMS 6 / GR740 ELF 기반 e2e 평가

측정일: 2026-09-14 UTC. 작업 기준: `/workspace/YARDA`.

**실제 GR740 BSP로 링크한 16개 SPARC RTEMS 입력에서 정적 e2e 검증을
통과했고, 480개 측정 RESULT가 모두 기준 결과와 바이트 단위로 일치했다.**
각 입력을 두 번 생성한 ELF와 LAT도 각각 동일했다. 이 결과는 호스트에서
수행하는 YARDA 분석의 재현성이다. **GR740 실기/시뮬레이터 실행과 물리 캐시
적중률의 일치는 검증하지 못했다.**

실행 가능한 실험은 [rtems_gr740/README.md](../backend/experiments/rtems_gr740/README.md),
이번 원본 증거는 [실험 archive](../benchmark-results/rtems-gr740-20260914/README.md)에 있다.
기존 B11/B12 archive는 변경하지 않았다.

## 1. 대상과 모델 경계

같은 커널 C 파일을 두 경로에 전달했다.

```text
kernel C -> Clang 14 strict region, SPARC target -> LAT v2
         -> sparc-rtems6-gcc + RTEMS 6 GR740 BSP -> SPARC ELF32 ET_EXEC
LAT + ELF의 실제 링크 주소 + cache-model.yaml
         -> 실제 yarda_cpp / batch / streaming / instrumented -> RESULT v2
```

- 분석기: 현재 HEAD `51b20ca3e83e5f582ee5e13e0ba1ad1cf943de12`에 실험 추가,
  Release `-O2 -DNDEBUG`. Frontend `3cc6f9276916693c915b20aa412966c2c35bf101`.
- 입력: `sparc-rtems6-gcc 13.3.0`, GNU ld 2.43, `/opt/rtems/6`의 GR740 BSP.
  `/workspace/experiments/common.mk`와 설치된 pkg-config의 ABI/링크 옵션을 적용했다.
  GCC는 `-O0 -g -mcpu=leon3 -mfpu -mhard-float`, Clang은
  `--target=sparc-unknown-rtems6`, 고정 `-O0` region 경로다.
- ELF는 big-endian, ELF32, `EM_SPARC=2`, `ET_EXEC`다. 실제 `Init`, RTEMS startup,
  커널 함수와 분석 대상 배열을 포함한다. 배열은 32-byte 정렬의 static 저장이며,
  GNU `nm`의 주소/크기와 YARDA의 ELF 해석을 대조했다.
- 실행 wrapper는 CPU 하나를 활성화하고 warm-up 1회와 측정 job 10회를 수행하도록
  구성했다. 수치 검증은 모든 출력 원소를 확인한다. 이번에는 이 wrapper의
  **RTEMS 실행 성공을 확인하지 못했으며**, 같은 커널의 호스트 수치 검증은 통과했다.

[GR740 매뉴얼 §6.3](https://www.gaisler.com/doc/gr740/GR740-UM-DS.pdf)의
L1 D-cache는 16 KiB, 4-way, 32-byte line이며 write-through/no-write-allocate다.
[보드 quick start guide](https://www.gaisler.com/doc/gr740/qsg_gr740.pdf)는
4-way, 2 MiB L2도 명시한다. 이번 YAML은 이 용량/라인/way 구성에 YARDA의
`exact-two-level-lru-demand-v1` 정책을 적용한다. 양쪽 LRU, 모든 demand miss의
allocation, L1 miss만 LLC로 전달, task별 cold start가 전제다.

**물리 GR740의 store 처리와 모델이 다르다.** 실제 L1 정책처럼
`write_allocate: false`를 지정하면 현 분석기는 거부하며, 이를 별도 negative
test로 확인했다. 아래 FHC/AMC/CSRD는 이 모델의 결과이고 GR740 하드웨어 counter가
아니다. GCC 명령어 순서·스택 spill·RTEMS 트래픽·instruction fetch·멀티코어 간섭은
분석하지 않았다. 주소는 GCC ELF에서 가져오지만 선택 구간의 접근 순서는 Clang의
LAT 계약에 따른다.

## 2. 워크로드와 검증

PolyBench/C 4.2.1의 ATAX·BiCG·MVT를 MICRO/MINI/SMALL/MEDIUM으로 구성했다.
원본 MINI/SMALL/MEDIUM 크기는 유지했다. MVT `N=520`은 LLC 용량을 넘기기 위한
별도 custom 크기다. ATAX/BiCG는 행렬을 순차 접근하며 벡터를 갱신하고,
MVT는 전치 방향 접근도 포함한다. 낮은 연산 집약도와 메모리 접근 패턴을 선택
근거로 삼았으며, 실행하지 못한 GR740에서 bandwidth-bound임을 실측한 것은 아니다.

원본의 동적 할당과 함수 인자 배열을 static 객체로 바꾸고 bound를 컴파일 상수로
고정했다. 초기화·검증·출력은 선택 구간 밖이다. `A[i][j]=i+2*j+1`, 입력 벡터는
1로 초기화해 행/열 접근이 구분되며 모든 출력의 정확한 닫힌형 기대값을 계산할
수 있게 했다. 이 데이터 크기의 정수 연산 결과는 double의 정확한 정수 범위 안이다.
호스트 검사에서는 두 번 초기화·실행하여 모든 출력 원소를 확인했다.

추가 sweep은 같은 주소의 4,096개 라인, 128 KiB를 1/16/128회 순회한다.
세 ELF 모두 `lines`의 base는 `0x23840`이고 size는 131,072 bytes다.
따라서 이 비교에서는 접근 횟수만 증가하고 주소 domain은 유지된다.

| 검증 | 결과 |
| --- | --- |
| Release build / 전체 CTest | 921/921 통과 |
| 기존 Python pytest | 100/100 통과 |
| SPARC ELF와 LAT를 각각 두 번 빌드 | 16/16 case에서 각각 bytes 동일 |
| 독립 C 식 기반 모든 source 접근 대조 | 16/16 통과; object/offset/width/operation/order/address |
| 독립 resident-LRU oracle의 모든 first-hit event | 16/16 통과 |
| batch/streaming 전체 event 및 CSRD 대조 | 16/16 통과 |
| 독립 source의 batch RESULT와 실제 CLI | 16/16 통과 |
| quadratic exact-distance oracle | micro 3개 통과; 큰 13개에는 적용하지 않음 |
| CLI 반복/diagnostics 추가 시 RESULT 동일성 | 16/16 통과 |
| events limit=16, truncation, telemetry ID/수/loop 수 | 16/16 통과 |
| 정상 정식 측정 RESULT | 480/480 bytes 동일 |
| 네 예산을 정확한 한도에서 성공 / 하나 낮춰 실패 | 16개 정상 입력의 경계 성공; ATAX micro의 4×3 mode 실패 확인 |
| 실패 시 기존 CLI RESULT 보존 | 네 예산 각각 통과 |
| no-write-allocate, missing SPARC symbol, dynamic bound | 기대한 거부, 정상 RESULT/LAT 없음 |
| LAT store를 load로 변조 | CLI는 분석 성공; 독립 source oracle은 기대대로 실패 |
| Valgrind: micro verifier, MVT SMALL CLI, budget failure | 세 경로 모두 memory errors 0, definite/indirect/possible lost 0 |

Valgrind의 LLVM command-line registry는 기존과 같은 still-reachable
2,574 bytes / 34 blocks를 남겼다. Budget failure의 정상 애플리케이션 종료 코드
1과 Valgrind 오류 코드 99를 구별했다. 새로운 C/C++/Python 코드 파일은 모두
300줄 이하이며 기존 분석 코어와 frontend는 변경하지 않았다.

## 3. 정식 측정 결과

각 case/mode는 별도 process로 warm-up 1회 후 10회 측정했다. 순서는
batch/streaming/instrumented와 그 역순을 교차했다. 총 48 warm-up과 480 formal
sample이며, 컴파일·oracle 검증·시뮬레이터 시도·Valgrind는 측정 밖에서 실행했다.
Sample별 timeout은 180초, 가상 주소 공간 한도는 4 GiB다. 실패한 정상 sample은 없다.

아래 시간은 입력/실행 파일 hashing, 파싱, 준비, 분석, RESULT 직렬화를 포함하는
**호스트 analyzer total 시간의 중앙값**이다. 컴파일 시간과 target job 시간은
포함하지 않는다. RSS는 해당 실행 이미지의 Linux `/proc/self/status` VmHWM이며
launcher에서 물려받은 `getrusage` 고수위와 구별한다. 각 분포의 min/max/IQR,
analysis-only 시간과 instrumented 결과는 [CSV](../benchmark-results/rtems-gr740-20260914/measurement-table.csv)와
[summary.json](../benchmark-results/rtems-gr740-20260914/summary.json)에 있다.

| Case | Working set KiB | MA | FHC L1 | FHC LLC | AMC | Batch ms | Streaming ms | Batch MiB | Streaming MiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| atax-micro | 0.11 | 53 | 48 | 0 | 5 | 54.372 | 54.674 | 8.062 | 7.875 |
| bicg-micro | 0.12 | 53 | 47 | 0 | 6 | 53.601 | 53.163 | 8.062 | 8.062 |
| mvt-micro | 0.16 | 72 | 65 | 0 | 7 | 53.110 | 53.319 | 7.969 | 7.969 |
| atax-mini | 13.42 | 12848 | 12417 | 0 | 431 | 58.244 | 54.358 | 15.686 | 8.250 |
| bicg-mini | 13.72 | 12848 | 12407 | 0 | 441 | 58.265 | 54.819 | 15.611 | 8.250 |
| mvt-mini | 13.75 | 12800 | 12360 | 0 | 440 | 58.885 | 54.849 | 15.664 | 8.250 |
| atax-small | 115.22 | 115312 | 111625 | 0 | 3687 | 103.722 | 67.102 | 77.254 | 10.031 |
| bicg-small | 116.12 | 115312 | 111596 | 0 | 3716 | 99.753 | 66.251 | 77.324 | 10.125 |
| mvt-small | 116.25 | 115200 | 107904 | 3576 | 3720 | 103.351 | 67.190 | 77.016 | 10.125 |
| atax-medium | 1258.67 | 1280000 | 1239662 | 59 | 40279 | 879.043 | 214.789 | 763.416 | 19.594 |
| bicg-medium | 1261.72 | 1280000 | 1239623 | 0 | 40377 | 861.611 | 216.526 | 765.551 | 19.875 |
| mvt-medium | 1262.50 | 1280000 | 1068531 | 171069 | 40400 | 924.506 | 255.230 | 807.145 | 19.688 |
| mvt-llc-crossing-custom | 2128.75 | 2163200 | 1789800 | 294712 | 78688 | 1747.622 | 395.818 | 1338.145 | 22.312 |
| sweep-v4096-r1 | 128.00 | 8192 | 4096 | 0 | 4096 | 58.950 | 56.002 | 14.635 | 10.125 |
| sweep-v4096-r16 | 128.00 | 131072 | 65536 | 61440 | 4096 | 112.229 | 74.026 | 97.812 | 10.312 |
| sweep-v4096-r128 | 128.00 | 1048576 | 524288 | 520192 | 4096 | 608.149 | 199.531 | 687.500 | 10.312 |

MA는 모델의 line reference 수다. 이번 정렬된 double 접근에서는 source count와
같다. Working set은 분석 대상 배열 크기의 합이며 RTEMS 전체 RAM 사용량이 아니다.

![호스트 시간과 RSS 비교](../benchmark-results/rtems-gr740-20260914/host-analysis.png)

[PDF 그림](../benchmark-results/rtems-gr740-20260914/host-analysis.pdf).
막대는 중앙값, 오차 막대는 min/max이며 RSS 축은 로그다.

큰 MVT에서 batch → streaming은 total 1,747.622 → 395.818 ms,
RSS 1,338.145 → 22.312 MiB였다. RSS 감소는 98.33%이고 total 중앙값 비는 4.42배다.
ATAX MEDIUM에서도 763.416 → 19.594 MiB, 879.043 → 214.789 ms였다.
이는 이번 호스트와 입력에 대한 관측이다.

RTEMS debug ELF 등의 input/hash 단계 중앙값이 약 53 ms이므로 micro의 약 54 ms를
커널 분석 자체의 비용으로 해석하면 안 된다. 예를 들어 큰 MVT의 analysis-only
중앙값은 batch 1,693.727 ms, streaming 341.674 ms다. 호스트는 x86-64 WSL2 Linux
`6.6.87.2-microsoft-standard-WSL2`, GNU C++ 11.4.0이며 CPU affinity/governor와
외부 부하는 통제하지 않았다. 큰 batch sample의 시간 편차도 그대로 공개했다.

## 4. 고정 주소 domain의 상태 크기

| Case | References | L1/LLC history | L1/LLC Fenwick slots | Compactions | Compaction ms | Streaming analysis ms | Instrumented analysis ms |
| --- | ---: | --- | --- | ---: | ---: | ---: | ---: |
| sweep-v4096-r1 | 8192 | 4096/4096 | 6912/65536 | 512 | 0.149 | 2.287 | 2.471 |
| sweep-v4096-r16 | 131072 | 4096/4096 | 8192/65536 | 4352 | 1.626 | 19.661 | 19.520 |
| sweep-v4096-r128 | 1048576 | 4096/4096 | 8192/65536 | 65792 | 13.998 | 146.474 | 149.174 |

References가 128배 증가해도 L1/LLC historical distinct-line 이력은 각각 4,096개다.
Streaming RSS 중앙값은 10.125 → 10.312 MiB, batch는 14.635 → 687.500 MiB였다.
Fenwick capacity의 초기 성장과 compaction 비용은 존재한다. Compaction 시간은
instrumented analysis 안에 포함되며 total에 다시 더하면 안 된다. 계측 경로의
중앙값이 일부 구간에서 더 낮은 것은 단일 실행의 계측 비용이 음수라는 의미가
아니며, 호스트 잡음과 process 분포를 고려해야 한다.

이 결과는 B12의 historical-state `O(V)` 주장과 부합한다. 캐시 associativity만큼의
상수 메모리, 전체 process RSS의 수학적 상한, 모든 워크로드에서의 속도 우위,
GR740 실기 속도 향상을 입증하지는 않는다.

## 5. RTEMS 실행 상태와 재현 자료

다음 실제 이미지를 pseudo-terminal을 붙여 시뮬레이터에 전달했다.

```sh
/workspace/laysim-gr740/laysim-gr740-cli -r -core0 \
  /workspace/YARDA/benchmark-results/rtems-gr740-20260914/inputs/atax-micro/input.elf
```

프로세스는 `Gtk-WARNING: cannot open display`로 exit 1했고,
`YARDA_RTEMS_COMPLETE`에 도달하지 못했다. RTEMS 실행 여부는
[target-runtime.json](../benchmark-results/rtems-gr740-20260914/target-runtime.json)에
`unavailable`로 보존했다. 이 기록으로 부팅 성공·target 수치 검증·실행 시간·cache
counter를 주장하지 않는다. 시뮬레이터와 display 환경을 준비하면 동일 ELF와
동일 실행 wrapper로 이어서 확인할 수 있다. 물리 cache counter와의 비교에는
별도로 현 모델과 실제 정책 차이를 처리해야 한다.

이번 정식 실행 명령:

```sh
python3 backend/experiments/rtems_gr740/run.py \
  /tmp/yarda-rtems-gr740-20260914/build \
  benchmark-results/rtems-gr740-20260914 \
  --simulator /workspace/laysim-gr740/laysim-gr740-cli
```

새 실행은 다른 출력 디렉터리를 사용한다. Git stage/commit은 수행하지 않았다.
로컬 `benchmark-results/`와 `/tmp`는 Git clone에 포함되지 않으므로 원본 증거가
필요한 공유/논문 재현에서는 archive도 함께 보존해야 한다.
