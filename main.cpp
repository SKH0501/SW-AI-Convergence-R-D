#include "ThreadPool.h"

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <unistd.h>

using namespace std;

enum class WorkloadType {
    CPU,
    IO,
    MIXED
};

enum class SubmitPattern {
    STEADY,
    SPIKE,
    BURST
};

enum TaskPhase {
    PHASE_ALL = 0,

    PHASE_NORMAL_1 = 1,
    PHASE_SPIKE = 2,
    PHASE_NORMAL_2 = 3,

    PHASE_BURST_1 = 4,
    PHASE_BURST_2 = 5
};

struct WorkloadArg {
    WorkloadType type;
};

struct SubmitInfo {
    long long overload_start_ns = 0;
    long long overload_end_ns = 0;
};

long long now_ns() {
    return chrono::duration_cast<chrono::nanoseconds>(
        chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void workload_function(void* arg) {
    WorkloadArg* warg = static_cast<WorkloadArg*>(arg);

    if (warg->type == WorkloadType::CPU) {
        volatile long long sum = 0;

        for (long long i = 0; i < 1000000; i++) {
            sum += i % 7;
        }
    }
    else if (warg->type == WorkloadType::IO) {
        usleep(10000); // 10ms
    }
    else if (warg->type == WorkloadType::MIXED) {
        volatile long long sum = 0;

        for (long long i = 0; i < 300000; i++) {
            sum += i % 7;
        }

        usleep(5000); // 5ms
    }
}

WorkloadType parse_workload(const string& name) {
    if (name == "cpu") {
        return WorkloadType::CPU;
    }

    if (name == "io") {
        return WorkloadType::IO;
    }

    if (name == "mixed") {
        return WorkloadType::MIXED;
    }

    cerr << "Unknown workload: " << name << "\n";
    exit(1);
}

SubmitPattern parse_pattern(const string& name) {
    if (name == "steady") {
        return SubmitPattern::STEADY;
    }

    if (name == "spike") {
        return SubmitPattern::SPIKE;
    }

    if (name == "burst") {
        return SubmitPattern::BURST;
    }

    cerr << "Unknown pattern: " << name << "\n";
    exit(1);
}

double average_ms(const vector<long long>& values_ns) {
    if (values_ns.empty()) {
        return 0.0;
    }

    long double sum = 0;

    for (long long value : values_ns) {
        sum += value / 1000000.0;
    }

    return static_cast<double>(sum / values_ns.size());
}

double percentile_ms(vector<long long> values_ns, double percentile) {
    if (values_ns.empty()) {
        return 0.0;
    }

    sort(values_ns.begin(), values_ns.end());

    int index = static_cast<int>((percentile / 100.0) * values_ns.size());

    if (index >= static_cast<int>(values_ns.size())) {
        index = static_cast<int>(values_ns.size()) - 1;
    }

    return values_ns[index] / 1000000.0;
}

double max_ms(const vector<long long>& values_ns) {
    if (values_ns.empty()) {
        return 0.0;
    }

    long long max_value = *max_element(values_ns.begin(), values_ns.end());

    return max_value / 1000000.0;
}

void write_csv_header_if_needed(const string& filename) {
    ifstream fin(filename);

    if (fin.good()) {
        return;
    }

    ofstream fout(filename);

    fout << "pool_type,"
         << "thread_count,"
         << "workload,"
         << "pattern,"
         << "task_count,"
         << "warmup_count,"
         << "total_time_ms,"
         << "throughput,"
         << "avg_latency_ms,"
         << "p95_latency_ms,"
         << "max_latency_ms,"
         << "avg_queue_wait_ms,"
         << "p95_queue_wait_ms,"
         << "max_queue_wait_ms,"
         << "avg_service_time_ms,"
         << "overload_avg_latency_ms,"
         << "overload_p95_latency_ms,"
         << "overload_avg_queue_wait_ms,"
         << "overload_p95_queue_wait_ms,"
         << "normal_after_avg_latency_ms,"
         << "normal_after_p95_latency_ms"
         << "\n";
}

void collect_metric_by_phases(
    const vector<TaskMetric>& metrics,
    const vector<int>& phases,
    const vector<int>& target_phases,
    vector<long long>& latencies,
    vector<long long>& queue_waits,
    vector<long long>& service_times
) {
    int task_count = static_cast<int>(metrics.size());

    for (int i = 0; i < task_count; i++) {
        bool matched = false;

        for (int phase : target_phases) {
            if (phases[i] == phase) {
                matched = true;
                break;
            }
        }

        if (!matched) {
            continue;
        }

        long long submit = metrics[i].submit_time_ns.load();
        long long start = metrics[i].start_time_ns.load();
        long long end = metrics[i].end_time_ns.load();

        if (submit == 0 || start == 0 || end == 0) {
            continue;
        }

        long long queue_wait = start - submit;
        long long service_time = end - start;
        long long latency = end - submit;

        queue_waits.push_back(queue_wait);
        service_times.push_back(service_time);
        latencies.push_back(latency);
    }
}

SubmitInfo submit_tasks_by_pattern(
    ThreadPool& pool,
    SubmitPattern pattern,
    void (*function)(void*),
    void* arg,
    vector<TaskMetric>& metrics,
    vector<int>& phases
) {
    int task_count = static_cast<int>(metrics.size());

    SubmitInfo info;

    if (pattern == SubmitPattern::STEADY) {
        for (int i = 0; i < task_count; i++) {
            phases[i] = PHASE_ALL;
            pool.submit(function, arg, POOL_WAIT, &metrics[i]);
            usleep(1000); // 1ms 간격
        }
    }
    else if (pattern == SubmitPattern::SPIKE) {
        /*
            SPIKE 정의

            task_count = 10000 기준:
            1000개 정상 부하
            8000개 단일 대규모 폭주
            1000개 정상 부하
        */

        int normal_1 = task_count * 1 / 10;
        int spike = task_count * 8 / 10;
        int normal_2 = task_count - normal_1 - spike;

        int idx = 0;

        // 1구간: 정상 부하
        for (int i = 0; i < normal_1; i++, idx++) {
            phases[idx] = PHASE_NORMAL_1;
            pool.submit(function, arg, POOL_WAIT, &metrics[idx]);
            usleep(2000); // 2ms 간격
        }

        // 2구간: 단일 대규모 spike
        info.overload_start_ns = now_ns();

        for (int i = 0; i < spike; i++, idx++) {
            phases[idx] = PHASE_SPIKE;
            pool.submit(function, arg, POOL_WAIT, &metrics[idx]);
            // sleep 없음
        }

        info.overload_end_ns = now_ns();

        // 3구간: 정상 부하
        for (int i = 0; i < normal_2; i++, idx++) {
            phases[idx] = PHASE_NORMAL_2;
            pool.submit(function, arg, POOL_WAIT, &metrics[idx]);
            usleep(2000); // 2ms 간격
        }
    }
    else if (pattern == SubmitPattern::BURST) {
        /*
            BURST 정의

            task_count = 10000 기준:
            1000개 정상 부하
            4000개 burst
            1000개 정상 부하
            4000개 burst
        */

        int normal_1 = task_count * 1 / 10;
        int burst_1 = task_count * 4 / 10;
        int normal_2 = task_count * 1 / 10;
        int burst_2 = task_count - normal_1 - burst_1 - normal_2;

        int idx = 0;

        // 1구간: 정상 부하
        for (int i = 0; i < normal_1; i++, idx++) {
            phases[idx] = PHASE_NORMAL_1;
            pool.submit(function, arg, POOL_WAIT, &metrics[idx]);
            usleep(2000); // 2ms 간격
        }

        // 2구간: 첫 번째 burst
        info.overload_start_ns = now_ns();

        for (int i = 0; i < burst_1; i++, idx++) {
            phases[idx] = PHASE_BURST_1;
            pool.submit(function, arg, POOL_WAIT, &metrics[idx]);
            // sleep 없음
        }

        // 3구간: 정상 부하
        for (int i = 0; i < normal_2; i++, idx++) {
            phases[idx] = PHASE_NORMAL_2;
            pool.submit(function, arg, POOL_WAIT, &metrics[idx]);
            usleep(2000); // 2ms 간격
        }

        // 4구간: 두 번째 burst
        for (int i = 0; i < burst_2; i++, idx++) {
            phases[idx] = PHASE_BURST_2;
            pool.submit(function, arg, POOL_WAIT, &metrics[idx]);
            // sleep 없음
        }

        info.overload_end_ns = now_ns();
    }

    return info;
}

void run_experiment(
    int thread_count,
    const string& workload_name,
    const string& pattern_name,
    int task_count,
    int warmup_count,
    const string& csv_file
) {
    WorkloadType workload = parse_workload(workload_name);
    SubmitPattern pattern = parse_pattern(pattern_name);

    ThreadPool pool(thread_count, task_count);

    WorkloadArg arg;
    arg.type = workload;

    // ============================
    // 1. Warm-up
    // ============================

    for (int i = 0; i < warmup_count; i++) {
        pool.submit(workload_function, &arg, POOL_WAIT, nullptr);
    }

    pool.wait_all();

    // warm-up 결과 제외
    pool.reset_counter();

    // ============================
    // 2. 본 실험
    // ============================

    vector<TaskMetric> metrics(task_count);
    vector<int> phases(task_count, PHASE_ALL);

    long long experiment_start_ns = now_ns();

    submit_tasks_by_pattern(
        pool,
        pattern,
        workload_function,
        &arg,
        metrics,
        phases
    );

    pool.wait_all();

    long long experiment_end_ns = now_ns();

    // ============================
    // 3. 전체 Metric 계산
    // ============================

    vector<long long> latencies;
    vector<long long> queue_waits;
    vector<long long> service_times;

    collect_metric_by_phases(
        metrics,
        phases,
        { PHASE_ALL, PHASE_NORMAL_1, PHASE_SPIKE, PHASE_NORMAL_2, PHASE_BURST_1, PHASE_BURST_2 },
        latencies,
        queue_waits,
        service_times
    );

    double total_time_ms = (experiment_end_ns - experiment_start_ns) / 1000000.0;
    double total_time_sec = total_time_ms / 1000.0;
    double throughput = task_count / total_time_sec;

    double avg_latency_ms = average_ms(latencies);
    double p95_latency_ms = percentile_ms(latencies, 95.0);
    double max_latency_ms = max_ms(latencies);

    double avg_queue_wait_ms = average_ms(queue_waits);
    double p95_queue_wait_ms = percentile_ms(queue_waits, 95.0);
    double max_queue_wait_ms = max_ms(queue_waits);

    double avg_service_time_ms = average_ms(service_times);

    // ============================
    // 4. 폭주 구간 Metric 계산
    // ============================

    vector<int> overload_phases;

    if (pattern == SubmitPattern::SPIKE) {
        overload_phases = { PHASE_SPIKE };
    }
    else if (pattern == SubmitPattern::BURST) {
        overload_phases = { PHASE_BURST_1, PHASE_BURST_2 };
    }
    else {
        overload_phases = { PHASE_ALL };
    }

    vector<long long> overload_latencies;
    vector<long long> overload_queue_waits;
    vector<long long> overload_service_times;

    collect_metric_by_phases(
        metrics,
        phases,
        overload_phases,
        overload_latencies,
        overload_queue_waits,
        overload_service_times
    );

    double overload_avg_latency_ms = average_ms(overload_latencies);
    double overload_p95_latency_ms = percentile_ms(overload_latencies, 95.0);

    double overload_avg_queue_wait_ms = average_ms(overload_queue_waits);
    double overload_p95_queue_wait_ms = percentile_ms(overload_queue_waits, 95.0);

    // ============================
    // 5. 폭주 이후 정상 구간 Metric 계산
    // ============================

    vector<long long> normal_after_latencies;
    vector<long long> normal_after_queue_waits;
    vector<long long> normal_after_service_times;

    collect_metric_by_phases(
        metrics,
        phases,
        { PHASE_NORMAL_2 },
        normal_after_latencies,
        normal_after_queue_waits,
        normal_after_service_times
    );

    double normal_after_avg_latency_ms = average_ms(normal_after_latencies);
    double normal_after_p95_latency_ms = percentile_ms(normal_after_latencies, 95.0);

    // ============================
    // 6. CSV 저장
    // ============================

    write_csv_header_if_needed(csv_file);

    ofstream fout(csv_file, ios::app);

    fout << "fixed,"
         << thread_count << ","
         << workload_name << ","
         << pattern_name << ","
         << task_count << ","
         << warmup_count << ","
         << total_time_ms << ","
         << throughput << ","
         << avg_latency_ms << ","
         << p95_latency_ms << ","
         << max_latency_ms << ","
         << avg_queue_wait_ms << ","
         << p95_queue_wait_ms << ","
         << max_queue_wait_ms << ","
         << avg_service_time_ms << ","
         << overload_avg_latency_ms << ","
         << overload_p95_latency_ms << ","
         << overload_avg_queue_wait_ms << ","
         << overload_p95_queue_wait_ms << ","
         << normal_after_avg_latency_ms << ","
         << normal_after_p95_latency_ms
         << "\n";

    fout.close();

    // ============================
    // 7. 콘솔 출력
    // ============================

    cout << "=============================\n";
    cout << "Pool Type                    : Fixed\n";
    cout << "Thread Count                 : " << thread_count << "\n";
    cout << "Workload                     : " << workload_name << "\n";
    cout << "Pattern                      : " << pattern_name << "\n";
    cout << "Task Count                   : " << task_count << "\n";
    cout << "Warm-up Count                : " << warmup_count << "\n";
    cout << "Total Time(ms)               : " << total_time_ms << "\n";
    cout << "Throughput                   : " << throughput << " tasks/sec\n";
    cout << "Avg Latency(ms)              : " << avg_latency_ms << "\n";
    cout << "P95 Latency(ms)              : " << p95_latency_ms << "\n";
    cout << "Max Latency(ms)              : " << max_latency_ms << "\n";
    cout << "Avg Queue Wait(ms)           : " << avg_queue_wait_ms << "\n";
    cout << "P95 Queue Wait(ms)           : " << p95_queue_wait_ms << "\n";
    cout << "Max Queue Wait(ms)           : " << max_queue_wait_ms << "\n";
    cout << "Avg Service Time(ms)         : " << avg_service_time_ms << "\n";
    cout << "Overload Avg Latency(ms)     : " << overload_avg_latency_ms << "\n";
    cout << "Overload P95 Latency(ms)     : " << overload_p95_latency_ms << "\n";
    cout << "Overload Avg Queue Wait(ms)  : " << overload_avg_queue_wait_ms << "\n";
    cout << "Overload P95 Queue Wait(ms)  : " << overload_p95_queue_wait_ms << "\n";
    cout << "Normal After Avg Latency(ms) : " << normal_after_avg_latency_ms << "\n";
    cout << "Normal After P95 Latency(ms) : " << normal_after_p95_latency_ms << "\n";
    cout << "=============================\n";
}

int main(int argc, char* argv[]) {
    if (argc < 7) {
        cerr << "Usage:\n";
        cerr << "./pool_runner <thread_count> <workload> <pattern> <task_count> <warmup_count> <csv_file>\n\n";

        cerr << "Examples:\n";
        cerr << "./pool_runner 4 cpu spike 10000 1000 result.csv\n";
        cerr << "./pool_runner 8 io burst 10000 1000 result.csv\n";
        cerr << "./pool_runner 16 mixed steady 10000 1000 result.csv\n";

        return 1;
    }

    int thread_count = stoi(argv[1]);
    string workload_name = argv[2];
    string pattern_name = argv[3];
    int task_count = stoi(argv[4]);
    int warmup_count = stoi(argv[5]);
    string csv_file = argv[6];

    run_experiment(
        thread_count,
        workload_name,
        pattern_name,
        task_count,
        warmup_count,
        csv_file
    );

    return 0;
}