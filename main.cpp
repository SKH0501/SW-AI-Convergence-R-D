#include "ThreadPool.h"

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <atomic>
#include <unistd.h>

using namespace std;

enum class WorkloadType {
    CPU,
    IO,
    MIXED
};

enum class SubmitPattern {
    BURST,
    STEADY,
    SPIKE
};

enum TaskPhase {
    PHASE_ALL = 0,
    PHASE_NORMAL_BEFORE = 1,
    PHASE_SPIKE = 2,
    PHASE_NORMAL_AFTER = 3
};

struct WorkloadArg {
    WorkloadType type;
};

struct QueueSample {
    long long time_ns;
    int queue_length;
};

struct SubmitInfo {
    long long spike_start_ns = 0;
    long long spike_end_ns = 0;
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
        usleep(10000);
    }
    else if (warg->type == WorkloadType::MIXED) {
        volatile long long sum = 0;

        for (long long i = 0; i < 300000; i++) {
            sum += i % 7;
        }

        usleep(5000);
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
    if (name == "burst") {
        return SubmitPattern::BURST;
    }

    if (name == "steady") {
        return SubmitPattern::STEADY;
    }

    if (name == "spike") {
        return SubmitPattern::SPIKE;
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
         << "spike_avg_latency_ms,"
         << "spike_p95_latency_ms,"
         << "spike_avg_queue_wait_ms,"
         << "spike_p95_queue_wait_ms,"
         << "normal_after_avg_latency_ms,"
         << "normal_after_p95_latency_ms,"
         << "max_queue_length,"
         << "recovery_time_ms"
         << "\n";
}

void sample_queue_length(
    ThreadPool& pool,
    atomic<bool>& running,
    vector<QueueSample>& samples
) {
    while (running.load()) {
        QueueSample sample;
        sample.time_ns = now_ns();
        sample.queue_length = pool.get_queue_length();

        samples.push_back(sample);

        usleep(1000); // 1ms마다 queue 길이 샘플링
    }

    QueueSample sample;
    sample.time_ns = now_ns();
    sample.queue_length = pool.get_queue_length();
    samples.push_back(sample);
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

    if (pattern == SubmitPattern::BURST) {
        for (int i = 0; i < task_count; i++) {
            phases[i] = PHASE_ALL;
            pool.submit(function, arg, POOL_WAIT, &metrics[i]);
        }
    }
    else if (pattern == SubmitPattern::STEADY) {
        for (int i = 0; i < task_count; i++) {
            phases[i] = PHASE_ALL;
            pool.submit(function, arg, POOL_WAIT, &metrics[i]);
            usleep(1000); // 1ms 간격으로 submit
        }
    }
    else if (pattern == SubmitPattern::SPIKE) {
        int normal_before = task_count * 2 / 10;
        int spike = task_count * 6 / 10;
        int normal_after = task_count - normal_before - spike;

        int idx = 0;

        // 1구간: 정상 부하
        for (int i = 0; i < normal_before; i++, idx++) {
            phases[idx] = PHASE_NORMAL_BEFORE;
            pool.submit(function, arg, POOL_WAIT, &metrics[idx]);
            usleep(5000); // 5ms 간격
        }

        // 2구간: spike 부하
        info.spike_start_ns = now_ns();

        for (int i = 0; i < spike; i++, idx++) {
            phases[idx] = PHASE_SPIKE;
            pool.submit(function, arg, POOL_WAIT, &metrics[idx]);
            // sleep 없음: 순간적으로 몰아넣음
        }

        info.spike_end_ns = now_ns();

        // 3구간: 다시 정상 부하
        for (int i = 0; i < normal_after; i++, idx++) {
            phases[idx] = PHASE_NORMAL_AFTER;
            pool.submit(function, arg, POOL_WAIT, &metrics[idx]);
            usleep(5000); // 5ms 간격
        }
    }

    return info;
}

void collect_metric_by_phase(
    const vector<TaskMetric>& metrics,
    const vector<int>& phases,
    int target_phase,
    vector<long long>& latencies,
    vector<long long>& queue_waits,
    vector<long long>& service_times
) {
    int task_count = static_cast<int>(metrics.size());

    for (int i = 0; i < task_count; i++) {
        if (target_phase != PHASE_ALL && phases[i] != target_phase) {
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

int calculate_max_queue_length(const vector<QueueSample>& samples) {
    int max_queue = 0;

    for (const QueueSample& sample : samples) {
        max_queue = max(max_queue, sample.queue_length);
    }

    return max_queue;
}

double calculate_recovery_time_ms(
    const vector<QueueSample>& samples,
    long long spike_end_ns
) {
    if (spike_end_ns == 0) {
        return 0.0;
    }

    for (const QueueSample& sample : samples) {
        if (sample.time_ns >= spike_end_ns && sample.queue_length == 0) {
            return (sample.time_ns - spike_end_ns) / 1000000.0;
        }
    }

    return -1.0;
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
    // 2. 본 실험 준비
    // ============================

    vector<TaskMetric> metrics(task_count);
    vector<int> phases(task_count, PHASE_ALL);

    vector<QueueSample> queue_samples;
    queue_samples.reserve(100000);

    atomic<bool> sampling_running(true);

    thread sampler(
        sample_queue_length,
        ref(pool),
        ref(sampling_running),
        ref(queue_samples)
    );

    long long experiment_start_ns = now_ns();

    SubmitInfo submit_info = submit_tasks_by_pattern(
        pool,
        pattern,
        workload_function,
        &arg,
        metrics,
        phases
    );

    pool.wait_all();

    long long experiment_end_ns = now_ns();

    sampling_running = false;
    sampler.join();

    // ============================
    // 3. 전체 Metric 계산
    // ============================

    vector<long long> latencies;
    vector<long long> queue_waits;
    vector<long long> service_times;

    collect_metric_by_phase(
        metrics,
        phases,
        PHASE_ALL,
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
    // 4. Spike 구간 Metric 계산
    // ============================

    vector<long long> spike_latencies;
    vector<long long> spike_queue_waits;
    vector<long long> spike_service_times;

    collect_metric_by_phase(
        metrics,
        phases,
        PHASE_SPIKE,
        spike_latencies,
        spike_queue_waits,
        spike_service_times
    );

    double spike_avg_latency_ms = average_ms(spike_latencies);
    double spike_p95_latency_ms = percentile_ms(spike_latencies, 95.0);

    double spike_avg_queue_wait_ms = average_ms(spike_queue_waits);
    double spike_p95_queue_wait_ms = percentile_ms(spike_queue_waits, 95.0);

    // ============================
    // 5. Spike 이후 정상 구간 Metric 계산
    // ============================

    vector<long long> normal_after_latencies;
    vector<long long> normal_after_queue_waits;
    vector<long long> normal_after_service_times;

    collect_metric_by_phase(
        metrics,
        phases,
        PHASE_NORMAL_AFTER,
        normal_after_latencies,
        normal_after_queue_waits,
        normal_after_service_times
    );

    double normal_after_avg_latency_ms = average_ms(normal_after_latencies);
    double normal_after_p95_latency_ms = percentile_ms(normal_after_latencies, 95.0);

    // ============================
    // 6. Queue 관련 Metric
    // ============================

    int max_queue_length = calculate_max_queue_length(queue_samples);

    double recovery_time_ms = calculate_recovery_time_ms(
        queue_samples,
        submit_info.spike_end_ns
    );

    // ============================
    // 7. CSV 저장
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
         << spike_avg_latency_ms << ","
         << spike_p95_latency_ms << ","
         << spike_avg_queue_wait_ms << ","
         << spike_p95_queue_wait_ms << ","
         << normal_after_avg_latency_ms << ","
         << normal_after_p95_latency_ms << ","
         << max_queue_length << ","
         << recovery_time_ms
         << "\n";

    fout.close();

    // ============================
    // 8. 콘솔 출력
    // ============================

    cout << "=============================\n";
    cout << "Pool Type                 : Fixed\n";
    cout << "Thread Count              : " << thread_count << "\n";
    cout << "Workload                  : " << workload_name << "\n";
    cout << "Pattern                   : " << pattern_name << "\n";
    cout << "Task Count                : " << task_count << "\n";
    cout << "Warm-up Count             : " << warmup_count << "\n";
    cout << "Total Time(ms)            : " << total_time_ms << "\n";
    cout << "Throughput                : " << throughput << " tasks/sec\n";
    cout << "Avg Latency(ms)           : " << avg_latency_ms << "\n";
    cout << "P95 Latency(ms)           : " << p95_latency_ms << "\n";
    cout << "Max Latency(ms)           : " << max_latency_ms << "\n";
    cout << "Avg Queue Wait(ms)        : " << avg_queue_wait_ms << "\n";
    cout << "P95 Queue Wait(ms)        : " << p95_queue_wait_ms << "\n";
    cout << "Max Queue Wait(ms)        : " << max_queue_wait_ms << "\n";
    cout << "Avg Service Time(ms)      : " << avg_service_time_ms << "\n";
    cout << "Spike Avg Latency(ms)     : " << spike_avg_latency_ms << "\n";
    cout << "Spike P95 Latency(ms)     : " << spike_p95_latency_ms << "\n";
    cout << "Spike Avg Queue Wait(ms)  : " << spike_avg_queue_wait_ms << "\n";
    cout << "Spike P95 Queue Wait(ms)  : " << spike_p95_queue_wait_ms << "\n";
    cout << "Normal After Avg Lat(ms)  : " << normal_after_avg_latency_ms << "\n";
    cout << "Normal After P95 Lat(ms)  : " << normal_after_p95_latency_ms << "\n";
    cout << "Max Queue Length          : " << max_queue_length << "\n";
    cout << "Recovery Time(ms)         : " << recovery_time_ms << "\n";
    cout << "=============================\n";
}

int main(int argc, char* argv[]) {
    if (argc < 7) {
        cerr << "Usage:\n";
        cerr << "./pool_runner <thread_count> <workload> <pattern> <task_count> <warmup_count> <csv_file>\n\n";

        cerr << "Examples:\n";
        cerr << "./pool_runner 4 cpu burst 10000 1000 result.csv\n";
        cerr << "./pool_runner 8 io steady 10000 1000 result.csv\n";
        cerr << "./pool_runner 16 mixed spike 10000 1000 result.csv\n";

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