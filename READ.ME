# ThreadPool Benchmark Runner

이 프로젝트는 POSIX pthread 기반 Fixed Thread Pool의 성능을 측정하기 위한 실험용 benchmark runner입니다.

Fixed ThreadPool에 대해 다양한 workload와 submit pattern을 적용하고, task별 실행 시간을 기록하여 throughput, latency, queue waiting time, service time, spike recovery time 등을 측정합니다.

---

## 1. 프로젝트 구조

```text
.
├── ThreadPool.h
├── ThreadPool.cpp
├── main.cpp
└── README.md
```

### ThreadPool.h

ThreadPool 클래스와 task 구조체를 선언합니다.

주요 구성 요소:

- `Task`
- `TaskMetric`
- `ThreadPool`
- `submit()`
- `wait_all()`
- `reset_counter()`
- `get_queue_length()`
- `shutdown()`

### ThreadPool.cpp

ThreadPool의 실제 동작을 구현합니다.

주요 기능:

- worker thread 생성
- task queue 관리
- condition variable 기반 worker 대기/깨우기
- task 실행
- task별 `start_time`, `end_time` 기록
- 모든 task 완료 대기
- queue length 조회
- thread pool shutdown

### main.cpp

실험을 실행하는 benchmark runner입니다.

주요 기능:

- 명령행 인자 파싱
- workload 선택
- submit pattern 선택
- warm-up 실행
- 본 실험 실행
- task별 metric 계산
- queue length sampling
- CSV 결과 저장

---

## 2. Build 방법

Ubuntu 환경에서 다음 명령어로 컴파일합니다.

```bash
g++ -O2 -pthread main.cpp ThreadPool.cpp -o pool_runner
```

### 컴파일 옵션 설명

| 옵션 | 의미 |
|---|---|
| `g++` | C++ 컴파일러 |
| `-O2` | 최적화 옵션 |
| `-pthread` | POSIX thread 사용 |
| `main.cpp ThreadPool.cpp` | 컴파일 대상 파일 |
| `-o pool_runner` | 실행 파일 이름 지정 |

---

## 3. 실행 방법

기본 실행 형식은 다음과 같습니다.

```bash
./pool_runner <thread_count> <workload> <pattern> <task_count> <warmup_count> <csv_file>
```

### 인자 설명

| 인자 | 설명 | 예시 |
|---|---|---|
| `thread_count` | Fixed ThreadPool의 worker thread 개수 | `4`, `8`, `16` |
| `workload` | task 하나의 작업 유형 | `cpu`, `io`, `mixed` |
| `pattern` | task 제출 패턴 | `burst`, `steady`, `spike` |
| `task_count` | 본 실험에서 실행할 task 수 | `10000` |
| `warmup_count` | 측정 전 미리 실행할 warm-up task 수 | `1000` |
| `csv_file` | 결과를 저장할 CSV 파일명 | `result.csv` |

---

## 4. 실행 예시

### CPU workload + spike pattern

```bash
./pool_runner 4 cpu spike 10000 1000 result.csv
./pool_runner 8 cpu spike 10000 1000 result.csv
./pool_runner 16 cpu spike 10000 1000 result.csv
```

### I/O workload + spike pattern

```bash
./pool_runner 4 io spike 10000 1000 result.csv
./pool_runner 8 io spike 10000 1000 result.csv
./pool_runner 16 io spike 10000 1000 result.csv
```

### Mixed workload + spike pattern

```bash
./pool_runner 4 mixed spike 10000 1000 result.csv
./pool_runner 8 mixed spike 10000 1000 result.csv
./pool_runner 16 mixed spike 10000 1000 result.csv
```

### Burst pattern

```bash
./pool_runner 4 cpu burst 10000 1000 result.csv
```

### Steady pattern

```bash
./pool_runner 4 cpu steady 10000 1000 result.csv
```

---

## 5. Workload 종류

workload는 task 하나가 어떤 작업을 수행하는지를 의미합니다.

### 5.1 CPU workload

```text
cpu
```

CPU 계산이 많은 작업입니다.

코드에서는 반복문을 이용해 CPU 연산을 수행합니다.

목적:

- CPU-bound workload에서 thread 수 증가가 성능 향상으로 이어지는지 확인
- thread 수가 너무 많을 때 context switching overhead가 발생하는지 확인

예상 분석 방향:

- CPU core 수보다 thread 수가 지나치게 많으면 성능이 오히려 악화될 수 있습니다.
- Fixed thread pool의 안정성을 확인하기 좋습니다.

---

### 5.2 I/O workload

```text
io
```

I/O 대기 상황을 흉내 내는 작업입니다.

코드에서는 `usleep(10000)`을 사용하여 10ms 대기합니다.

목적:

- thread가 실제로 CPU를 쓰지 않고 대기하는 상황에서 thread 수 증가가 throughput에 미치는 영향 확인

예상 분석 방향:

- I/O-bound workload에서는 thread 수가 많을수록 throughput이 좋아질 수 있습니다.
- Adaptive ThreadPool과 비교할 때 의미 있는 workload입니다.

---

### 5.3 Mixed workload

```text
mixed
```

CPU 연산과 I/O 대기가 섞인 작업입니다.

코드에서는 짧은 CPU 연산 후 `usleep(5000)`을 수행합니다.

목적:

- 현실적인 서버 workload와 비슷한 상황을 구성
- CPU-bound와 I/O-bound의 중간 성격 분석

예상 분석 방향:

- Fixed와 Adaptive ThreadPool의 trade-off를 관찰하기 좋습니다.

---

## 6. Submit Pattern 종류

submit pattern은 task가 thread pool에 어떤 방식으로 들어오는지를 의미합니다.

workload가 “task 하나의 성격”이라면, pattern은 “task 유입 방식”입니다.

### 6.1 Burst pattern

```text
burst
```

모든 task를 빠르게 연속으로 submit합니다.

```text
task 10000개를 거의 한 번에 submit
```

목적:

- 순간적으로 많은 요청이 들어왔을 때 queue waiting time과 latency 확인

예상 분석 방향:

- thread 수가 적으면 queue waiting time이 커질 수 있습니다.
- thread 수가 많으면 throughput이 증가할 수 있지만, CPU workload에서는 overhead가 발생할 수 있습니다.

---

### 6.2 Steady pattern

```text
steady
```

task를 일정한 간격으로 submit합니다.

현재 코드는 task를 1ms 간격으로 submit합니다.

```text
submit → 1ms 대기 → submit → 1ms 대기 → ...
```

목적:

- 안정적인 요청 유입 상황에서 thread pool 성능 확인

예상 분석 방향:

- queue가 크게 쌓이지 않는 상황에서 latency가 안정적인지 확인할 수 있습니다.

---

### 6.3 Spike pattern

```text
spike
```

정상 부하 → 순간 폭주 → 정상 부하 형태로 task를 submit합니다.

현재 코드는 전체 task를 다음 비율로 나눕니다.

```text
normal before : 20%
spike         : 60%
normal after  : 20%
```

예를 들어 `task_count = 10000`이면 다음과 같습니다.

```text
normal before : 2000개, 5ms 간격 submit
spike         : 6000개, sleep 없이 즉시 submit
normal after  : 2000개, 5ms 간격 submit
```

목적:

- 갑작스러운 부하 증가 상황에서 thread pool이 얼마나 잘 버티는지 확인
- spike 이후 queue가 얼마나 빨리 안정화되는지 확인

예상 분석 방향:

- Fixed ThreadPool은 thread 수가 고정되어 예측 가능성이 높습니다.
- 하지만 spike 상황에서는 queue length와 latency가 크게 증가할 수 있습니다.
- Adaptive ThreadPool과 비교할 때 recovery time, max queue length, p95 latency가 중요한 지표가 됩니다.

---

## 7. Warm-up

본 실험 전에 warm-up task를 먼저 실행합니다.

예시:

```bash
./pool_runner 4 cpu spike 10000 1000 result.csv
```

위 명령어에서 `1000`은 warm-up task 수입니다.

Warm-up은 다음과 같은 cold start 영향을 줄이기 위해 사용합니다.

- thread 초기 실행 비용
- CPU cache 초기 상태
- 메모리 할당 초기 비용
- 동적 라이브러리 로딩 영향
- 프로그램 초반 실행 편차

중요한 점:

```text
warm-up 결과는 CSV metric에 포함하지 않습니다.
```

실험 흐름은 다음과 같습니다.

```text
1. warm-up task 실행
2. warm-up task 완료 대기
3. counter 초기화
4. 본 실험 시작 시간 기록
5. 본 실험 task 실행
6. metric 계산
```

---

## 8. 측정 Metric

본 실험에서는 task마다 다음 세 가지 timestamp를 기록합니다.

| Timestamp | 의미 |
|---|---|
| `submit_time_ns` | main thread가 task를 queue에 넣은 시간 |
| `start_time_ns` | worker thread가 task를 꺼내 실행을 시작한 시간 |
| `end_time_ns` | worker thread가 task 실행을 끝낸 시간 |

이를 기반으로 여러 metric을 계산합니다.

---

## 9. Metric 의미

### 9.1 Queue Waiting Time

```text
queue_waiting_time = start_time - submit_time
```

task가 queue 안에서 기다린 시간입니다.

의미:

- worker thread가 부족하면 queue waiting time이 증가합니다.
- burst나 spike 상황에서 중요합니다.
- Adaptive ThreadPool이 효과적이라면 spike 상황에서 이 값이 줄어들 수 있습니다.

### 9.2 Service Time

```text
service_time = end_time - start_time
```

worker thread가 실제로 task를 수행한 시간입니다.

의미:

- workload 자체의 실행 비용을 나타냅니다.
- CPU workload에서는 계산 시간입니다.
- I/O workload에서는 sleep 시간이 포함됩니다.

### 9.3 Latency

```text
latency = end_time - submit_time
```

task가 submit된 시점부터 완료될 때까지 걸린 전체 시간입니다.

관계식:

```text
latency = queue_waiting_time + service_time
```

의미:

- 사용자 관점에서 task 하나가 완료되기까지 걸린 시간입니다.
- 서버 성능 분석에서 중요한 지표입니다.

### 9.4 Total Time

```text
total_time = experiment_end_time - experiment_start_time
```

본 실험 전체 task가 모두 완료되는 데 걸린 시간입니다.

주의:

```text
warm-up 시간은 포함하지 않습니다.
```

### 9.5 Throughput

```text
throughput = task_count / total_time_seconds
```

초당 처리한 task 수입니다.

의미:

- 처리량을 나타냅니다.
- 값이 클수록 같은 시간에 더 많은 task를 처리했다는 뜻입니다.

### 9.6 P95 Latency

전체 latency를 정렬했을 때 95% 지점의 값입니다.

의미:

```text
전체 task 중 95%가 이 시간 안에 완료되었다.
```

평균 latency만 보면 일부 매우 느린 task를 놓칠 수 있습니다.

예를 들어:

```text
avg_latency = 10ms
p95_latency = 300ms
```

라면 대부분 task는 빠르지만, 일부 task는 매우 느리다는 뜻입니다.

### 9.7 Max Latency

가장 오래 걸린 task의 latency입니다.

의미:

- 최악의 요청 지연 시간 확인
- spike 상황에서 매우 중요

### 9.8 P95 Queue Waiting Time

전체 queue waiting time을 정렬했을 때 95% 지점의 값입니다.

의미:

- 대부분의 task가 queue에서 얼마나 기다렸는지 확인
- thread pool이 요청 폭주를 얼마나 잘 흡수하는지 분석 가능

### 9.9 Max Queue Waiting Time

가장 오래 queue에서 기다린 task의 대기 시간입니다.

의미:

- 최악의 queue 적체 상황 확인

### 9.10 Max Queue Length

실험 중 관측된 최대 queue 길이입니다.

현재 코드는 1ms마다 queue length를 sampling합니다.

의미:

- spike 상황에서 queue가 얼마나 많이 쌓였는지 확인
- Fixed ThreadPool과 Adaptive ThreadPool 비교 시 중요한 지표

### 9.11 Recovery Time

```text
recovery_time_ms
```

현재 코드에서는 다음과 같이 정의합니다.

```text
spike 구간 submit이 끝난 뒤,
queue_length가 처음으로 0이 되는 데 걸린 시간
```

의미:

- spike 이후 thread pool이 정상 상태로 회복되는 데 걸린 시간
- 값이 작을수록 spike를 빠르게 처리했다는 뜻입니다.

주의:

```text
recovery_time_ms = -1
```

이 나오면 샘플링 기간 동안 queue length가 0으로 돌아온 순간을 찾지 못했다는 뜻입니다.

---

## 10. CSV 출력 컬럼

결과 CSV에는 다음 컬럼이 저장됩니다.

```csv
pool_type,thread_count,workload,pattern,task_count,warmup_count,total_time_ms,throughput,avg_latency_ms,p95_latency_ms,max_latency_ms,avg_queue_wait_ms,p95_queue_wait_ms,max_queue_wait_ms,avg_service_time_ms,spike_avg_latency_ms,spike_p95_latency_ms,spike_avg_queue_wait_ms,spike_p95_queue_wait_ms,normal_after_avg_latency_ms,normal_after_p95_latency_ms,max_queue_length,recovery_time_ms
```

### 컬럼 설명

| 컬럼 | 의미 |
|---|---|
| `pool_type` | thread pool 종류. 현재는 `fixed` |
| `thread_count` | worker thread 개수 |
| `workload` | `cpu`, `io`, `mixed` |
| `pattern` | `burst`, `steady`, `spike` |
| `task_count` | 본 실험 task 수 |
| `warmup_count` | warm-up task 수 |
| `total_time_ms` | 전체 task 완료 시간 |
| `throughput` | 초당 처리 task 수 |
| `avg_latency_ms` | 평균 latency |
| `p95_latency_ms` | p95 latency |
| `max_latency_ms` | 최대 latency |
| `avg_queue_wait_ms` | 평균 queue waiting time |
| `p95_queue_wait_ms` | p95 queue waiting time |
| `max_queue_wait_ms` | 최대 queue waiting time |
| `avg_service_time_ms` | 평균 service time |
| `spike_avg_latency_ms` | spike 구간 task의 평균 latency |
| `spike_p95_latency_ms` | spike 구간 task의 p95 latency |
| `spike_avg_queue_wait_ms` | spike 구간 task의 평균 queue waiting time |
| `spike_p95_queue_wait_ms` | spike 구간 task의 p95 queue waiting time |
| `normal_after_avg_latency_ms` | spike 이후 정상 구간의 평균 latency |
| `normal_after_p95_latency_ms` | spike 이후 정상 구간의 p95 latency |
| `max_queue_length` | 실험 중 최대 queue 길이 |
| `recovery_time_ms` | spike 이후 queue가 0이 되는 데 걸린 시간 |

---

## 11. 실험 추천 방법

처음에는 작은 task 수로 정상 동작을 확인합니다.

```bash
./pool_runner 4 cpu spike 100 10 result.csv
```

정상 동작이 확인되면 task 수를 늘립니다.

```bash
./pool_runner 4 cpu spike 10000 1000 result.csv
```

Fixed ThreadPool 비교를 위해 다음과 같이 실행합니다.

```bash
./pool_runner 4 cpu spike 10000 1000 result.csv
./pool_runner 8 cpu spike 10000 1000 result.csv
./pool_runner 16 cpu spike 10000 1000 result.csv
```

workload별로도 반복합니다.

```bash
./pool_runner 4 io spike 10000 1000 result.csv
./pool_runner 8 io spike 10000 1000 result.csv
./pool_runner 16 io spike 10000 1000 result.csv

./pool_runner 4 mixed spike 10000 1000 result.csv
./pool_runner 8 mixed spike 10000 1000 result.csv
./pool_runner 16 mixed spike 10000 1000 result.csv
```

---

## 12. 반복 실험

성능 측정은 실행할 때마다 값이 조금씩 달라질 수 있습니다.

따라서 동일한 실험을 여러 번 반복하고 평균과 표준편차를 계산하는 것이 좋습니다.

예시 반복 횟수:

```text
개발 확인용: 5회
1차 분석용: 30회
최종 분석용: 100회 이상
```

---

## 13. Shell Script 예시

반복 실행을 위해 다음과 같은 script를 사용할 수 있습니다.

파일명:

```bash
run_fixed_tests.sh
```

내용:

```bash
#!/bin/bash

g++ -O2 -pthread main.cpp ThreadPool.cpp -o pool_runner

CSV="result.csv"

rm -f $CSV

TASKS=10000
WARMUP=1000
REPEAT=30

for i in $(seq 1 $REPEAT)
do
    echo "Run $i / $REPEAT"

    ./pool_runner 4 cpu spike $TASKS $WARMUP $CSV
    ./pool_runner 8 cpu spike $TASKS $WARMUP $CSV
    ./pool_runner 16 cpu spike $TASKS $WARMUP $CSV

    ./pool_runner 4 io spike $TASKS $WARMUP $CSV
    ./pool_runner 8 io spike $TASKS $WARMUP $CSV
    ./pool_runner 16 io spike $TASKS $WARMUP $CSV

    ./pool_runner 4 mixed spike $TASKS $WARMUP $CSV
    ./pool_runner 8 mixed spike $TASKS $WARMUP $CSV
    ./pool_runner 16 mixed spike $TASKS $WARMUP $CSV
done

echo "All experiments finished."
echo "Result saved to $CSV"
```

실행 권한 부여:

```bash
chmod +x run_fixed_tests.sh
```

실행:

```bash
./run_fixed_tests.sh
```

---

## 14. Fixed ThreadPool 분석 방향

Fixed ThreadPool은 thread 수가 고정되어 있습니다.

장점:

- 구조가 단순함
- 예측 가능성이 높음
- thread 생성/삭제 overhead가 없음
- 안정성 분석에 유리함

단점:

- workload 변화에 유연하게 대응하기 어려움
- spike 상황에서 queue가 길어질 수 있음
- thread 수가 너무 작으면 latency 증가
- thread 수가 너무 크면 context switching overhead 가능

---

## 15. Adaptive ThreadPool과 비교할 때 볼 지표

Adaptive ThreadPool과 비교할 때는 다음 metric이 중요합니다.

| 비교 지표 | 분석 의미 |
|---|---|
| `throughput` | Adaptive가 더 많은 task를 처리했는가 |
| `p95_latency_ms` | tail latency가 줄었는가 |
| `p95_queue_wait_ms` | queue 적체가 줄었는가 |
| `max_queue_length` | spike 때 queue가 덜 쌓였는가 |
| `recovery_time_ms` | spike 이후 더 빨리 회복했는가 |
| `avg_service_time_ms` | workload 자체 실행 시간은 비슷한가 |

Adaptive 쪽에서는 추가로 다음 metric을 측정하면 좋습니다.

| Adaptive 전용 지표 | 의미 |
|---|---|
| `max_thread_count` | 실험 중 최대 thread 수 |
| `thread_create_count` | 새로 생성한 thread 수 |
| `thread_destroy_count` | 제거한 thread 수 |
| `resize_count` | thread 수 조절 횟수 |

---

## 16. 주의사항

1. 동일한 실험 환경에서 실행해야 합니다.

```text
같은 Ubuntu VirtualBox 환경
같은 CPU core 수
같은 RAM
같은 task_count
같은 warmup_count
같은 workload
같은 pattern
```

2. 결과는 한 번만 보고 판단하지 않습니다.

```text
여러 번 반복 실행 후 평균과 표준편차를 확인해야 합니다.
```

3. warm-up 결과는 본 실험에 포함하지 않습니다.

4. CPU workload에서는 thread 수가 많다고 무조건 좋은 것은 아닙니다.

5. I/O workload에서는 thread 수 증가가 throughput 향상으로 이어질 수 있습니다.

6. spike pattern에서는 평균보다 p95, max, recovery time을 함께 봐야 합니다.

---

## 17. 실험 목표

이 benchmark의 목적은 단순히 어떤 thread pool이 빠른지 확인하는 것이 아닙니다.

핵심 목표는 다음과 같습니다.

```text
어떤 workload와 submit pattern에서 Fixed ThreadPool이 안정적인지,
어떤 상황에서 Adaptive ThreadPool이 더 유리할 수 있는지,
그리고 그 trade-off가 어떤 metric에서 나타나는지 분석한다.
```

특히 다음 질문에 답하는 것을 목표로 합니다.

```text
1. CPU-bound workload에서는 thread 수 증가가 항상 유리한가?
2. I/O-bound workload에서는 thread 수가 많을수록 throughput이 좋아지는가?
3. spike 상황에서 Fixed ThreadPool은 queue 적체를 얼마나 겪는가?
4. spike 이후 recovery time은 thread 수에 따라 어떻게 달라지는가?
5. Adaptive ThreadPool은 queue waiting time과 p95 latency를 줄이는가?
6. Adaptive ThreadPool의 장점이 thread 생성/삭제 overhead보다 큰가?
```

---

## 18. Example

```bash
g++ -O2 -pthread main.cpp ThreadPool.cpp -o pool_runner

./pool_runner 4 cpu spike 10000 1000 result.csv
./pool_runner 8 cpu spike 10000 1000 result.csv
./pool_runner 16 cpu spike 10000 1000 result.csv

cat result.csv
```

예상 출력 파일:

```csv
pool_type,thread_count,workload,pattern,task_count,warmup_count,total_time_ms,throughput,avg_latency_ms,p95_latency_ms,max_latency_ms,avg_queue_wait_ms,p95_queue_wait_ms,max_queue_wait_ms,avg_service_time_ms,spike_avg_latency_ms,spike_p95_latency_ms,spike_avg_queue_wait_ms,spike_p95_queue_wait_ms,normal_after_avg_latency_ms,normal_after_p95_latency_ms,max_queue_length,recovery_time_ms
fixed,4,cpu,spike,10000,1000,2450.31,4081.11,1100.22,2200.45,2500.10,1099.10,2199.44,2499.00,1.12,1300.55,2300.66,1299.44,2299.50,900.11,1800.22,5980,1200.35
```

---

## 19. Summary

이 benchmark runner는 Fixed ThreadPool의 성능을 다음 관점에서 측정합니다.

```text
1. workload 특성
   - CPU
   - I/O
   - Mixed

2. 요청 유입 패턴
   - Burst
   - Steady
   - Spike

3. 성능 metric
   - throughput
   - latency
   - queue waiting time
   - service time
   - max queue length
   - recovery time
```

이를 통해 Fixed ThreadPool과 Adaptive ThreadPool의 성능 및 안정성 trade-off를 비교할 수 있습니다.
